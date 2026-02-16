#ifndef ADAPTIVE_BUFFER_H
#define ADAPTIVE_BUFFER_H

#include "types.h"
#include "circular_buffer.h"
#include "logging.h"
#include <Arduino.h>

// Calculate adaptive buffer size based on baud rate
static inline size_t calculateAdaptiveBufferSize(uint32_t baudrate) {
    // Fixed at 2048 — adaptive sizing (256-2048) caused issues with MAVLink bursts
    return 2048;
    // Previous adaptive sizing (disabled):
    // if (baudrate >= 921600) return 2048;
    // if (baudrate >= 460800) return 1024;
    // if (baudrate >= 230400) return 768;
    // if (baudrate >= 115200) return 384;
    // return 256;
}

// Initialize adaptive buffer timing
static inline void initAdaptiveBuffer(BridgeContext* ctx, size_t size) {
    // Set buffer size
    ctx->adaptive.bufferSize = size;
    
    // Initialize timing
    if (!ctx->adaptive.bufferStartTime) {
        ctx->adaptive.bufferStartTime = new unsigned long(0);
    }
    if (!ctx->adaptive.lastByteTime) {
        ctx->adaptive.lastByteTime = new unsigned long(0);
    }
    
    *ctx->adaptive.bufferStartTime = micros();
    *ctx->adaptive.lastByteTime = micros();
    
    log_msg(LOG_INFO, "Adaptive buffer timing initialized for %zu bytes", size);
}

// Process single byte - write to telemetry buffer
static inline void processAdaptiveBufferByte(BridgeContext* ctx, uint8_t data, 
                                            unsigned long currentMicros) {
    if (!ctx->buffers.uart1InputBuffer) {
        return;
    }

    CircularBuffer* circBuf = ctx->buffers.uart1InputBuffer;
    
    // Write byte to buffer
    size_t written = circBuf->write(&data, 1);
    
    if (written == 0) {
        // Buffer full - data lost
        return;
    }
    
    // Update timing for buffer start
    if (circBuf->available() == 1) {
        *ctx->adaptive.bufferStartTime = currentMicros;
    }
    *ctx->adaptive.lastByteTime = currentMicros;
    
    // That's all! Pipeline will read from CircularBuffer independently
}

// Cleanup adaptive buffer timing
static inline void cleanupAdaptiveBuffer(BridgeContext* ctx) {
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