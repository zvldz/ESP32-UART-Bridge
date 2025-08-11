#ifndef ADAPTIVE_BUFFER_H
#define ADAPTIVE_BUFFER_H

#include "types.h"
#include "logging.h"
#include "uart/uart_interface.h"
#include "usb/usb_interface.h"
#include "protocols/protocol_pipeline.h"  // Protocol detection hooks
#include <Arduino.h>
#include "diagnostics.h"

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
    
    // Debug: Track transmission attempts
    static uint32_t transmitAttempts = 0;
    static uint32_t lastTransmitReport = 0;
    transmitAttempts++;
    
    // Check if protocol found a packet
    if (ctx->protocol.enabled && ctx->protocol.packetFound) {
        // Transmit from packet start to packet end
        transmitOffset = ctx->protocol.currentPacketStart;
        bytesToSend = ctx->protocol.detectedPacketSize;
        
        // Ensure we don't send beyond buffer
        if (transmitOffset + bytesToSend > *ctx->adaptive.bufferIndex) {
            bytesToSend = *ctx->adaptive.bufferIndex - transmitOffset;
        }
        
        // Check if we have enough space for the complete packet
        int availableSpace = ctx->interfaces.usbInterface->availableForWrite();
        if (availableSpace < bytesToSend) {
            // Check buffer criticality
            size_t bufferFillPercent = (*ctx->adaptive.bufferIndex * 100) / ctx->adaptive.bufferSize;
            
            if (bufferFillPercent > 90) {
                // Buffer critically full - log and force partial transmission
                static uint32_t criticalCount = 0;
                criticalCount++;
                if (criticalCount % 10 == 1) {  // Log every 10th occurrence
                    log_msg(LOG_WARNING, "Buffer critical (%zu%%), forcing partial packet transmission", 
                            bufferFillPercent);
                }
                
                // INCREMENT STATISTICS
                if (ctx->protocol.stats) {
                    ctx->protocol.stats->partialTransmissions++;
                }
                
                // Continue with partial transmission
            } else {
                // Buffer not critical - wait for USB space
                static uint32_t waitCount = 0;
                waitCount++;
                if (waitCount % 100 == 1) {  // Log periodically
                    log_msg(LOG_DEBUG, "Waiting for USB space: need %zu, have %d", 
                            bytesToSend, availableSpace);
                }
                return;  // Don't transmit partial packet
            }
        }
        
        // Log packet transmission
        static uint32_t txPackets = 0;
        txPackets++;
        if (txPackets % 1000 == 0) {  // Reduced frequency
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
    
    // Debug: Report USB status periodically
    uint32_t now = millis();
    if (now - lastTransmitReport > 5000) {  // Every 5 seconds
        if (availableSpace == 0 || *ctx->adaptive.bufferIndex > ctx->adaptive.bufferSize / 2) {
            #ifdef DEBUG
            forceSerialLog("TX_STATUS: attempts=%u, bufIdx=%u/%u, USB_avail=%d, proto=%s",
                          transmitAttempts,
                          *ctx->adaptive.bufferIndex,
                          ctx->adaptive.bufferSize,
                          availableSpace,
                          ctx->protocol.enabled ? "MAVLINK" : "NONE");
            #endif
            lastTransmitReport = now;
            transmitAttempts = 0;  // Reset counter
        }
    }
    
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
            #ifdef DEBUG
            forceSerialLog("TX_ERROR: consumed=%u > bufferIndex=%u, offset=%u, sent=%u",
                          totalConsumed, *ctx->adaptive.bufferIndex, transmitOffset, bytesToSend);
            #endif
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
            #ifdef DEBUG
            forceSerialLog("TX_ERROR: Invalid remaining=%u > bufSize=%u", 
                          remaining, ctx->adaptive.bufferSize);
            #endif
            *ctx->adaptive.bufferIndex = 0;
            ctx->protocol.lastAnalyzedOffset = 0;
            ctx->protocol.packetFound = false;
            ctx->protocol.currentPacketStart = 0;
            return;
        }

        // Safe memmove only if there's data to move
        if (remaining > 0 && totalConsumed > 0) {
            #ifdef DEBUG
                // Only log problematic memmoves
                if (remaining > 1000 || totalConsumed > 1000) {
                    forceSerialLog("memmove#1: dst=%p, src=%p, size=%u, bufIdx=%u, totalConsumed=%u, remaining=%u", 
                        ctx->adaptive.buffer, 
                        ctx->adaptive.buffer + totalConsumed, 
                        remaining,
                        *ctx->adaptive.bufferIndex,
                        totalConsumed,
                        remaining);
                }
            #endif
            memmove(ctx->adaptive.buffer, 
                    ctx->adaptive.buffer + totalConsumed, 
                    remaining);
        }

        *ctx->adaptive.bufferIndex = remaining;
        
        // FIXED: Properly adjust protocol offsets instead of resetting
        // This maintains the correct analysis position after buffer shift
        if (ctx->protocol.lastAnalyzedOffset >= totalConsumed) {
            ctx->protocol.lastAnalyzedOffset -= totalConsumed;
        } else {
            ctx->protocol.lastAnalyzedOffset = 0;
        }
        
        // Adjust packet start position if we have a found packet
        if (ctx->protocol.packetFound && ctx->protocol.currentPacketStart >= totalConsumed) {
            ctx->protocol.currentPacketStart -= totalConsumed;
        } else {
            // Packet was transmitted or invalid position
            ctx->protocol.packetFound = false;
            ctx->protocol.currentPacketStart = 0;
        }
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
            #ifdef DEBUG
            forceSerialLog("PARTIAL_TX_ERROR: sent %u > available %u",
                          partialBytes, *ctx->adaptive.bufferIndex);
            #endif
            *ctx->adaptive.bufferIndex = 0;
        } else {
            size_t remaining = *ctx->adaptive.bufferIndex - partialBytes;
            
            // Move remaining data to front of buffer only if needed
            if (remaining > 0 && remaining <= ctx->adaptive.bufferSize) {
                #ifdef DEBUG
                    // Only log problematic partial transmissions
                    if (remaining > 1000 || partialBytes > 1000) {
                        forceSerialLog("memmove#2: dst=%p, src=%p, size=%u, bufIdx=%u, partialBytes=%u, remaining=%u", 
                            ctx->adaptive.buffer, 
                            ctx->adaptive.buffer + partialBytes, 
                            remaining,
                            *ctx->adaptive.bufferIndex,
                            partialBytes,
                            remaining);
                    }
                #endif
                memmove(ctx->adaptive.buffer, 
                        ctx->adaptive.buffer + partialBytes, 
                        remaining);
            }
            
            *ctx->adaptive.bufferIndex = remaining;
        }
        
        // FIXED: Update protocol analysis state after partial transmission
        // Properly adjust offset instead of crude logic
        if (ctx->protocol.lastAnalyzedOffset >= partialBytes) {
            ctx->protocol.lastAnalyzedOffset -= partialBytes;
        } else {
            ctx->protocol.lastAnalyzedOffset = 0;
        }
        
        // Adjust packet position for partial transmission
        if (ctx->protocol.packetFound && ctx->protocol.currentPacketStart >= partialBytes) {
            ctx->protocol.currentPacketStart -= partialBytes;
        } else {
            ctx->protocol.packetFound = false;
            ctx->protocol.currentPacketStart = 0;
        }
    } else {
        // No space available - USB buffer completely full
        static uint32_t usbFullCount = 0;
        static uint32_t lastUsbFullReport = 0;
        usbFullCount++;
        
        // Report USB full condition periodically
        if (usbFullCount % 100 == 1 || (now - lastUsbFullReport) > 5000) {
            #ifdef DEBUG
            forceSerialLog("USB_FULL#%u: bufIdx=%u/%u, no space for TX", 
                          usbFullCount, *ctx->adaptive.bufferIndex, ctx->adaptive.bufferSize);
            #endif
            lastUsbFullReport = now;
        }
        
        if (*ctx->adaptive.bufferIndex >= ctx->adaptive.bufferSize) {
            // Must drop data to prevent our buffer overflow
            *ctx->diagnostics.droppedBytes += *ctx->adaptive.bufferIndex;
            *ctx->diagnostics.totalDroppedBytes += *ctx->adaptive.bufferIndex;
            (*ctx->diagnostics.dropEvents)++;
            
            // Track maximum dropped packet size
            if (*ctx->adaptive.bufferIndex > *ctx->diagnostics.maxDropSize) {
                *ctx->diagnostics.maxDropSize = *ctx->adaptive.bufferIndex;
            }
            
            #ifdef DEBUG
            forceSerialLog("DROPPED: %u bytes, USB had no space", *ctx->adaptive.bufferIndex);
            #endif
            
            // HOOK: Post-transmission cleanup for dropped data
            onProtocolPacketTransmitted(ctx, *ctx->adaptive.bufferIndex);
            
            *ctx->adaptive.bufferIndex = 0;  // Drop buffer to prevent overflow
            
            // FIXED: Reset protocol state when dropping buffer
            ctx->protocol.lastAnalyzedOffset = 0;
            ctx->protocol.packetFound = false;
            ctx->protocol.currentPacketStart = 0;
        }
        // If buffer not full yet, keep data and try again next iteration
    }
    
    // Final safety check - ensure buffer index is valid
    if (*ctx->adaptive.bufferIndex > ctx->adaptive.bufferSize) {
        #ifdef DEBUG
        forceSerialLog("CORRUPTION: bufIdx=%u > bufSize=%u, forcing reset",
                      *ctx->adaptive.bufferIndex, ctx->adaptive.bufferSize);
        #endif
        *ctx->adaptive.bufferIndex = 0;
        ctx->protocol.lastAnalyzedOffset = 0;
        ctx->protocol.packetFound = false;
        ctx->protocol.currentPacketStart = 0;
    }
}

