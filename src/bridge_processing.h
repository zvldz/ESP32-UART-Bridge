#ifndef BRIDGE_PROCESSING_H
#define BRIDGE_PROCESSING_H

#include "types.h"
#include "leds.h"
#include "logging.h"
#include "defines.h"
#include "uart_interface.h"
#include "usb_interface.h"
#include <Arduino.h>

// External buffer variables from uartbridge.cpp
extern uint8_t device3TxBuffer[];
extern uint8_t device3RxBuffer[];
extern int device3TxHead;
extern int device3TxTail;
extern int device3RxHead;
extern int device3RxTail;

// Check if we should yield to WiFi task
static inline bool shouldYieldToWiFi(BridgeContext* ctx, DeviceMode mode) {
    if (mode == MODE_CONFIG && millis() - *ctx->timing.lastWifiYield > 50) {
        *ctx->timing.lastWifiYield = millis();
        return true;
    }
    return false;
}

// Process data from Device 1 UART input
static inline void processDevice1Input(BridgeContext* ctx) {
    const unsigned long LED_NOTIFY_INTERVAL = 10; // Minimum 10ms between LED notifications
    const int UART_BLOCK_SIZE = 64;
    
    unsigned long currentTime = micros();
    
    while (ctx->interfaces.uartBridgeSerial->available()) {
        // Notify LED with rate limiting
        if (*ctx->system.currentMode == MODE_NORMAL && 
            millis() - *ctx->timing.lastUartLedNotify > LED_NOTIFY_INTERVAL) {
            led_notify_uart_rx();
            *ctx->timing.lastUartLedNotify = millis();
        }

        uint8_t data = ctx->interfaces.uartBridgeSerial->read();
        (*ctx->stats.device1RxBytes)++;
        
        // Copy to Device 3 if in Mirror or Bridge mode
        if (ctx->devices.device3Active) {
            if (xSemaphoreTake(*ctx->sync.device3Mutex, 0) == pdTRUE) {
                int nextHead = (device3TxHead + 1) % DEVICE3_UART_BUFFER_SIZE;
                if (nextHead != device3TxTail) {
                    device3TxBuffer[device3TxHead] = data;
                    device3TxHead = nextHead;
                }
                xSemaphoreGive(*ctx->sync.device3Mutex);
            }
        }

        // Route to Device 2 based on its role
        if (ctx->devices.device2IsUSB) {
            // Device 2 is USB - use adaptive buffering
            (*ctx->stats.totalUartPackets)++;

            // Start buffer timing on first byte
            if (*ctx->adaptive.bufferIndex == 0) {
                *ctx->adaptive.bufferStartTime = currentTime;
            }

            // Add byte to buffer
            ctx->adaptive.buffer[(*ctx->adaptive.bufferIndex)++] = data;
            *ctx->adaptive.lastByteTime = currentTime;

            // Determine if we should transmit the buffer
            bool shouldTransmit = false;
            unsigned long timeSinceLastByte = 0;
            unsigned long timeInBuffer = currentTime - *ctx->adaptive.bufferStartTime;

            // Calculate pause since last byte
            if (*ctx->adaptive.lastByteTime > 0 && !ctx->interfaces.uartBridgeSerial->available()) {
                delayMicroseconds(50);
                if (!ctx->interfaces.uartBridgeSerial->available()) {
                    timeSinceLastByte = micros() - *ctx->adaptive.lastByteTime;
                }
            }

            // Check if DMA detected packet boundary
            if (ctx->interfaces.uartBridgeSerial->hasPacketTimeout()) {
                shouldTransmit = true;
            }

            // Transmission decision logic
            if (*ctx->adaptive.bufferIndex <= 12 && timeSinceLastByte >= 200) {
                shouldTransmit = true;
            } else if (*ctx->adaptive.bufferIndex <= 64 && timeSinceLastByte >= 1000) {
                shouldTransmit = true;
            } else if (timeSinceLastByte >= 5000) {
                shouldTransmit = true;
            } else if (timeInBuffer >= 15000) {
                shouldTransmit = true;
            } else if (*ctx->adaptive.bufferIndex >= ctx->adaptive.bufferSize) {
                shouldTransmit = true;
            }

            // Transmit accumulated data if conditions met
            if (shouldTransmit) {
                int availableSpace = ctx->interfaces.usbInterface->availableForWrite();

                if (availableSpace >= *ctx->adaptive.bufferIndex) {
                    ctx->interfaces.usbInterface->write(ctx->adaptive.buffer, *ctx->adaptive.bufferIndex);
                    *ctx->stats.device2TxBytes += *ctx->adaptive.bufferIndex;
                    *ctx->adaptive.bufferIndex = 0;
                } else if (availableSpace > 0) {
                    ctx->interfaces.usbInterface->write(ctx->adaptive.buffer, availableSpace);
                    *ctx->stats.device2TxBytes += availableSpace;
                    memmove(ctx->adaptive.buffer, ctx->adaptive.buffer + availableSpace, 
                           *ctx->adaptive.bufferIndex - availableSpace);
                    *ctx->adaptive.bufferIndex -= availableSpace;
                } else {
                    // No space available - handle overflow
                    if (*ctx->adaptive.bufferIndex >= ctx->adaptive.bufferSize) {
                        *ctx->diagnostics.droppedBytes += *ctx->adaptive.bufferIndex;
                        *ctx->diagnostics.totalDroppedBytes += *ctx->adaptive.bufferIndex;
                        (*ctx->diagnostics.dropEvents)++;

                        if (*ctx->adaptive.bufferIndex > *ctx->diagnostics.maxDropSize) {
                            *ctx->diagnostics.maxDropSize = *ctx->adaptive.bufferIndex;
                        }

                        *ctx->adaptive.bufferIndex = 0;
                    }
                }
            }
        } else if (ctx->devices.device2IsUART2) {
            // Device 2 is UART2 - batch transfer
            uint8_t batchBuffer[UART_BLOCK_SIZE];
            int batchSize = 0;
            
            batchBuffer[batchSize++] = data;
            while (ctx->interfaces.uartBridgeSerial->available() && batchSize < UART_BLOCK_SIZE) {
                int nextByte = ctx->interfaces.uartBridgeSerial->read();
                if (nextByte >= 0) {
                    batchBuffer[batchSize++] = (uint8_t)nextByte;
                    (*ctx->stats.device1RxBytes)++;
                    
                    // Also copy to Device 3 if needed
                    if (ctx->devices.device3Active) {
                        if (xSemaphoreTake(*ctx->sync.device3Mutex, 0) == pdTRUE) {
                            int nextHead = (device3TxHead + 1) % DEVICE3_UART_BUFFER_SIZE;
                            if (nextHead != device3TxTail) {
                                device3TxBuffer[device3TxHead] = (uint8_t)nextByte;
                                device3TxHead = nextHead;
                            }
                            xSemaphoreGive(*ctx->sync.device3Mutex);
                        }
                    }
                }
            }
            
            // Write batch to Device 2
            if (batchSize > 0 && ctx->interfaces.device2Serial->availableForWrite() >= batchSize) {
                ctx->interfaces.device2Serial->write(batchBuffer, batchSize);
                *ctx->stats.device2TxBytes += batchSize;
            }
        }

        *ctx->stats.lastActivity = millis();
    }
}

