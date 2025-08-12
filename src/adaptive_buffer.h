#ifndef ADAPTIVE_BUFFER_H
#define ADAPTIVE_BUFFER_H

#include "types.h"
#include "circular_buffer.h"  // NEW: CircularBuffer implementation
#include "logging.h"
#include "uart/uart_interface.h"
#include "usb/usb_interface.h"
#include "protocols/protocol_pipeline.h"  // Protocol detection hooks
#include <Arduino.h>
#include "diagnostics.h"

// Note: addToDevice4BridgeTx will be available via bridge_processing.h include chain

// Adaptive buffering timeout constants (in microseconds)
#define ADAPTIVE_BUFFER_TIMEOUT_SMALL_US    200
#define ADAPTIVE_BUFFER_TIMEOUT_MEDIUM_US   1000
#define ADAPTIVE_BUFFER_TIMEOUT_LARGE_US    5000
#define ADAPTIVE_BUFFER_TIMEOUT_EMERGENCY_US 15000

// Packet size thresholds
#define PACKET_SIZE_SMALL    12
#define PACKET_SIZE_MEDIUM   64

// Simple traffic pattern detector based on gaps between bytes
struct SimpleTrafficDetector {
    uint32_t lastByteTime;   // Last byte timestamp in microseconds
    uint32_t fastBytes;      // Count of consecutive fast bytes
    bool burstMode;          // Currently in burst mode
    
    // Statistics for debugging (optional)
    uint32_t totalBursts;
    uint32_t maxBurstSize;
};

// Update traffic mode based on gap between bytes
static inline void updateTrafficMode(SimpleTrafficDetector* td, uint32_t nowMicros) {
    uint32_t gap = nowMicros - td->lastByteTime;
    td->lastByteTime = nowMicros;
    
    if (gap < 1000) {  // < 1ms between bytes = fast data
        td->fastBytes++;
        if (td->fastBytes > 100 && !td->burstMode) {
            td->burstMode = true;
            td->totalBursts++;
        }
        if (td->fastBytes > td->maxBurstSize) {
            td->maxBurstSize = td->fastBytes;
        }
    } else if (gap > 5000) {  // > 5ms = pause, end of burst
        if (td->burstMode && td->fastBytes > 0) {
            td->burstMode = false;
        }
        td->fastBytes = 0;
    }
}

// Calculate adaptive buffer size based on baud rate and protocol
static inline size_t calculateAdaptiveBufferSize(uint32_t baudrate, bool protocolEnabled = false) {
    if (protocolEnabled) {
        // Larger buffers for protocol optimization to prevent fragmentation
        if (baudrate >= 921600) return 2048;
        if (baudrate >= 460800) return 1024;
        if (baudrate >= 230400) return 768;
        if (baudrate >= 115200) return 384;  // Increased from 288 to 384
        return 256;
    }
    
    // Original sizes optimized for minimal FIFO overflow
    if (baudrate >= 921600) return 2048;
    if (baudrate >= 460800) return 1024; 
    if (baudrate >= 230400) return 768;
    if (baudrate >= 115200) return 288;  // Optimal for USB without protocol
    return 256;
}

static inline void initAdaptiveBuffer(BridgeContext* ctx, size_t size) {
    // Create CircularBuffer
    ctx->adaptive.circBuf = new CircularBuffer();
    ctx->adaptive.circBuf->init(size);
    
    // Set buffer size
    ctx->adaptive.bufferSize = size;
    
    if (!ctx->adaptive.bufferStartTime) {
        ctx->adaptive.bufferStartTime = new unsigned long(0);
    }
    if (!ctx->adaptive.lastByteTime) {
        ctx->adaptive.lastByteTime = new unsigned long(0);
    }
    
    *ctx->adaptive.bufferStartTime = micros();
    *ctx->adaptive.lastByteTime = micros();
}

// Calculate adaptive thresholds based on buffer size
struct AdaptiveThresholds {
    // Transmission threshold - when to start sending
    static size_t getTransmitThreshold(size_t bufSize, bool burst) {
        if (burst) {
            // CHANGED: reduced from 256 to 200 for more frequent transmission
            // 200 bytes is optimal for single MAVLink packet
            size_t optimal = min(bufSize / 5, (size_t)200);  // was bufSize/4 and 512
            return optimal;
        } else {
            // Normal mode: keep as is for low latency
            return min(bufSize / 10, (size_t)128);
        }
    }
    
