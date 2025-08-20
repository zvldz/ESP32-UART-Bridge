#ifndef ADAPTIVE_BUFFER_H
#define ADAPTIVE_BUFFER_H

#include "types.h"
#include "circular_buffer.h"
#include "logging.h"
#include <Arduino.h>

// Calculate adaptive buffer size based on baud rate
static inline size_t calculateAdaptiveBufferSize(uint32_t baudrate) {
    // TODO: Analyze after MAVLink stabilization - consider minimum 2048 for all speeds
    // Current adaptive sizing may be too small for burst traffic at low baudrates
    
    // Larger buffers for higher speeds to prevent FIFO overflow
    if (baudrate >= 921600) return 2048;
    if (baudrate >= 460800) return 1024;
    if (baudrate >= 230400) return 768;
    if (baudrate >= 115200) return 384;
    return 256;
}

// Initialize adaptive buffer with CircularBuffer
static inline void initAdaptiveBuffer(BridgeContext* ctx, size_t size) {
    // Create CircularBuffer
    ctx->adaptive.circBuf = new CircularBuffer();
    ctx->adaptive.circBuf->init(size);
    
    // Set buffer size
    ctx->adaptive.bufferSize = size;
    
    // Initialize timing (if still needed elsewhere)
    if (!ctx->adaptive.bufferStartTime) {
        ctx->adaptive.bufferStartTime = new unsigned long(0);
    }
    if (!ctx->adaptive.lastByteTime) {
        ctx->adaptive.lastByteTime = new unsigned long(0);
    }
    
    *ctx->adaptive.bufferStartTime = micros();
    *ctx->adaptive.lastByteTime = micros();
    
    log_msg(LOG_INFO, "Adaptive buffer initialized: %zu bytes", size);
}

// Process single byte - just write to buffer
static inline void processAdaptiveBufferByte(BridgeContext* ctx, uint8_t data, 
                                            unsigned long currentMicros) {
    CircularBuffer* circBuf = ctx->adaptive.circBuf;
    if (!circBuf) return;
    
    // Write byte to buffer
    size_t written = circBuf->write(&data, 1);
    
    if (written == 0) {
        // Buffer full - data lost
        // Could increment overflow counter here if needed
        return;
    }
    
    // Update timing for buffer start
    if (circBuf->available() == 1) {
        *ctx->adaptive.bufferStartTime = currentMicros;
    }
    *ctx->adaptive.lastByteTime = currentMicros;
    
    // That's all! Pipeline will read from CircularBuffer independently
}

// Cleanup adaptive buffer
static inline void cleanupAdaptiveBuffer(BridgeContext* ctx) {
    if (ctx->adaptive.circBuf) {
        delete ctx->adaptive.circBuf;
        ctx->adaptive.circBuf = nullptr;
    }
    
    if (ctx->adaptive.bufferStartTime) {
        delete ctx->adaptive.bufferStartTime;
        ctx->adaptive.bufferStartTime = nullptr;
    }
    
    if (ctx->adaptive.lastByteTime) {
        delete ctx->adaptive.lastByteTime;
        ctx->adaptive.lastByteTime = nullptr;
    }
}

#endif // ADAPTIVE_BUFFER_H