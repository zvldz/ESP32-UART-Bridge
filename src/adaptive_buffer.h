#ifndef ADAPTIVE_BUFFER_H
#define ADAPTIVE_BUFFER_H

#include "types.h"
#include "logging.h"
#include "uart_interface.h"
#include "usb_interface.h"
#include <Arduino.h>

// Adaptive buffering timeout constants (in microseconds)
#define ADAPTIVE_BUFFER_TIMEOUT_SMALL_US    200
#define ADAPTIVE_BUFFER_TIMEOUT_MEDIUM_US   1000
#define ADAPTIVE_BUFFER_TIMEOUT_LARGE_US    5000
#define ADAPTIVE_BUFFER_TIMEOUT_EMERGENCY_US 15000

// Packet size thresholds
#define PACKET_SIZE_SMALL    12
#define PACKET_SIZE_MEDIUM   64

// Calculate adaptive buffer size based on baud rate
static inline size_t calculateAdaptiveBufferSize(uint32_t baudrate) {
    if (baudrate >= 921600) return 2048;
    if (baudrate >= 460800) return 1024;
    if (baudrate >= 230400) return 768;
    if (baudrate >= 115200) return 288;
    return 256;
}

// Check if buffer should be transmitted based on timing and size
static inline bool shouldTransmitBuffer(BridgeContext* ctx, 
                                       unsigned long currentTime,
                                       unsigned long timeSinceLastByte) {
    // No data to transmit
    if (*ctx->adaptive.bufferIndex == 0) {
        return false;
    }
    
    // Calculate time buffer has been accumulating
    unsigned long timeInBuffer = currentTime - *ctx->adaptive.bufferStartTime;
    
    // Check if DMA detected packet boundary
    if (ctx->interfaces.uartBridgeSerial->hasPacketTimeout()) {
        return true;  // Hardware detected pause
    }
    
    // Transmission decision logic (prioritized for low latency):
    
    // 1. Small critical packets (heartbeat, commands) - immediate transmission
    if (*ctx->adaptive.bufferIndex <= PACKET_SIZE_SMALL && 
        timeSinceLastByte >= ADAPTIVE_BUFFER_TIMEOUT_SMALL_US) {
        return true;
    }
    
    // 2. Medium packets (attitude, GPS) - quick transmission
    if (*ctx->adaptive.bufferIndex <= PACKET_SIZE_MEDIUM && 
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
    if (*ctx->adaptive.bufferIndex >= ctx->adaptive.bufferSize) {
        return true;
    }
    
    return false;
}

// Transmit adaptive buffer to USB interface
static inline void transmitAdaptiveBuffer(BridgeContext* ctx) {
    // Check USB buffer availability to prevent blocking
    int availableSpace = ctx->interfaces.usbInterface->availableForWrite();
    
    if (availableSpace >= *ctx->adaptive.bufferIndex) {
        // Can write entire buffer
        ctx->interfaces.usbInterface->write(ctx->adaptive.buffer, *ctx->adaptive.bufferIndex);
        *ctx->stats.device2TxBytes += *ctx->adaptive.bufferIndex;
        *ctx->adaptive.bufferIndex = 0;  // Reset buffer
    } else if (availableSpace > 0) {
        // USB buffer has some space - write what we can
        ctx->interfaces.usbInterface->write(ctx->adaptive.buffer, availableSpace);
        *ctx->stats.device2TxBytes += availableSpace;
        
        // Move remaining data to front of buffer
        memmove(ctx->adaptive.buffer, 
                ctx->adaptive.buffer + availableSpace, 
                *ctx->adaptive.bufferIndex - availableSpace);
        *ctx->adaptive.bufferIndex -= availableSpace;
    } else {
        // No space available - USB buffer completely full
        if (*ctx->adaptive.bufferIndex >= ctx->adaptive.bufferSize) {
            // Must drop data to prevent our buffer overflow
            *ctx->diagnostics.droppedBytes += *ctx->adaptive.bufferIndex;
            *ctx->diagnostics.totalDroppedBytes += *ctx->adaptive.bufferIndex;
            (*ctx->diagnostics.dropEvents)++;
            
            // Track maximum dropped packet size
            if (*ctx->adaptive.bufferIndex > *ctx->diagnostics.maxDropSize) {
                *ctx->diagnostics.maxDropSize = *ctx->adaptive.bufferIndex;
            }
            
            *ctx->adaptive.bufferIndex = 0;  // Drop buffer to prevent overflow
            
            // Log will be handled by periodic diagnostics
        }
        // If buffer not full yet, keep data and try again next iteration
    }
}

// Handle buffer timeout - force transmission after emergency timeout
static inline void handleBufferTimeout(BridgeContext* ctx) {
    if (*ctx->adaptive.bufferIndex > 0) {
        unsigned long timeInBuffer = micros() - *ctx->adaptive.bufferStartTime;
        
        if (timeInBuffer >= ADAPTIVE_BUFFER_TIMEOUT_EMERGENCY_US) {
            // Emergency timeout - try to transmit
            int availableSpace = ctx->interfaces.usbInterface->availableForWrite();
            
            if (availableSpace >= *ctx->adaptive.bufferIndex) {
                // Can write entire buffer
                ctx->interfaces.usbInterface->write(ctx->adaptive.buffer, *ctx->adaptive.bufferIndex);
                *ctx->stats.device2TxBytes += *ctx->adaptive.bufferIndex;
                *ctx->adaptive.bufferIndex = 0;
            } else if (availableSpace > 0) {
                // Write what we can
                ctx->interfaces.usbInterface->write(ctx->adaptive.buffer, availableSpace);
                *ctx->stats.device2TxBytes += availableSpace;
                memmove(ctx->adaptive.buffer, 
                        ctx->adaptive.buffer + availableSpace, 
                        *ctx->adaptive.bufferIndex - availableSpace);
                *ctx->adaptive.bufferIndex -= availableSpace;
            } else {
                // Buffer stuck - drop data
                *ctx->diagnostics.droppedBytes += *ctx->adaptive.bufferIndex;
                *ctx->diagnostics.totalDroppedBytes += *ctx->adaptive.bufferIndex;
                (*ctx->diagnostics.dropEvents)++;
                
                // Track timeout drop sizes
                int* timeoutDropSizes = ctx->diagnostics.timeoutDropSizes;
                int* timeoutDropIndex = ctx->diagnostics.timeoutDropIndex;
                
                timeoutDropSizes[*timeoutDropIndex % 10] = *ctx->adaptive.bufferIndex;
                (*timeoutDropIndex)++;
                
                *ctx->adaptive.bufferIndex = 0;
                
                // Log will be handled by periodic diagnostics
            }
        }
    }
}

// Process adaptive buffering for a single byte
static inline void processAdaptiveBufferByte(BridgeContext* ctx, uint8_t data, unsigned long currentTime) {
    // ===== TEMPORARY DIAGNOSTIC CODE - REMOVE AFTER DEBUGGING =====
    // Added to diagnose FIFO overflow issue
    //static unsigned long delayCount = 0;
    //static unsigned long lastDelayLog = 0;
    // ===== END TEMPORARY DIAGNOSTIC CODE =====
    
    // Start buffer timing on first byte
    if (*ctx->adaptive.bufferIndex == 0) {
        *ctx->adaptive.bufferStartTime = currentTime;
    }
    
    // Add byte to buffer
    ctx->adaptive.buffer[(*ctx->adaptive.bufferIndex)++] = data;
    *ctx->adaptive.lastByteTime = currentTime;
    
    // Calculate pause since last byte
    unsigned long timeSinceLastByte = 0;
    if (*ctx->adaptive.lastByteTime > 0 && !ctx->interfaces.uartBridgeSerial->available()) {
        // Small delay to detect real pause vs processing delay
        delayMicroseconds(50);
        
        // ===== TEMPORARY DIAGNOSTIC CODE =====
        //delayCount++;
        // ===== END TEMPORARY DIAGNOSTIC CODE =====
        
        if (!ctx->interfaces.uartBridgeSerial->available()) {
            timeSinceLastByte = micros() - *ctx->adaptive.lastByteTime;
        }
    }
    
    // Check if we should transmit
    if (shouldTransmitBuffer(ctx, currentTime, timeSinceLastByte)) {
        transmitAdaptiveBuffer(ctx);
    }
    
    // ===== TEMPORARY DIAGNOSTIC CODE - REMOVE AFTER DEBUGGING =====
    //if (millis() - lastDelayLog > 5000 && delayCount > 0) {
    //    log_msg("[TEMP DIAG] Adaptive delays: " + String(delayCount) + 
    //            " times (50us each = " + String(delayCount * 50 / 1000) + "ms total)", LOG_WARNING);
    //    delayCount = 0;
    //    lastDelayLog = millis();
    //}
    // ===== END TEMPORARY DIAGNOSTIC CODE =====
}

#endif // ADAPTIVE_BUFFER_H