// Process Device 2 USB input
static inline void processDevice2USB(BridgeContext* ctx) {
    const unsigned long LED_NOTIFY_INTERVAL = 10;
    int bytesRead = 0;
    const int maxBytesPerLoop = 256;
    
    while (ctx->interfaces.usbInterface->available() && bytesRead < maxBytesPerLoop) {
        // Notify LED with rate limiting
        if (*ctx->system.currentMode == MODE_NORMAL && 
            millis() - *ctx->timing.lastUsbLedNotify > LED_NOTIFY_INTERVAL) {
            led_notify_usb_rx();
            *ctx->timing.lastUsbLedNotify = millis();
        }

        // Commands from GCS are critical - immediate byte-by-byte transfer
        int data = ctx->interfaces.usbInterface->read();
        if (data >= 0) {
            ctx->interfaces.uartBridgeSerial->write((uint8_t)data);
            (*ctx->stats.device1TxBytes)++;
            (*ctx->stats.device2RxBytes)++;
            *ctx->stats.lastActivity = millis();
            bytesRead++;
            
            // Also send to Device 3 if in Bridge mode
            if (ctx->devices.device3IsBridge) {
                if (ctx->interfaces.device3Serial && ctx->interfaces.device3Serial->availableForWrite() > 0) {
                    ctx->interfaces.device3Serial->write((uint8_t)data);
                    // Note: Device3 TX bytes counted in device3Task
                }
            }
        }
    }
}

// Process Device 2 UART input
static inline void processDevice2UART(BridgeContext* ctx) {
    const int UART_BLOCK_SIZE = 64;
    uint8_t uartReadBuffer[UART_BLOCK_SIZE];
    
    int available = ctx->interfaces.device2Serial->available();
    if (available > 0) {
        // Read in blocks
        int toRead = min(available, UART_BLOCK_SIZE);
        int actuallyRead = 0;
        
        for (int i = 0; i < toRead; i++) {
            int byte = ctx->interfaces.device2Serial->read();
            if (byte >= 0) {
                uartReadBuffer[actuallyRead++] = (uint8_t)byte;
            }
        }
        
        if (actuallyRead > 0) {
            *ctx->stats.device2RxBytes += actuallyRead;
            
            // Write to Device 1
            if (ctx->interfaces.uartBridgeSerial->availableForWrite() >= actuallyRead) {
                ctx->interfaces.uartBridgeSerial->write(uartReadBuffer, actuallyRead);
                *ctx->stats.device1TxBytes += actuallyRead;
                *ctx->stats.lastActivity = millis();
            }
        }
    }
}

// Process Device 3 Bridge mode input
static inline void processDevice3BridgeInput(BridgeContext* ctx) {
    const int UART_BLOCK_SIZE = 64;
    
    if (xSemaphoreTake(*ctx->sync.device3Mutex, 0) == pdTRUE) {
        // Process in blocks for efficiency
        int bytesToWrite = 0;
        uint8_t writeBuffer[UART_BLOCK_SIZE];
        
        while (device3RxHead != device3RxTail && bytesToWrite < UART_BLOCK_SIZE) {
            writeBuffer[bytesToWrite++] = device3RxBuffer[device3RxTail];
            device3RxTail = (device3RxTail + 1) % DEVICE3_UART_BUFFER_SIZE;
        }
        
        xSemaphoreGive(*ctx->sync.device3Mutex);
        
        // Write collected data to Device 1
        if (bytesToWrite > 0 && ctx->interfaces.uartBridgeSerial->availableForWrite() >= bytesToWrite) {
            ctx->interfaces.uartBridgeSerial->write(writeBuffer, bytesToWrite);
            *ctx->stats.device1TxBytes += bytesToWrite;
        }
    }
}

#endif // BRIDGE_PROCESSING_H