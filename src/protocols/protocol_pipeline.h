#ifndef PROTOCOL_PIPELINE_H
#define PROTOCOL_PIPELINE_H

#include "types.h"
#include "protocol_detector.h"
#include "mavlink_detector.h"  // For PacketDetectionResult
#include "protocol_factory.h"

// ========== Byte-level Processing Hooks ==========

// Pre-process incoming byte (e.g., SBUS inversion)
static inline bool preprocessProtocolByte(BridgeContext* ctx, uint8_t* data) {
    // TODO: Implement protocol-specific byte transformations
    return true;  // true = continue processing
}

// Notify protocol detector of new byte
static inline void onProtocolByteReceived(BridgeContext* ctx, uint8_t data) {
    // TODO: Update protocol state machine if needed
}

// ========== Buffer Analysis Hooks ==========

// Check if buffer contains valid protocol start
static inline bool isValidPacketStart(BridgeContext* ctx, 
                                     const uint8_t* data, 
                                     size_t len) {
    if (!ctx->protocol.enabled || !ctx->protocol.detector) return false;
    
    // Use detector to check for valid packet start
    bool isValid = ctx->protocol.detector->canDetect(data, len);
    
    // Track consecutive errors for future use
    if (!isValid) {
        ctx->protocol.consecutiveErrors++;
    } else {
        ctx->protocol.consecutiveErrors = 0;
        ctx->protocol.lastValidPacketTime = millis();
    }
    
    return isValid;
}

// Find complete packet boundary
static inline PacketDetectionResult findPacketBoundary(BridgeContext* ctx,
                                    const uint8_t* data,
                                    size_t len) {
    if (!ctx->protocol.enabled || !ctx->protocol.detector) return PacketDetectionResult(0, 0);
    
    // FIXED: Use real detector instead of TODO stub!
    return ctx->protocol.detector->findPacketBoundary(data, len);
}

// ========== Timing Checks ==========

// Check protocol-specific timing constraints
static inline bool checkProtocolTiming(BridgeContext* ctx,
                                      unsigned long timeSinceLastByte,
                                      unsigned long timeInBuffer) {
    // Check if packet is taking too long to complete
    if (ctx->protocol.packetInProgress && timeInBuffer > 30000) {  // 30ms timeout
        // Force transmission to prevent excessive buffering
        return true;
    }
    return false;
}

// Check for protocol timeouts
static inline void checkProtocolTimeouts(BridgeContext* ctx) {
    // TODO: Reset hung protocol states
}

// ========== Transmission Hooks ==========

// Get optimal transmission size based on protocol
static inline size_t getProtocolTransmitSize(BridgeContext* ctx, size_t currentSize) {
    // TODO: Return detected packet size if available
    if (ctx->protocol.detectedPacketSize > 0) {
        return ctx->protocol.detectedPacketSize;
    }
    return currentSize;
}

// Pre-process data before transmission
static inline bool preprocessProtocolTransmit(BridgeContext* ctx, 
                                             uint8_t* buffer, 
                                             size_t* size) {
    // TODO: Add framing, byte stuffing, etc.
    return true;  // false = delay transmission
}

// ========== State Management ==========

// Reset protocol detection state
static inline void resetProtocolState(BridgeContext* ctx) {
    ctx->protocol.detectedPacketSize = 0;
    ctx->protocol.packetInProgress = false;
    ctx->protocol.packetStartTime = 0;
    ctx->protocol.statsUpdated = false;
    ctx->protocol.packetFound = false;
    ctx->protocol.currentPacketStart = 0;
    // Don't reset lastAnalyzedOffset here - it's managed by buffer operations
    // Note: Don't reset error counters here - they track longer-term behavior
}