// Handle buffer timeout - force transmission after emergency timeout
static inline void handleBufferTimeout(BridgeContext* ctx) {
    if (*ctx->adaptive.bufferIndex > 0) {
        // CRITICAL: Validate buffer index first!
        if (*ctx->adaptive.bufferIndex > ctx->adaptive.bufferSize) {
            #ifdef DEBUG
            forceSerialLog("ERROR: Buffer index corrupted in timeout: %u > %u", 
                          *ctx->adaptive.bufferIndex, ctx->adaptive.bufferSize);
            #endif
            // Reset to safe state
            *ctx->adaptive.bufferIndex = 0;
            ctx->protocol.lastAnalyzedOffset = 0;
            ctx->protocol.packetFound = false;
            ctx->protocol.currentPacketStart = 0;
            return;
        }
        
        unsigned long timeInBuffer = micros() - *ctx->adaptive.bufferStartTime;
        
        if (timeInBuffer >= ADAPTIVE_BUFFER_TIMEOUT_EMERGENCY_US) {
            // Emergency timeout - try to transmit
            int availableSpace = ctx->interfaces.usbInterface->availableForWrite();
            
            // Validate availableSpace is reasonable
            if (availableSpace < 0 || availableSpace > 65536) {
                #ifdef DEBUG
                forceSerialLog("ERROR: Invalid availableSpace: %d", availableSpace);
                #endif
                // Drop buffer to prevent corruption
                *ctx->diagnostics.droppedBytes += *ctx->adaptive.bufferIndex;
                *ctx->diagnostics.totalDroppedBytes += *ctx->adaptive.bufferIndex;
                (*ctx->diagnostics.dropEvents)++;
                *ctx->adaptive.bufferIndex = 0;
                // FIXED: Reset protocol state
                ctx->protocol.lastAnalyzedOffset = 0;
                ctx->protocol.packetFound = false;
                ctx->protocol.currentPacketStart = 0;
                return;
            }
            
            if (availableSpace >= *ctx->adaptive.bufferIndex) {
                // Can write entire buffer
                ctx->interfaces.usbInterface->write(ctx->adaptive.buffer, *ctx->adaptive.bufferIndex);
                *ctx->stats.device2TxBytes += *ctx->adaptive.bufferIndex;
                *ctx->adaptive.bufferIndex = 0;
                // FIXED: Reset protocol state after full transmission
                ctx->protocol.lastAnalyzedOffset = 0;
                ctx->protocol.packetFound = false;
                ctx->protocol.currentPacketStart = 0;
            } else if (availableSpace > 0) {
                // Write what we can (safely)
                size_t toWrite = (size_t)availableSpace;  // We know it's positive now
                
                // Double-check toWrite doesn't exceed buffer
                if (toWrite > *ctx->adaptive.bufferIndex) {
                    toWrite = *ctx->adaptive.bufferIndex;
                }
                
                ctx->interfaces.usbInterface->write(ctx->adaptive.buffer, toWrite);
                *ctx->stats.device2TxBytes += toWrite;
                
                // CRITICAL: Check for underflow before subtraction
                if (*ctx->adaptive.bufferIndex < toWrite) {
                    #ifdef DEBUG
                    forceSerialLog("ERROR: Buffer underflow prevented: bufIdx=%u < toWrite=%u", 
                                  *ctx->adaptive.bufferIndex, toWrite);
                    #endif
                    *ctx->adaptive.bufferIndex = 0;
                    ctx->protocol.lastAnalyzedOffset = 0;
                    ctx->protocol.packetFound = false;
                    ctx->protocol.currentPacketStart = 0;
                    return;
                }
                
                // Safe calculation
                size_t remaining = *ctx->adaptive.bufferIndex - toWrite;
                
                // Additional validation
                if (remaining > ctx->adaptive.bufferSize) {
                    #ifdef DEBUG
                    forceSerialLog("ERROR: Invalid remaining in timeout: %u > bufSize=%u", 
                                  remaining, ctx->adaptive.bufferSize);
                    #endif
                    *ctx->adaptive.bufferIndex = 0;
                    ctx->protocol.lastAnalyzedOffset = 0;
                    ctx->protocol.packetFound = false;
                    ctx->protocol.currentPacketStart = 0;
                    return;
                }
                
                // Validate source pointer before memmove
                if (toWrite > ctx->adaptive.bufferSize) {
                    #ifdef DEBUG
                    forceSerialLog("ERROR: toWrite exceeds buffer size: %u > %u", 
                                  toWrite, ctx->adaptive.bufferSize);
                    #endif
                    *ctx->adaptive.bufferIndex = 0;
                    ctx->protocol.lastAnalyzedOffset = 0;
                    ctx->protocol.packetFound = false;
                    ctx->protocol.currentPacketStart = 0;
                    return;
                }
                
                // Safe memmove only if needed
                if (remaining > 0) {
                    #ifdef DEBUG
                        // Always log timeout memmoves as they're rare
                        forceSerialLog("memmove#3: dst=%p, src=%p, size=%u, bufIdx=%u, toWrite=%u, remaining=%u, availSpace=%d", 
                            ctx->adaptive.buffer, 
                            ctx->adaptive.buffer + toWrite, 
                            remaining,
                            *ctx->adaptive.bufferIndex,
                            toWrite,
                            remaining,
                            availableSpace);
                    #endif
                    
                    // Final safety check - ensure source is within buffer bounds
                    uint8_t* src = ctx->adaptive.buffer + toWrite;
                    uint8_t* bufEnd = ctx->adaptive.buffer + ctx->adaptive.bufferSize;
                    if (src >= bufEnd || src < ctx->adaptive.buffer) {
                        #ifdef DEBUG
                        forceSerialLog("ERROR: memmove source out of bounds: src=%p, buf=%p-%p", 
                                      src, ctx->adaptive.buffer, bufEnd);
                        #endif
                        *ctx->adaptive.bufferIndex = 0;
                        ctx->protocol.lastAnalyzedOffset = 0;
                        ctx->protocol.packetFound = false;
                        ctx->protocol.currentPacketStart = 0;
                        return;
                    }
                    
                    memmove(ctx->adaptive.buffer, src, remaining);
                }
                *ctx->adaptive.bufferIndex = remaining;
                
                // FIXED: Properly adjust protocol offsets after timeout memmove
                if (ctx->protocol.lastAnalyzedOffset >= toWrite) {
                    ctx->protocol.lastAnalyzedOffset -= toWrite;
                } else {
                    ctx->protocol.lastAnalyzedOffset = 0;
                }
                
                if (ctx->protocol.packetFound && ctx->protocol.currentPacketStart >= toWrite) {
                    ctx->protocol.currentPacketStart -= toWrite;
                } else {
                    ctx->protocol.packetFound = false;
                    ctx->protocol.currentPacketStart = 0;
                }
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
                
                // FIXED: Reset protocol state when dropping
                ctx->protocol.lastAnalyzedOffset = 0;
                ctx->protocol.packetFound = false;
                ctx->protocol.currentPacketStart = 0;
                
                // Log will be handled by periodic diagnostics
            }
        }
    }
}

