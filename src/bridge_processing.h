#ifndef BRIDGE_PROCESSING_H
#define BRIDGE_PROCESSING_H

#include "types.h"
#include "leds.h"
#include "logging.h"
#include "defines.h"
#include "uart/uart_interface.h"
#include "usb/usb_interface.h"
#include "devices/device3_task.h"
// Protocol detection hooks (forward declarations to avoid circular includes)
#include <Arduino.h>

// Forward declaration for Pipeline
class ProtocolPipeline;
extern ProtocolPipeline* g_pipeline;

// NOW include adaptive_buffer.h - function is already defined!
#include "adaptive_buffer.h"  // Include for processAdaptiveBufferByte

// Check if we should yield to WiFi task
static inline bool shouldYieldToWiFi(BridgeContext* ctx, BridgeMode mode) {
    if (mode == BRIDGE_NET && millis() - *ctx->timing.lastWifiYield > 50) {
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
    int timestampUpdateInterval = 1;  // Default: update every byte
    
    // Calculate update interval based on baudrate for protocol mode
    if (ctx->protocol.enabled && ctx->system.config) {
        uint32_t baudrate = ctx->system.config->baudrate;
        if (baudrate >= 921600) {
            timestampUpdateInterval = 32;  // Update every 32 bytes
        } else if (baudrate >= 460800) {
            timestampUpdateInterval = 16;  // Update every 16 bytes
        } else if (baudrate >= 230400) {
            timestampUpdateInterval = 8;   // Update every 8 bytes
        } else if (baudrate >= 115200) {
            timestampUpdateInterval = 4;   // Update every 4 bytes
        } else {
            timestampUpdateInterval = 1;   // Update every byte for slow speeds
        }
    }
    
    int bytesProcessed = 0;
    
    while (ctx->interfaces.uartBridgeSerial->available()) {
        // Notify LED with rate limiting
        if (*ctx->system.bridgeMode == BRIDGE_STANDALONE && 
            millis() - *ctx->timing.lastUartLedNotify > LED_NOTIFY_INTERVAL) {
            led_notify_uart_rx();
            *ctx->timing.lastUartLedNotify = millis();
        }

        uint8_t data = ctx->interfaces.uartBridgeSerial->read();
        
        // Update timestamp periodically based on baudrate
        if (++bytesProcessed % timestampUpdateInterval == 0) {
            currentTime = micros();
        }
        
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
        
        // NEW: Copy to Device 4 via Pipeline (UART->UDP)
        if (ctx->system.config->device4.role == D4_NETWORK_BRIDGE && g_pipeline) {
            // No need to inject back into Pipeline - data already in input buffer
            // Pipeline will automatically process and route to UdpSender
        }

        // Route to Device 2 based on its role
        if (ctx->devices.device2IsUSB) {
            // Pipeline reads from buffer automatically - no need for hooks
            
            // Device 2 is USB - use adaptive buffering from adaptive_buffer.h
            (*ctx->stats.totalUartPackets)++;
            processAdaptiveBufferByte(ctx, data, currentTime);
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
                    
                    // NEW: Copy to Device 4 via Pipeline (UART->UDP)
                    if (ctx->system.config->device4.role == D4_NETWORK_BRIDGE && g_pipeline) {
                        // Data already goes through Pipeline via input buffer processing
                    }
                }
            }
            
            // Write batch to Device 2
            if (batchSize > 0 && ctx->interfaces.device2Serial->availableForWrite() >= batchSize) {
                ctx->interfaces.device2Serial->write(batchBuffer, batchSize);
                *ctx->stats.device2TxBytes += batchSize;
            }
            
            // NEW: Send batch to Device 4 via Pipeline (UART->UDP)
            if (batchSize > 0 && ctx->system.config->device4.role == D4_NETWORK_BRIDGE && g_pipeline) {
                // Data is already in Pipeline input buffer, will be processed automatically
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
        if (*ctx->system.bridgeMode == BRIDGE_STANDALONE && 
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