    // Warning level - start considering drops
    static size_t getDropThreshold(size_t bufSize) {
        // CHANGED: raised thresholds by 10% for less aggressive dropping
        if (bufSize <= 512) return bufSize * 85 / 100;   // 85% (was 75%)
        if (bufSize <= 1024) return bufSize * 70 / 100;  // 70% (was 60%)
        return bufSize * 60 / 100;                        // 60% (was 50%)
    }
    
    // Critical level - must drop immediately
    static size_t getEmergencyDrop(size_t bufSize) {
        if (bufSize <= 512) return bufSize * 90 / 100;   // 90% for small
        if (bufSize <= 1024) return bufSize * 75 / 100;  // 75% for medium
        return bufSize * 65 / 100;                        // 65% for large
    }
};

// Check if buffer should be transmitted based on timing and size
static inline bool shouldTransmitBuffer(BridgeContext* ctx, 
                                       unsigned long currentTime,
                                       unsigned long timeSinceLastByte) {
    CircularBuffer* circBuf = ctx->adaptive.circBuf;
    if (!circBuf) return false;
    
    // No data to transmit
    size_t available = circBuf->available();
    if (available == 0) {
        return false;
    }
    
    // NEW: Check protocol-specific conditions first (only if protocol enabled)
    // FIX: Check that protocol is actually enabled before calling
    if (ctx->protocol.enabled && shouldTransmitProtocolBuffer(ctx, currentTime, timeSinceLastByte)) {
        return true;
    }
    
    // Calculate time buffer has been accumulating (unified micros() base!)
    unsigned long timeInBuffer = currentTime - *ctx->adaptive.bufferStartTime;
    
    // Check if DMA detected packet boundary
    if (ctx->interfaces.uartBridgeSerial->hasPacketTimeout()) {
        return true;  // Hardware detected pause
    }
    
    // Transmission decision logic (prioritized for low latency):
    
    // 1. Small critical packets (heartbeat, commands) - immediate transmission
    if (available <= PACKET_SIZE_SMALL && 
        timeSinceLastByte >= ADAPTIVE_BUFFER_TIMEOUT_SMALL_US) {
        return true;
    }
    
    // 2. Medium packets (attitude, GPS) - quick transmission
    if (available <= PACKET_SIZE_MEDIUM && 
        timeSinceLastByte >= ADAPTIVE_BUFFER_TIMEOUT_MEDIUM_US) {
        return true;
    }
    
    // 3. Natural packet boundary detected (works for any size)
    if (timeSinceLastByte >= ADAPTIVE_BUFFER_TIMEOUT_LARGE_US) {
        return true;
    }
    
    // 4. Emergency timeout (prevent excessive latency)
    if (timeInBuffer >= ADAPTIVE_BUFFER_TIMEOUT_EMERGENCY_US) {
        return true;
    }
    
    // 5. Buffer safety limit (prevent overflow)
    if (available >= ctx->adaptive.bufferSize) {
        return true;
    }
    
    return false;
}

// NEW: Scatter-gather transmission with skipBytes support
static inline void transmitAdaptiveBuffer(BridgeContext* ctx, unsigned long currentTime) {
    CircularBuffer* circBuf = ctx->adaptive.circBuf;
    if (!circBuf) return;
    
    // First handle skipBytes from detector (if any)
    if (ctx->protocol.skipBytes > 0) {
        circBuf->consume(ctx->protocol.skipBytes);
        ctx->protocol.skipBytes = 0;
    }
    
    // Get segments for transmission (real data, NOT shadow!)
    auto segments = circBuf->getReadSegments();
    
    if (segments.total() == 0) return;
    
    // Check available space in USB
    int availableSpace = ctx->interfaces.usbInterface->availableForWrite();
    
    // Normalize negative values (driver bug workaround)
    size_t usbSpace = (availableSpace > 0) ? (size_t)availableSpace : 0;
    
    if (usbSpace == 0) {
        // USB buffer full - handle emergency case
        if (circBuf->available() >= ctx->adaptive.bufferSize * 0.9) {
            // Buffer critically full - drop data to prevent system hang
            size_t dropped = circBuf->available();
            circBuf->consume(dropped);
            *ctx->diagnostics.droppedBytes += dropped;
            (*ctx->diagnostics.dropEvents)++;
            
            log_msg(LOG_WARNING, "Emergency: Dropped %zu bytes, USB full", dropped);
        }
        return;
    }
    
    // Calculate how much to send
    size_t toSend = min(usbSpace, segments.total());
    size_t sent = 0;
    
    // Scatter-gather transmission
    if (toSend >= segments.first.size) {
        // Can send entire first segment
        size_t sent1 = ctx->interfaces.usbInterface->write(segments.first.data, segments.first.size);
        sent += sent1;
        
        // If there's space and second segment exists
        if (segments.second.size > 0 && toSend > segments.first.size) {
            size_t toSend2 = min(segments.second.size, toSend - segments.first.size);
            size_t sent2 = ctx->interfaces.usbInterface->write(segments.second.data, toSend2);
            sent += sent2;
        }
    } else {
        // Partial transmission of first segment only
        sent = ctx->interfaces.usbInterface->write(segments.first.data, toSend);
    }
    
    if (sent > 0) {
        *ctx->stats.device2TxBytes += sent;
        
        // Confirm transmission
        circBuf->consume(sent);
        
        // Update protocol state proportionally
        if (ctx->protocol.lastAnalyzedOffset >= sent) {
            ctx->protocol.lastAnalyzedOffset -= sent;
        } else {
            ctx->protocol.lastAnalyzedOffset = 0;
        }
        
        // Update compatibility fields
        
        // Log statistics periodically
        static uint32_t txCount = 0;
        if (++txCount % 1000 == 0) {
            auto stats = circBuf->getStats();
            log_msg(LOG_INFO, "CircBuf TX: %u packets, drops=%u, overflows=%u, maxDepth=%u",
                    txCount, stats->droppedBytes, stats->overflowEvents, stats->maxDepth);
        }
    }
}