// Post-transmission cleanup
static inline void onProtocolPacketTransmitted(BridgeContext* ctx, size_t bytesSent) {
    if (ctx->protocol.stats && bytesSent > 0) {
        // If we detected packets, count them as transmitted
        if (ctx->protocol.detectedPacketSize > 0) {
            // Calculate how many complete packets were sent
            size_t packetsInBuffer = bytesSent / ctx->protocol.detectedPacketSize;
            if (packetsInBuffer > 0) {
                ctx->protocol.stats->packetsTransmitted += packetsInBuffer;
            }
            // Update total bytes only for actual transmitted data
            ctx->protocol.stats->totalBytes += bytesSent;
        }
    }
    resetProtocolState(ctx);
}

// Update protocol statistics
static inline void updateProtocolStats(BridgeContext* ctx, bool success) {
    if (!ctx->protocol.stats) return;
    
    if (success) {
        // Packet successfully detected - stats will be updated by detector
        // The detector calls onPacketDetected() which handles size and timing
        ctx->protocol.stats->updatePacketRate(millis());
    } else {
        // Detection error or timeout
        ctx->protocol.stats->onDetectionError();
    }
}

// Reset protocol statistics
static inline void resetProtocolStats(BridgeContext* ctx) {
    if (!ctx->protocol.stats) return;
    
    ctx->protocol.stats->reset();
    log_msg(LOG_INFO, "Protocol statistics reset");
}

// Update protocol state machine
static inline void updateProtocolState(BridgeContext* ctx) {
    // TODO: Periodic protocol maintenance
}

// ========== Initialization ==========

// Initialize protocol detection subsystem
static inline void initProtocolDetection(BridgeContext* ctx, Config* config) {
    ctx->protocol.enabled = (config->protocolOptimization != PROTOCOL_NONE);
    ctx->protocol.detectedPacketSize = 0;
    ctx->protocol.packetInProgress = false;
    ctx->protocol.packetStartTime = 0;
    
    // Initialize error tracking
    ctx->protocol.consecutiveErrors = 0;
    ctx->protocol.lastValidPacketTime = 0;
    ctx->protocol.temporarilyDisabled = false;  // Reserved for future use
    
    // Create detector and stats using factory (was TODO)
    if (ctx->protocol.enabled) {
        initProtocolDetectionFactory(ctx, (ProtocolType)config->protocolOptimization);
    } else {
        ctx->protocol.detector = nullptr;
        ctx->protocol.stats = nullptr;
    }
}

// Configure hardware for specific protocol
static inline void configureHardwareForProtocol(BridgeContext* ctx, uint8_t protocol) {
    // TODO: Optimize UART settings, DMA timeout, etc.
}

// ========== Main Decision Functions ==========

// Check if buffer is critically full
static inline bool isBufferCritical(BridgeContext* ctx) {
    return (*ctx->adaptive.bufferIndex >= ctx->adaptive.bufferSize - 64);
}

// Detect MAVFtp mode based on message ID
static inline void detectMavftpMode(BridgeContext* ctx, uint32_t currentTime,
                                   const uint8_t* packetData, size_t packetSize) {
    if (!ctx->protocol.enabled || !packetData || packetSize < 10) {
        return;
    }
    
    // Check message ID from the actual packet data, not from buffer start
    uint8_t msgId = 0;
    if (packetData[0] == 0xFE && packetSize >= 6) {  // MAVLink v1
        msgId = packetData[5];
    } else if (packetData[0] == 0xFD && packetSize >= 10) {  // MAVLink v2
        msgId = packetData[7];
    } else {
        return;  // Not a valid MAVLink packet
    }
    
    // Rest of the function remains the same...
    if (msgId == 110) {  // FILE_TRANSFER_PROTOCOL
        ctx->protocol.mavftpCount++;
        ctx->protocol.lastMavftpTime = currentTime;
        
        // Activate special mode after 3 consecutive MAVFtp packets
        if (ctx->protocol.mavftpCount >= 3 && !ctx->protocol.mavftpActive) {
            ctx->protocol.mavftpActive = true;
            log_msg(LOG_DEBUG, "MAVFtp mode activated");
        }
    } else if (msgId == 0 || msgId == 30 || msgId == 24) {
        // HEARTBEAT, ATTITUDE, GPS_RAW_INT - critical packets
        // Don't reset counter, but don't increment either
    } else {
        // Other packets - slowly decay the counter
        if (ctx->protocol.mavftpCount > 0) {
            ctx->protocol.mavftpCount--;
        }
    }
    
    // Deactivate after 500ms without MAVFtp
    if (ctx->protocol.mavftpActive && 
        (currentTime - ctx->protocol.lastMavftpTime) > 500000) {
        ctx->protocol.mavftpActive = false;
        ctx->protocol.mavftpCount = 0;
        log_msg(LOG_DEBUG, "MAVFtp mode deactivated");
    }
}