// Process adaptive buffering for a single byte
static inline void processAdaptiveBufferByte(BridgeContext* ctx, uint8_t data, unsigned long currentTime) {
    // Debug counters for overflow tracking
    static uint32_t overflowCount = 0;
    static uint32_t lastOverflowReport = 0;
    
    // CRITICAL FIX: Prevent buffer overflow
    if (*ctx->adaptive.bufferIndex >= ctx->adaptive.bufferSize) {
        overflowCount++;
        
        // Report every 100 overflows or every 5 seconds
        uint32_t now = millis();
        if (overflowCount % 100 == 1 || (now - lastOverflowReport) > 5000) {
            #ifdef DEBUG
            forceSerialLog("OVERFLOW#%u: bufIdx=%u, size=%u, proto=%s, baud=%u, USB_avail=%d", 
                          overflowCount,
                          *ctx->adaptive.bufferIndex, 
                          ctx->adaptive.bufferSize,
                          ctx->protocol.enabled ? "MAVLINK" : "NONE",
                          ctx->system.config ? ctx->system.config->baudrate : 0,
                          ctx->interfaces.usbInterface->availableForWrite());
            lastOverflowReport = now;
            #endif
        }
        
        // Force transmit when buffer is full
        transmitAdaptiveBuffer(ctx, currentTime);
        
        // If still full, reset buffer (data loss but no crash)
        if (*ctx->adaptive.bufferIndex >= ctx->adaptive.bufferSize) {
            *ctx->adaptive.bufferIndex = 0;
            ctx->protocol.lastAnalyzedOffset = 0;
            ctx->protocol.packetFound = false;
            ctx->protocol.currentPacketStart = 0;
        }
    }
    
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
        timeSinceLastByte = micros() - *ctx->adaptive.lastByteTime;
    }
    
    // Check if we should transmit
    if (shouldTransmitBuffer(ctx, currentTime, timeSinceLastByte)) {
        transmitAdaptiveBuffer(ctx, currentTime);
    }
}

#endif // ADAPTIVE_BUFFER_H