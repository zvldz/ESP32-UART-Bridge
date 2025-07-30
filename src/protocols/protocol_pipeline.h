#ifndef PROTOCOL_PIPELINE_H
#define PROTOCOL_PIPELINE_H

#include "types.h"
#include "protocol_detector.h"

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
    // TODO: Delegate to protocol detector
    if (!ctx->protocol.enabled || !ctx->protocol.detector) return false;
    
    bool isValid = false;
    // Future: isValid = ctx->protocol.detector->canDetect(data, len);
    
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
static inline int findPacketBoundary(BridgeContext* ctx,
                                    const uint8_t* data,
                                    size_t len) {
    // TODO: Delegate to protocol detector
    if (!ctx->protocol.enabled || !ctx->protocol.detector) return 0;
    
    // Future: return ctx->protocol.detector->findPacketBoundary(data, len);
    return 0;
}

// ========== Timing Checks ==========

// Check protocol-specific timing constraints
static inline bool checkProtocolTiming(BridgeContext* ctx,
                                      unsigned long timeSinceLastByte,
                                      unsigned long timeInBuffer) {
    // TODO: Implement timing checks (SBUS gaps, Modbus silence, etc.)
    return false;  // true = transmit due to timing
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
    // Note: Don't reset error counters here - they track longer-term behavior
}

// Post-transmission cleanup
static inline void onProtocolPacketTransmitted(BridgeContext* ctx, size_t bytesSent) {
    // TODO: Update statistics, handle remaining bytes
    resetProtocolState(ctx);
}

// Update protocol statistics
static inline void updateProtocolStats(BridgeContext* ctx, bool success) {
    // TODO: Update protocol statistics when ProtocolStats is implemented
    if (!ctx->protocol.stats) return;
    
    // Future implementation:
    // if (success) {
    //     ctx->protocol.stats->packetsDetected++;
    //     updateProtocolFrameRate(ctx->protocol.stats, millis());
    // } else {
    //     ctx->protocol.stats->errors++;
    // }
}

// Reset protocol statistics
static inline void resetProtocolStats(BridgeContext* ctx) {
    // TODO: Reset protocol statistics when ProtocolStats is implemented
    if (!ctx->protocol.stats) return;
    
    // Future: Clear all counters
}

// Update protocol state machine
static inline void updateProtocolState(BridgeContext* ctx) {
    // TODO: Periodic protocol maintenance
}

// ========== Initialization ==========

// Initialize protocol detection subsystem
static inline void initProtocolDetection(BridgeContext* ctx, Config* config) {
    ctx->protocol.enabled = (config->protocolOptimization != PROTOCOL_NONE);
    ctx->protocol.detector = nullptr;  // TODO: Create detector based on config
    ctx->protocol.detectedPacketSize = 0;
    ctx->protocol.packetInProgress = false;
    ctx->protocol.packetStartTime = 0;
    ctx->protocol.stats = nullptr;  // TODO: Allocate when protocol is selected
    
    // Initialize error tracking
    ctx->protocol.consecutiveErrors = 0;
    ctx->protocol.lastValidPacketTime = 0;
    ctx->protocol.temporarilyDisabled = false;  // Reserved for future use
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

// Main protocol detection logic
static inline bool checkProtocolPacket(BridgeContext* ctx,
                                      unsigned long currentTime,
                                      unsigned long timeSinceLastByte) {
    if (!ctx->protocol.enabled) return false;
    
    // 1. Check timing constraints
    if (checkProtocolTiming(ctx, timeSinceLastByte, 
                           currentTime - *ctx->adaptive.bufferStartTime)) {
        return true;
    }
    
    // 2. Check for valid packet start
    if (!ctx->protocol.packetInProgress) {
        if (!isValidPacketStart(ctx, ctx->adaptive.buffer, 
                               *ctx->adaptive.bufferIndex)) {
            return false;
        }
        ctx->protocol.packetInProgress = true;
        ctx->protocol.packetStartTime = currentTime;
    }
    
    // 3. Try to find packet boundary
    int packetSize = findPacketBoundary(ctx, ctx->adaptive.buffer,
                                       *ctx->adaptive.bufferIndex);
    
    if (packetSize > 0) {
        ctx->protocol.detectedPacketSize = packetSize;
        updateProtocolStats(ctx, true);  // Update success stats
        return true;
    }
    
    // 4. Check for timeout
    if (currentTime - ctx->protocol.packetStartTime > 100000) {  // 100ms
        updateProtocolStats(ctx, false);  // Update error stats
        return true;  // Force transmission
    }
    
    return false;  // Keep waiting
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