// Transmit to all devices with correct synchronization
static inline void transmitToAllDevices(BridgeContext* ctx, unsigned long currentMicros) {
    CircularBuffer* circBuf = ctx->adaptive.circBuf;
    auto segments = circBuf->getReadSegments();
    
    if (segments.total() == 0) return;
    
    // Check readiness of all active devices
    int space2_raw = ctx->interfaces.usbInterface->availableForWrite();
    
    int space3_raw = ctx->devices.device3Active ? 
                     ctx->interfaces.device3Serial->availableForWrite() : 0;
    
    // Normalize negative values (driver bug workaround)
    size_t space2 = (space2_raw > 0) ? (size_t)space2_raw : 0;
    size_t space3 = ctx->devices.device3Active ? 
                    ((space3_raw > 0) ? (size_t)space3_raw : 0) : SIZE_MAX;
    
    // Minimum available space across all devices
    size_t minSpace = min(space2, space3);
    
    if (minSpace == 0) {
        // Handle USB bottleneck with adaptive dropping
        size_t bufSize = ctx->adaptive.bufferSize;
        size_t available = circBuf->available();
        
        size_t emergencyLevel = AdaptiveThresholds::getEmergencyDrop(bufSize);
        size_t warningLevel = AdaptiveThresholds::getDropThreshold(bufSize);
        
        if (available > emergencyLevel) {
            // CRITICAL: Drop 33% immediately
            size_t toDrop = available / 3;
            circBuf->consume(toDrop);
            
            // CRITICAL: Keep this log for production monitoring
            log_msg(LOG_ERROR, "CRITICAL USB bottleneck: dropped %zu of %zu bytes (buf=%zu)",
                    toDrop, available, bufSize);
            // Update drop statistics
            *ctx->diagnostics.droppedBytes += toDrop;
            (*ctx->diagnostics.dropEvents)++;
            
        } else if (available > warningLevel) {
            // WARNING: Consider dropping after several attempts
            static uint32_t warnCount = 0;
            if (++warnCount > 5) {
                size_t toDrop = available / 8;  // Drop 12.5%
                circBuf->consume(toDrop);
                
                // Keep warning for production
                log_msg(LOG_WARNING, "USB bottleneck: dropped %zu bytes after %u retries", toDrop, 5);
                
                warnCount = 0;
            }
        }
        
        return;  // Try again later
    }
    
    size_t toSend = min(minSpace, segments.total());
    
    // Send data and count ACTUALLY sent
    size_t sent2 = 0;  // Device 2
    size_t sent3 = SIZE_MAX;  // Device 3 (if not active - doesn't limit)
    
    // Device 2 - main path
    if (toSend >= segments.first.size) {
        // Full first segment
        sent2 = ctx->interfaces.usbInterface->write(segments.first.data, segments.first.size);
        
        // Second segment if space available
        if (segments.second.size > 0 && toSend > segments.first.size) {
            size_t toSend2 = min(segments.second.size, toSend - segments.first.size);
            sent2 += ctx->interfaces.usbInterface->write(segments.second.data, toSend2);
        }
    } else {
        // Partial first segment
        sent2 = ctx->interfaces.usbInterface->write(segments.first.data, toSend);
    }
    
    // Device 3 - if active (same speed but may write less!)
    if (ctx->devices.device3Active) {
        sent3 = 0;
        
        // Send EXACTLY what Device2 sent
        if (sent2 <= segments.first.size) {
            // Only from first segment
            sent3 = ctx->interfaces.device3Serial->write(segments.first.data, sent2);
        } else {
            // First segment full + part of second
            sent3 = ctx->interfaces.device3Serial->write(segments.first.data, segments.first.size);
            size_t sent3_seg2 = ctx->interfaces.device3Serial->write(
                segments.second.data, sent2 - segments.first.size);
            sent3 += sent3_seg2;
        }
    }
    
    // Device 4 (UDP) - batched send of ACTUALLY transmitted bytes only!
    if (ctx->system.config->device4.role == D4_NETWORK_BRIDGE) {
        size_t actualSent = min(sent2, sent3);  // What really went out
        
        if (actualSent <= segments.first.size) {
            // Only first segment (partially or fully)
            addToDevice4BridgeTx(segments.first.data, actualSent);
        } else {
            // First full + part of second
            addToDevice4BridgeTx(segments.first.data, segments.first.size);
            addToDevice4BridgeTx(segments.second.data, actualSent - segments.first.size);
        }
    }
    
    // Consume MINIMUM of what all devices actually sent
    // CONTRACT: always consume(min(sent_all)), never consume(sent_single)!
    size_t actualConsumed = min(sent2, sent3);  // sent3 = SIZE_MAX if inactive
    circBuf->consume(actualConsumed);
    *ctx->stats.device2TxBytes += sent2;
    
    
    // Update compatibility fields
}

