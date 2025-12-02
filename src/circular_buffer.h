#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <Arduino.h>
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logging.h"

// Overflow policy for buffer full condition
enum OverflowPolicy {
    DROP_NEW,      // Drop new data when full (SAFE for TX)
    DROP_OLD,      // Overwrite old data (DANGEROUS with active TX!)
    REJECT         // Reject write, return 0 (non-blocking)
};
// IMPORTANT: Use only DROP_NEW during active transmission!
// DROP_OLD can corrupt data being transmitted by TX task

// Buffer statistics
struct CircularBufferStats {
    uint32_t bytesWritten;    // TODO v2: uint64_t for long sessions
    uint32_t bytesRead;       // TODO v2: uint64_t
    uint32_t droppedBytes;
    uint32_t overflowEvents;
    uint32_t wrapCount;
    uint32_t wrapLinearizations;
    uint32_t partialWrites;   // Number of partial write operations
    uint32_t maxDepth;
    // [COMMENT CANDIDATE FOR REMOVAL] p95/p99 latency metrics - over-engineering for this project
};

// Contiguous view for parser (zero-copy access)
struct ContiguousView {
    const uint8_t* ptr;
    size_t safeLen;  // Guaranteed readable length
};

class CircularBuffer {
public:
    
private:
    uint8_t* mainBuffer;      // Main circular buffer
    size_t capacity;          // Main buffer size (power of 2)
    size_t capacityMask;      // Fast wrap mask (capacity - 1)
    size_t head;              // Write position
    size_t tail;              // Read position
    
    // Temporary buffer for linearizing wrapped data in getContiguousForParser()
    // Size: 296 bytes = MAVLink v2 max frame (280) + margin (16)
    // 
    // IMPORTANT: This buffer is used when data wraps around main buffer boundary.
    // If future parsers need larger contiguous views:
    // 1. Increase this buffer size, OR  
    // 2. Use getReadSegments() for manual wrap handling
    uint8_t tempLinearBuffer[296];
    
    // Synchronization
    portMUX_TYPE bufferMux = portMUX_INITIALIZER_UNLOCKED;
    
    // Configuration
    OverflowPolicy overflowPolicy = DROP_NEW;
    
    // Statistics
    CircularBufferStats stats;
    
    // Timing (unified micros() base!)
    uint32_t lastWriteTimeMicros = 0;
    
public:
    // Scatter-gather segments for TX (NOT using shadow!)
    struct Segment {
        const uint8_t* data;
        size_t size;
    };
    
    struct SegmentPair {
        Segment first;
        Segment second;
        size_t total() const { return first.size + second.size; }
    };
    
    // Round to power of 2 for optimization
    static size_t roundToPowerOf2(size_t size) {
        if (size <= 256) return 256;
        if (size <= 512) return 512;
        if (size <= 1024) return 1024;
        if (size <= 2048) return 2048;
        if (size <= 4096) return 4096;
        if (size <= 8192) return 8192;
        return 16384;  // Maximum for high speeds (921600)
    }
    
