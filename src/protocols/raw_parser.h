#ifndef RAW_PARSER_H
#define RAW_PARSER_H

#include "protocol_parser.h"
#include "packet_memory_pool.h"
#include "../logging.h"
#include <Arduino.h>

class RawParser : public ProtocolParser {
private:
    // Timeout constants (from adaptive_buffer.h)
    static constexpr uint32_t TIMEOUT_SMALL_US = 200;
    static constexpr uint32_t TIMEOUT_MEDIUM_US = 1000;
    static constexpr uint32_t TIMEOUT_LARGE_US = 5000;
    static constexpr uint32_t TIMEOUT_EMERGENCY_US = 15000;
    
    // Packet size thresholds
    static constexpr size_t PACKET_SIZE_SMALL = 12;
    static constexpr size_t PACKET_SIZE_MEDIUM = 64;
    static constexpr size_t MAX_RAW_CHUNK = 512;  // CRITICAL: Cap RAW size

    // Memory pool standard sizes for optimal allocation
    // Chunks will be rounded to these sizes to avoid heap usage
    // Pool sizes: 64, 128, 288, 512 bytes
    
    uint32_t lastParseTime;
    uint32_t bufferStartTime;
    PacketMemoryPool* memPool;

public:
    RawParser() : lastParseTime(0), bufferStartTime(0) {
        memPool = PacketMemoryPool::getInstance();
        log_msg(LOG_INFO, "RawParser initialized with memory pool");
    }
    
    ParseResult parse(CircularBuffer* buffer, uint32_t currentTime) override {
        ParseResult result;
        
        size_t available = buffer->available();
        if (available == 0) {
            return result;
        }
        
        // Track buffer timing
        if (bufferStartTime == 0) {
            bufferStartTime = currentTime;
        }
        
        uint32_t timeSinceLastByte = buffer->getTimeSinceLastWriteMicros();
        uint32_t timeInBuffer = currentTime - bufferStartTime;
        
        // Decide if we should transmit based on timeouts
        bool shouldTransmit = false;
        
        // Small critical packets - immediate transmission
        if (available <= PACKET_SIZE_SMALL && 
            timeSinceLastByte >= TIMEOUT_SMALL_US) {
            shouldTransmit = true;
        }
        
        // Medium packets - quick transmission
        else if (available <= PACKET_SIZE_MEDIUM && 
                 timeSinceLastByte >= TIMEOUT_MEDIUM_US) {
            shouldTransmit = true;
        }
        
        // Natural packet boundary detected
        else if (timeSinceLastByte >= TIMEOUT_LARGE_US) {
            shouldTransmit = true;
        }
        
        // Emergency timeout
        else if (timeInBuffer >= TIMEOUT_EMERGENCY_US) {
            shouldTransmit = true;
        }
        
        // Buffer getting full
        else if (available >= buffer->getCapacity() * 0.8) {
            shouldTransmit = true;
        }
        
        if (shouldTransmit) {
            // CRITICAL: Limit chunk size to prevent memory issues
            size_t available_data = min((size_t)available, (size_t)MAX_RAW_CHUNK);

            // Determine allocation size rounded up to pool sizes
            size_t allocSize;
            size_t dataSize;

            if (available_data <= 64) {
                allocSize = available_data;  // Small packets fit in 64B pool
                dataSize = available_data;
            } else if (available_data <= 128) {
                allocSize = 128;  // Always use 128B pool
                dataSize = available_data;
            } else if (available_data <= 288) {
                allocSize = 288;  // Always use 288B pool  
                dataSize = min(available_data, (size_t)288);
            } else {
                allocSize = 512;  // Always use 512B pool
                dataSize = min(available_data, (size_t)512);
            }

            // Create a single "packet" with chunk data
            result.count = 1;
            result.packets = new ParsedPacket[1];

            // Get data from buffer
            auto segments = buffer->getReadSegments();

            // Allocate from pool using rounded size
            size_t actualAllocSize;
            result.packets[0].data = memPool->allocate(allocSize, actualAllocSize);
            
            if (!result.packets[0].data) {
                // Pool exhausted - critical error
                log_msg(LOG_ERROR, "RAW: Failed to allocate %zu bytes", allocSize);
                delete[] result.packets;
                result.packets = nullptr;
                result.count = 0;
                return result;
            }
            
            result.packets[0].size = dataSize;  // Actual data size
            result.packets[0].allocSize = actualAllocSize;  // Allocation size from pool
            result.packets[0].pool = memPool;
            result.packets[0].timestamp = currentTime;
            
            // Copy actual data (not allocation size)
            size_t copied = 0;
            if (segments.first.size > 0) {
                size_t toCopy = min((size_t)segments.first.size, dataSize);
                memcpy(result.packets[0].data, segments.first.data, toCopy);
                copied += toCopy;
            }
            if (copied < dataSize && segments.second.size > 0) {
                size_t toCopy = min((size_t)segments.second.size, (dataSize - copied));
                memcpy(result.packets[0].data + copied, segments.second.data, toCopy);
                copied += toCopy;
            }
            
            // Set hints for raw data
            result.packets[0].hints.canFragment = true;
            result.packets[0].hints.canBatch = true;
            result.packets[0].hints.keepWhole = false;
            
            result.bytesConsumed = dataSize;  // Consume actual data size
            
            // Reset timing
            bufferStartTime = 0;
            
            // Update stats
            if (stats) {
                stats->totalBytes += dataSize;
                stats->packetsTransmitted++;
                stats->updatePacketSize(dataSize);  // This updates min/max/avg automatically
            }
        }
        
        lastParseTime = currentTime;
        return result;
    }
    
    void reset() override {
        lastParseTime = 0;
        bufferStartTime = 0;

        if (stats) {
            stats->reset();  // Use built-in reset method
        }

        log_msg(LOG_DEBUG, "RawParser reset");
    }
    
    const char* getName() const override {
        return "RAW";
    }
    
    size_t getMinimumBytes() const override {
        return 1;  // Raw works with any amount
    }
};

#endif // RAW_PARSER_H