// Check if packet is critical and should be sent immediately
static inline bool isCriticalPacket(const uint8_t* data, size_t len) {
    if (len < 6) return false;
    
    uint8_t msgId = 0;
    if (data[0] == 0xFE && len >= 6) {  // MAVLink v1
        msgId = data[5];
    } else if (data[0] == 0xFD && len >= 10) {  // MAVLink v2
        msgId = data[7];
    }
    
    // Critical messages that should be transmitted immediately
    switch (msgId) {
        case 0:   // HEARTBEAT
        case 1:   // SYS_STATUS  
        case 24:  // GPS_RAW_INT
        case 30:  // ATTITUDE
        case 33:  // GLOBAL_POSITION_INT
        case 74:  // VFR_HUD
        case 253: // STATUSTEXT
            return true;
        default:
            return false;
    }
}

// Main protocol detection logic - analyzes buffer incrementally
static inline bool checkProtocolPacket(BridgeContext* ctx,
                                      unsigned long currentTime,
                                      unsigned long timeSinceLastByte) {
    if (!ctx->protocol.enabled || !ctx->protocol.detector) return false;
    
    // If we already found a packet, return true to transmit it
    if (ctx->protocol.packetFound) {
        return true;
    }
    
    // Check for timeout on incomplete packet detection
    if (ctx->protocol.packetInProgress) {
        uint32_t timeInProgress = currentTime - ctx->protocol.packetStartTime;
        uint32_t timeout = 50000;  // 50ms default
        if (ctx->system.config && ctx->system.config->baudrate <= 115200) {
            timeout = 100000;  // 100ms for slow speeds
        }
        
        if (timeInProgress > timeout) {
            // Timeout - reset detection state
            log_msg(LOG_DEBUG, "Protocol: Detection timeout after %lu us (%lu ms)", 
                    timeInProgress, timeInProgress/1000);
            ctx->protocol.packetInProgress = false;
            ctx->protocol.lastAnalyzedOffset = 0;  // Reset to reanalyze buffer
            if (ctx->protocol.stats) {
                ctx->protocol.stats->onDetectionError();
            }
            return true;  // Force transmission
        }
    }
    
    // Analyze buffer starting from last analyzed position
    size_t bufferSize = *ctx->adaptive.bufferIndex;
    
    // Safety check: ensure analyzed offset is within valid range
    // This should never happen with correct offset management, but protect against bugs
    if (ctx->protocol.lastAnalyzedOffset >= bufferSize) {
        // Log this unusual situation for debugging
        static uint32_t offsetFixCount = 0;
        offsetFixCount++;
        if (offsetFixCount % 10 == 1) {  // Log first and every 10th occurrence
            log_msg(LOG_WARNING, "Protocol: Fixed invalid analyzed offset %zu (bufSize=%zu), count=%u",
                    ctx->protocol.lastAnalyzedOffset, bufferSize, offsetFixCount);
        }
    
        // Don't reset to 0 - adjust to valid position at buffer end
        ctx->protocol.lastAnalyzedOffset = bufferSize;
    }
    
    // CRITICAL FIX: Track how many bytes we've skipped to prevent infinite loops
    size_t totalSkipped = 0;
    const size_t MAX_SKIP_PER_ITERATION = 100;  // Prevent runaway skipping
    
    // Search for packets in the unanalyzed portion of buffer
    while (ctx->protocol.lastAnalyzedOffset < bufferSize && totalSkipped < MAX_SKIP_PER_ITERATION) {
        size_t searchStart = ctx->protocol.lastAnalyzedOffset;
        
        // Validate search start is within buffer
        if (searchStart >= bufferSize) {
            ctx->protocol.lastAnalyzedOffset = bufferSize;
            break;
        }
        
        // Additional safety check
        if (searchStart > ctx->adaptive.bufferSize) {
            log_msg(LOG_ERROR, "Invalid search offset: %zu > buffer size %zu", 
                    searchStart, ctx->adaptive.bufferSize);
            ctx->protocol.lastAnalyzedOffset = 0;
            break;
        }
        
        size_t remainingBytes = bufferSize - searchStart;
        
        // Validate pointer before detector call
        uint8_t* searchPtr = ctx->adaptive.buffer + searchStart;
        if (searchPtr < ctx->adaptive.buffer || 
            searchPtr >= ctx->adaptive.buffer + ctx->adaptive.bufferSize) {
            log_msg(LOG_ERROR, "Invalid search pointer at offset %zu", searchStart);
            ctx->protocol.lastAnalyzedOffset++;
            totalSkipped++;
            continue;
        }

        // Check if this position could be a packet start
        if (ctx->protocol.detector->canDetect(searchPtr, remainingBytes)) {
            // Try to find complete packet
            PacketDetectionResult detectResult = ctx->protocol.detector->findPacketBoundary(
                searchPtr, remainingBytes);
            
            // Check for error (skipBytes > remainingBytes would be a bug)
            if (detectResult.skipBytes > remainingBytes) {
                static uint32_t errorCount = 0;
                static uint32_t lastErrorReport = 0;
                errorCount++;
                
                uint32_t now = millis();
                // Log with exponential backoff
                if (errorCount == 1 || (now - lastErrorReport) > (1000 * min(errorCount, (uint32_t)10))) {
                    log_msg(LOG_ERROR, "Detector error #%u: skipBytes=%d > remainingBytes=%zu", 
                            errorCount, detectResult.skipBytes, remainingBytes);
                    lastErrorReport = now;
                }
                
                // Recovery: reset detector and skip one byte
                ctx->protocol.detector->reset();
                ctx->protocol.lastAnalyzedOffset++;
                totalSkipped++;
                continue;
            }
            
            if (detectResult.packetSize > 0) {
                // Complete packet found!
                // Adjust for skipped bytes at the beginning
                ctx->protocol.currentPacketStart = searchStart + detectResult.skipBytes;
                ctx->protocol.detectedPacketSize = detectResult.packetSize;
                ctx->protocol.packetFound = true;
                ctx->protocol.packetInProgress = false;
                
                // Update statistics
                if (ctx->protocol.stats) {
                    ctx->protocol.stats->onPacketDetected(detectResult.packetSize, currentTime);
                    if (detectResult.skipBytes > 0) {
                        // Track skipped bytes in statistics
                        ctx->protocol.stats->totalSkippedBytes += detectResult.skipBytes;
                    }
                }
                
                // Log detection
                static uint32_t detectCount = 0;
                detectCount++;
                if (detectCount % 100 == 0) {  // Log every 100 packets
                    log_msg(LOG_INFO, "Protocol: Detected %u packets, last at offset %zu, size %d (skipped %d)",
                            detectCount, searchStart + detectResult.skipBytes, 
                            detectResult.packetSize, detectResult.skipBytes);
                }
                
                #ifdef DEBUG
                // Debug: detailed detection info
                if (detectCount <= 10 || detectCount % 1000 == 0) {
                    log_msg(LOG_DEBUG, "FIX_PHASE_1: skipBytes=%d, packetSize=%d, offset=%zu", 
                            detectResult.skipBytes, detectResult.packetSize, searchStart);
                }
                #endif
                
                // Detect MAVFtp mode (use correct offset)
                uint8_t* packetPtr = searchPtr + detectResult.skipBytes;
                detectMavftpMode(ctx, currentTime, packetPtr, detectResult.packetSize);
                
                // Check if critical packet (use correct offset)
                bool isCritical = isCriticalPacket(packetPtr, detectResult.packetSize);
                
                // Update analyzed position to after this packet
                ctx->protocol.lastAnalyzedOffset = searchStart + detectResult.skipBytes + detectResult.packetSize;
                
                // Decide whether to transmit immediately
                if (isCritical || ctx->protocol.mavftpActive) {
                    return true;  // Transmit immediately
                }
                
                // For non-critical packets, continue searching for more packets
                // This allows batching multiple small packets
                continue;
                
            } else if (detectResult.packetSize == 0 && remainingBytes > 0) {
                // Need more data for complete packet
                if (!ctx->protocol.packetInProgress) {
                    ctx->protocol.packetInProgress = true;
                    ctx->protocol.packetStartTime = currentTime;
                }
                break;  // Wait for more data
                
            } else if (detectResult.packetSize < 0) {
                // Error or request to skip bytes
                int skipBytes = 1;  // Default skip
                
                // Log resync event
                if (ctx->protocol.stats) {
                    ctx->protocol.stats->onResyncEvent();
                    ctx->protocol.stats->onDetectionError();
                }
                
                // Skip forward 
                ctx->protocol.lastAnalyzedOffset++;
                totalSkipped++;
                
                // Log resync with more detail
                static uint32_t resyncCount = 0;
                resyncCount++;
                if (resyncCount % 10 == 0) {  // Log every 10 resyncs
                    log_msg(LOG_DEBUG, "Protocol: Resync #%u at offset %zu",
                            resyncCount, searchStart);
                }
                
                continue;  // Keep searching
            }
        } else {
            // Not a valid packet start, move forward by 1 byte
            ctx->protocol.lastAnalyzedOffset++;
            totalSkipped++;
        }
    }
    
    // Check if we skipped too many bytes (sign of a problem)
    if (totalSkipped >= MAX_SKIP_PER_ITERATION) {
        log_msg(LOG_WARNING, "Protocol: Skipped %zu bytes in one iteration, may have sync issues", totalSkipped);
        // Force transmission to clear buffer
        return true;
    }
    
    // Check if we found any packets
    if (ctx->protocol.packetFound) {
        return true;
    }
    
    // Check if buffer is getting full
    if (bufferSize > ctx->adaptive.bufferSize * 0.8) {
        log_msg(LOG_DEBUG, "Protocol: Buffer 80% full, forcing transmission");
        return true;
    }
    
    // Periodic state dump for debugging
    static uint32_t lastStateDump = 0;
    if (millis() - lastStateDump > 10000 && ctx->protocol.enabled) {
        log_msg(LOG_DEBUG, "Protocol state: bufSize=%zu, bufIndex=%zu, analyzed=%zu, "
                "pktFound=%d, pktStart=%zu, pktSize=%zu",
                ctx->adaptive.bufferSize, *ctx->adaptive.bufferIndex,
                ctx->protocol.lastAnalyzedOffset, ctx->protocol.packetFound,
                ctx->protocol.currentPacketStart, ctx->protocol.detectedPacketSize);
        lastStateDump = millis();
    }
    
    return false;  // Continue accumulating data
}

// Main transmission decision with protocol awareness
static inline bool shouldTransmitProtocolBuffer(BridgeContext* ctx,
                                               unsigned long currentTime,
                                               unsigned long timeSinceLastByte) {

    
    // Priority order:
    // 1. Safety - prevent buffer overflow
    if (isBufferCritical(ctx)) return true;
    
    // 2. Protocol-specific detection
    if (ctx->protocol.enabled) {
        return checkProtocolPacket(ctx, currentTime, timeSinceLastByte);
    }
    
    return false;
}

#endif // PROTOCOL_PIPELINE_H