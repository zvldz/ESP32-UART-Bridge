#ifndef ADAPTIVE_BUFFER_H
#define ADAPTIVE_BUFFER_H

#include "types.h"
#include "logging.h"
#include "uart_interface.h"
#include "usb_interface.h"
#include "protocols/protocol_pipeline.h"  // Protocol detection hooks
#include <Arduino.h>

// Adaptive buffering timeout constants (in microseconds)
#define ADAPTIVE_BUFFER_TIMEOUT_SMALL_US    200
#define ADAPTIVE_BUFFER_TIMEOUT_MEDIUM_US   1000
#define ADAPTIVE_BUFFER_TIMEOUT_LARGE_US    5000
#define ADAPTIVE_BUFFER_TIMEOUT_EMERGENCY_US 15000

// Packet size thresholds
#define PACKET_SIZE_SMALL    12
#define PACKET_SIZE_MEDIUM   64

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

// Check if buffer should be transmitted based on timing and size
static inline bool shouldTransmitBuffer(BridgeContext* ctx, 
                                       unsigned long currentTime,
                                       unsigned long timeSinceLastByte) {
    // No data to transmit
    if (*ctx->adaptive.bufferIndex == 0) {
        return false;
    }
    
    // NEW: Check protocol-specific conditions first
    if (shouldTransmitProtocolBuffer(ctx, currentTime, timeSinceLastByte)) {
        return true;
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
static inline void transmitAdaptiveBuffer(BridgeContext* ctx, unsigned long currentTime) {
    size_t bytesToSend = *ctx->adaptive.bufferIndex;
    size_t transmitOffset = 0;  // Where to start transmitting from
    
    // Check if protocol found a packet
    if (ctx->protocol.enabled && ctx->protocol.packetFound) {
        // Transmit from packet start to packet end
        transmitOffset = ctx->protocol.currentPacketStart;
        bytesToSend = ctx->protocol.detectedPacketSize;
        
        // Ensure we don't send beyond buffer
        if (transmitOffset + bytesToSend > *ctx->adaptive.bufferIndex) {
            bytesToSend = *ctx->adaptive.bufferIndex - transmitOffset;
        }
        
        // Log packet transmission
        static uint32_t txPackets = 0;
        txPackets++;
        if (txPackets % 100 == 0) {
            log_msg(LOG_INFO, "Protocol: Transmitted %u packets", txPackets);
        }
    }
    
    // Special handling for protocol mode - allow more time for packet completion
    if (ctx->protocol.enabled && ctx->protocol.packetInProgress) {
        // Get base timeout based on baud rate
        uint32_t baseTimeout = 2000; // 2ms default
        if (ctx->system.config && ctx->system.config->baudrate <= 57600) {
            baseTimeout = 10000; // 10ms
        } else if (ctx->system.config && ctx->system.config->baudrate <= 230400) {
            baseTimeout = 5000; // 5ms
        }
        
        // Extend timeout for protocol mode
        uint32_t protocolTimeout = baseTimeout * 2;
        
        // Even longer timeout for MAVFtp mode
        if (ctx->protocol.mavftpActive) {
            protocolTimeout = baseTimeout * 4;
        }
        
        if ((currentTime - ctx->protocol.packetStartTime) < protocolTimeout) {
            return; // Give more time for protocol packets
        }
    } else if (ctx->protocol.enabled && !ctx->protocol.packetInProgress &&
               *ctx->adaptive.bufferIndex < 64 &&  // Only wait for small amounts of data
               (currentTime - *ctx->adaptive.bufferStartTime) < 1000) {  // 1ms max wait
        // Wait briefly for packet start detection on small data
        return;
    }
    // Otherwise: timeout reached or protocol disabled - transmit everything to prevent data loss
    
    // HOOK: Pre-transmission processing
    if (!preprocessProtocolTransmit(ctx, ctx->adaptive.buffer, &bytesToSend)) {
        return;
    }
    
    // Check USB buffer availability to prevent blocking
    int availableSpace = ctx->interfaces.usbInterface->availableForWrite();
    
    if (availableSpace >= bytesToSend) {
        // Transmit the data
        ctx->interfaces.usbInterface->write(ctx->adaptive.buffer + transmitOffset, bytesToSend);
        *ctx->stats.device2TxBytes += bytesToSend;
        
        // Post-transmission cleanup
        onProtocolPacketTransmitted(ctx, bytesToSend);
        
        // CRITICAL: Validate consumed bytes before calculating remaining
        size_t totalConsumed = transmitOffset + bytesToSend;

        // Safety check - totalConsumed must not exceed buffer index
        if (totalConsumed > *ctx->adaptive.bufferIndex) {
            // Logic error detected - log and reset buffer
            log_msg(LOG_ERROR, "TX error: consumed=%zu > bufferIndex=%zu, offset=%zu, sent=%zu",
                    totalConsumed, *ctx->adaptive.bufferIndex, transmitOffset, bytesToSend);
            
            // Reset to safe state
            *ctx->adaptive.bufferIndex = 0;
            ctx->protocol.lastAnalyzedOffset = 0;
            ctx->protocol.packetFound = false;
            ctx->protocol.currentPacketStart = 0;
            return;
        }

        size_t remaining = *ctx->adaptive.bufferIndex - totalConsumed;

        // Additional validation of remaining size
        if (remaining > ctx->adaptive.bufferSize) {
            log_msg(LOG_ERROR, "Invalid remaining size: %zu > buffer size %zu", 
                    remaining, ctx->adaptive.bufferSize);
            *ctx->adaptive.bufferIndex = 0;
            ctx->protocol.lastAnalyzedOffset = 0;
            ctx->protocol.packetFound = false;
            ctx->protocol.currentPacketStart = 0;
            return;
        }

        // Safe memmove only if there's data to move
        if (remaining > 0 && totalConsumed > 0) {
            memmove(ctx->adaptive.buffer, 
                    ctx->adaptive.buffer + totalConsumed, 
                    remaining);
        }

        *ctx->adaptive.bufferIndex = remaining;
        
        // Reset protocol analysis state
        ctx->protocol.lastAnalyzedOffset = 0;
        ctx->protocol.packetFound = false;
        ctx->protocol.currentPacketStart = 0;
    } else if (availableSpace > 0) {
        // Partial transmission - this is tricky with offset
        // For now, fall back to transmitting from beginning
        if (ctx->protocol.packetFound && transmitOffset > 0) {
            // Can't do partial transmission of offset packet
            // Wait for more USB space
            return;
        }
        
        // Calculate safe partial bytes to send
        size_t partialBytes = (availableSpace < *ctx->adaptive.bufferIndex) ? 
                      (size_t)availableSpace : (size_t)*ctx->adaptive.bufferIndex;
        
        // Normal partial transmission from buffer start
        ctx->interfaces.usbInterface->write(ctx->adaptive.buffer, partialBytes);
        *ctx->stats.device2TxBytes += partialBytes;
        
        // HOOK: Post-transmission cleanup for partial transmission
        onProtocolPacketTransmitted(ctx, partialBytes);
        
        // Safe calculation of remaining bytes
        if (partialBytes > *ctx->adaptive.bufferIndex) {
            // Should never happen after min(), but be paranoid
            log_msg(LOG_ERROR, "Partial TX error: sent %zu > available %zu",
                    partialBytes, *ctx->adaptive.bufferIndex);
            *ctx->adaptive.bufferIndex = 0;
        } else {
            size_t remaining = *ctx->adaptive.bufferIndex - partialBytes;
            
            // Move remaining data to front of buffer only if needed
            if (remaining > 0 && remaining <= ctx->adaptive.bufferSize) {
                memmove(ctx->adaptive.buffer, 
                        ctx->adaptive.buffer + partialBytes, 
                        remaining);
            }
            
            *ctx->adaptive.bufferIndex = remaining;
        }
        
        // Update protocol analysis state after partial transmission
        if (ctx->protocol.lastAnalyzedOffset > availableSpace) {
            ctx->protocol.lastAnalyzedOffset -= availableSpace;
        } else {
            ctx->protocol.lastAnalyzedOffset = 0;
        }
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
            
            // HOOK: Post-transmission cleanup for dropped data
            onProtocolPacketTransmitted(ctx, *ctx->adaptive.bufferIndex);
            
            *ctx->adaptive.bufferIndex = 0;  // Drop buffer to prevent overflow
            
            // Log will be handled by periodic diagnostics
        }
        // If buffer not full yet, keep data and try again next iteration
    }
    
    // Final safety check - ensure buffer index is valid
    if (*ctx->adaptive.bufferIndex > ctx->adaptive.bufferSize) {
        log_msg(LOG_ERROR, "Buffer index corrupted: %zu > %zu, forcing reset",
                *ctx->adaptive.bufferIndex, ctx->adaptive.bufferSize);
        *ctx->adaptive.bufferIndex = 0;
        ctx->protocol.lastAnalyzedOffset = 0;
        ctx->protocol.packetFound = false;
        ctx->protocol.currentPacketStart = 0;
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
        
        if (!ctx->interfaces.uartBridgeSerial->available()) {
            timeSinceLastByte = micros() - *ctx->adaptive.lastByteTime;
        }
    }
    
    // Check if we should transmit
    if (shouldTransmitBuffer(ctx, currentTime, timeSinceLastByte)) {
        transmitAdaptiveBuffer(ctx, currentTime);
    }
}

#endif // ADAPTIVE_BUFFER_H