    // Initialize buffer with DMA-compatible memory
    // useSlowMemory: if true, prefer PSRAM for non-critical buffers (e.g., logs)
    void init(size_t requestedSize, bool useSlowMemory = false) {
        // Check minimum size
        if (requestedSize < 256) requestedSize = 256;

        capacity = roundToPowerOf2(requestedSize);
        capacityMask = capacity - 1;

        // Choose memory type based on usage
        uint32_t caps = MALLOC_CAP_DMA | MALLOC_CAP_8BIT;  // Default for high-speed UART

        if (useSlowMemory) {
            // Check PSRAM availability for non-critical buffers
            size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
            if (psramFree > capacity) {
                caps = MALLOC_CAP_SPIRAM;
                log_msg(LOG_INFO, "CircBuf: Using PSRAM for %zu bytes buffer", capacity);
            } else {
                log_msg(LOG_WARNING, "CircBuf: PSRAM requested but not available (%zu free, %zu needed), using internal RAM",
                        psramFree, capacity);
                // Fall back to internal RAM without DMA (logs don't need DMA)
                caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
            }
        }

        // Allocate memory with chosen capabilities
        mainBuffer = static_cast<uint8_t*>(heap_caps_malloc(capacity, caps));

        if (!mainBuffer && !useSlowMemory) {
            // For high-speed buffers, try fallback options
            log_msg(LOG_ERROR, "CircBuf: Failed to allocate %zu bytes (DMA)", capacity);
            // Fallback to regular memory if DMA unavailable
            mainBuffer = static_cast<uint8_t*>(heap_caps_malloc(capacity, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

            if (!mainBuffer) {
                // Try smaller size
                capacity = roundToPowerOf2(requestedSize / 2);
                capacityMask = capacity - 1;
                mainBuffer = static_cast<uint8_t*>(heap_caps_malloc(capacity, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

                if (!mainBuffer) {
                    // Critical failure
                    esp_restart();
                }
            }
        } else if (!mainBuffer && useSlowMemory) {
            // For slow buffers, try internal RAM as last resort
            mainBuffer = static_cast<uint8_t*>(heap_caps_malloc(capacity, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
            if (!mainBuffer) {
                log_msg(LOG_ERROR, "CircBuf: Failed to allocate %zu bytes for slow buffer", capacity);
                esp_restart();
            }
        }

        head = tail = 0;
        memset(&stats, 0, sizeof(stats));
        lastWriteTimeMicros = micros();  // Unified time base!

        // Log allocation result
        const char* memType = (caps == MALLOC_CAP_SPIRAM) ? "PSRAM" :
                              (caps & MALLOC_CAP_DMA) ? "DMA" : "Internal";
        log_msg(LOG_INFO, "CircBuf: Allocated %zu bytes in %s (tempLinearBuffer: %zu bytes BSS)",
                capacity, memType, sizeof(tempLinearBuffer));
    }
    
    // Free space calculation (leave 1 byte empty for full/empty distinction)
    size_t freeSpace() const {
        // Critical: leave 1 byte empty to distinguish full/empty
        if (head >= tail) {
            return capacity - (head - tail) - 1;
        } else {
            return tail - head - 1;
        }
    }
    
    // Available data for reading
    size_t available() const {
        if (head >= tail) {
            return head - tail;
        } else {
            return capacity - tail + head;
        }
    }
    
    // Get buffer capacity
    size_t getCapacity() const { return capacity; }
    
    // Get free space available for writing
    size_t getFreeSpace() const {
        return capacity - available();
    }
    
    // Reserve-Copy-Commit write pattern for thread safety
    size_t write(const uint8_t* data, size_t len) {
        if (len == 0) return 0;
        
        // PHASE 1: Reserve - reserve space WITHOUT publishing
        size_t writePos;
        size_t toWrite;
        bool wrapped = false;
        
        portENTER_CRITICAL(&bufferMux);
        size_t space = freeSpace();
        toWrite = len;
        
        // Handle overflow
        if (len > space) {
            switch (overflowPolicy) {
                case DROP_NEW: {
                    toWrite = space;
                    stats.droppedBytes += (len - space);
                    stats.overflowEvents++;
                    break;
                }
                    
                case DROP_OLD: {
                    size_t toSkip = len - space;
                    tail = (tail + toSkip) & capacityMask;
                    stats.droppedBytes += toSkip;
                    break;
                }
                    
                case REJECT: {
                    portEXIT_CRITICAL(&bufferMux);
                    return 0;
                }
            }
        }
        
        // Save position WITHOUT changing head (reserve only!)
        writePos = head;
        
        // Detect wrap for statistics
        if (writePos + toWrite > capacity) {
            wrapped = true;
        }
        
        portEXIT_CRITICAL(&bufferMux);
        
        // PHASE 2: Copy - copy data OUTSIDE critical section
        size_t written = 0;
        size_t currentPos = writePos;
        
        while (written < toWrite) {
            size_t chunk = min((size_t)(toWrite - written), (size_t)(capacity - currentPos));
            memcpy(&mainBuffer[currentPos], &data[written], chunk);
            
            currentPos = (currentPos + chunk) & capacityMask;
            written += chunk;
        }
        
        // PHASE 3: Commit - publish changes (SHORT critical section!)
        portENTER_CRITICAL(&bufferMux);
        
        // Now data is copied - can publish
        head = (writePos + toWrite) & capacityMask;
        stats.bytesWritten += toWrite;
        lastWriteTimeMicros = micros();
        
        if (wrapped) {
            stats.wrapCount++;
        }
        
        
        // Update max depth
        size_t currentDepth = available();
        if (currentDepth > stats.maxDepth) {
            stats.maxDepth = currentDepth;
        }
        
        portEXIT_CRITICAL(&bufferMux);
        
        return written;
    }
    
    // Get segments for TX (NOT using shadow!)
    SegmentPair getReadSegments() {
        portENTER_CRITICAL(&bufferMux);
        
        SegmentPair segments;
        size_t avail = available();
        
        if (avail == 0) {
            segments.first = {nullptr, 0};
            segments.second = {nullptr, 0};
        } else if (head > tail) {
            // Data is contiguous
            segments.first = {&mainBuffer[tail], avail};
            segments.second = {nullptr, 0};
        } else {
            // Data is wrapped - two segments
            segments.first = {&mainBuffer[tail], capacity - tail};
            segments.second = {&mainBuffer[0], head};
        }
        
        portEXIT_CRITICAL(&bufferMux);
        
        return segments;
    }
    
    // Contiguous reading ONLY for parser (with automatic linearization)
    // 
    // WARNING: For wrapped data, returns pointer to internal tempLinearBuffer!
    // This means:
    // 1. view.ptr may NOT point to mainBuffer (when data wraps)
    // 2. Limited to 296 bytes max (MAVLink v2 frame size)
    // 3. NOT suitable for DMA operations (use getReadSegments() instead)
    // 
    // Safe for: MAVLink parser (reads and copies data immediately)
    // 
    // If other parsers need >296 bytes OR direct mainBuffer access:
    // - Increase tempLinearBuffer size, OR
    // - Use getReadSegments() for manual segment handling
    ContiguousView getContiguousForParser(size_t needed) {
        portENTER_CRITICAL(&bufferMux);
        
        ContiguousView view{nullptr, 0};
        size_t avail = available();
        
        // Limit request to available data
        if (needed > avail) {
            needed = avail;
        }
        
        // Return empty view if no data
        if (needed == 0) {
            portEXIT_CRITICAL(&bufferMux);
            return view;
        }
        
        // Case 1: Linear data - direct pointer to main buffer
        if (tail + needed <= capacity) {
            view.ptr = &mainBuffer[tail];
            view.safeLen = needed;
            portEXIT_CRITICAL(&bufferMux);
            return view;
        }
        
        // Case 2: Wrapped data - linearize into temp buffer
        // Limit to temp buffer size
        if (needed > sizeof(tempLinearBuffer)) {
            needed = sizeof(tempLinearBuffer);
        }
        
        // Copy wrapped data: [tail..capacity) + [0..remaining)
        size_t firstPart = capacity - tail;
        size_t secondPart = needed - firstPart;
        
        // Copy tail to end of main buffer
        memcpy(tempLinearBuffer, &mainBuffer[tail], firstPart);
        
        // Copy beginning of main buffer if needed
        if (secondPart > 0) {
            memcpy(tempLinearBuffer + firstPart, &mainBuffer[0], secondPart);
        }
        
        // Return pointer to linearized data
        view.ptr = tempLinearBuffer;
        view.safeLen = needed;
        
        // Update statistics
        stats.wrapLinearizations++;  // Count wrap linearization events
        
        portEXIT_CRITICAL(&bufferMux);
        
        // Log wrap events
        static uint32_t wrapLogCount = 0;
        if (++wrapLogCount % 100 == 0) {
            log_msg(LOG_DEBUG, "CircBuf: Wrapped read #%u at tail=%zu, linearized %zu bytes", 
                    wrapLogCount, tail, needed);
        }
        
        return view;
    }
    
    // Confirm transmission with validation
    void consume(size_t bytes) {
        portENTER_CRITICAL(&bufferMux);

        size_t avail = available();
        if (bytes > avail) {
            log_msg(LOG_ERROR, "CircBuf: Trying to consume %zu but only %zu available",
                    bytes, avail);
            bytes = avail;
        }

        tail = (tail + bytes) & capacityMask;
        stats.bytesRead += bytes;

        portEXIT_CRITICAL(&bufferMux);
    }
    
    // Timing functions (UNIFIED micros() base!)
    uint32_t getTimeSinceLastWriteMicros() const {
        return micros() - lastWriteTimeMicros;
    }
    
    uint32_t getLastWriteTimeMicros() const {
        return lastWriteTimeMicros;
    }
    
    // Statistics access
    CircularBufferStats* getStats() { return &stats; }
    
    // Diagnostics
    void logState(const char* context) {
        log_msg(LOG_DEBUG, "CircBuf[%s]: cap=%zu, head=%zu, tail=%zu, avail=%zu, free=%zu, "
                "wrapped=%d, drops=%u, overflows=%u",
                context, capacity, head, tail, available(), freeSpace(),
                (head < tail), stats.droppedBytes, stats.overflowEvents);
    }
    
    // Destructor
    ~CircularBuffer() {
        if (mainBuffer) {
            heap_caps_free(mainBuffer);
        }
    }
};

#endif // CIRCULAR_BUFFER_H