// Handle buffer timeout - force transmission after emergency timeout
static inline void handleBufferTimeout(BridgeContext* ctx) {
    CircularBuffer* circBuf = ctx->adaptive.circBuf;
    if (!circBuf) return;
    
    size_t available = circBuf->available();
    if (available == 0) return;
    
    unsigned long timeInBuffer = micros() - *ctx->adaptive.bufferStartTime;
    
    if (timeInBuffer >= ADAPTIVE_BUFFER_TIMEOUT_EMERGENCY_US) {
        // Emergency timeout - try to transmit
        transmitAdaptiveBuffer(ctx, micros());
        
        // If still full after timeout, drop data
        if (circBuf->available() >= ctx->adaptive.bufferSize * 0.9) {
            size_t dropped = circBuf->available();
            circBuf->consume(dropped);
            *ctx->diagnostics.droppedBytes += dropped;
            (*ctx->diagnostics.dropEvents)++;
            
            log_msg(LOG_WARNING, "Timeout: Dropped %zu bytes", dropped);
            
            // Reset protocol state
            ctx->protocol.lastAnalyzedOffset = 0;
            ctx->protocol.packetFound = false;
            ctx->protocol.currentPacketStart = 0;
        }
    }
}

// Process adaptive buffering for a single byte
static inline void processAdaptiveBufferByte(BridgeContext* ctx, uint8_t data, 
                                            unsigned long currentMicros) {
    CircularBuffer* circBuf = ctx->adaptive.circBuf;
    if (!circBuf) return;
    
    // Write byte to buffer
    size_t written = circBuf->write(&data, 1);
    
    if (written == 0) {
        // Buffer full - data lost
        return;
    }
    
    // Update buffer timing
    if (circBuf->available() == 1) {
        *ctx->adaptive.bufferStartTime = currentMicros;
    }
    *ctx->adaptive.lastByteTime = currentMicros;
    
    // Update traffic detector
    static SimpleTrafficDetector traffic = {0, 0, false, 0, 0};
    updateTrafficMode(&traffic, currentMicros);
    
    // Calculate adaptive thresholds
    size_t bufSize = ctx->adaptive.bufferSize;
    size_t threshold = AdaptiveThresholds::getTransmitThreshold(bufSize, traffic.burstMode);
    
    // Also use time-based threshold
    uint32_t timeoutUs = traffic.burstMode ? 5000 : 2000;  // 5ms or 2ms
    
    // Check transmission conditions
    if (circBuf->available() >= threshold || 
        circBuf->getTimeSinceLastWriteMicros() >= timeoutUs) {
        transmitToAllDevices(ctx, currentMicros);
    }
}

#endif // ADAPTIVE_BUFFER_H