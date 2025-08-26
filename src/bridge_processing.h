#ifndef BRIDGE_PROCESSING_H
#define BRIDGE_PROCESSING_H

#include "types.h"
#include "leds.h"
#include "logging.h"
#include "defines.h"
#include "uart/uart_interface.h"
#include "uart/uart_dma.h"
#include "protocols/uart_sender.h"
#include "usb/usb_interface.h"
// Protocol detection hooks (forward declarations to avoid circular includes)
#include <Arduino.h>

// Forward declaration for Pipeline
class ProtocolPipeline;

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
        uint8_t data = ctx->interfaces.uartBridgeSerial->read();
        
        // Update timestamp periodically based on baudrate
        if (++bytesProcessed % timestampUpdateInterval == 0) {
            currentTime = micros();
        }
        
        g_deviceStats.device1.rxBytes.fetch_add(1, std::memory_order_relaxed);
        g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
        
        

        // All data goes to telemetryBuffer for Pipeline routing
        if (ctx->buffers.telemetryBuffer) {
            // Process through adaptive buffer (for all Device2 types)
            processAdaptiveBufferByte(ctx, data, currentTime);
        }

        // lastActivity updated directly in g_deviceStats
    }
}

// Add new function for Device3 Bridge RX
static inline void processDevice3BridgeRx(BridgeContext* ctx) {
    if (ctx->devices.device3IsBridge && ctx->interfaces.device3Serial) {
        // Poll Device3 DMA events
        // Note: All UARTs use UartDMA implementation (see main.cpp)
        static_cast<UartDMA*>(ctx->interfaces.device3Serial)->pollEvents();
        
        // Process available data
        while (ctx->interfaces.device3Serial->available()) {
            uint8_t byte = ctx->interfaces.device3Serial->read();
            ctx->interfaces.uartBridgeSerial->write(byte);
            
            // Update RX statistics
            g_deviceStats.device3.rxBytes.fetch_add(1, std::memory_order_relaxed);
            g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
            // lastActivity updated directly in g_deviceStats
        }
    }
}

// Process Device 2 USB input
static inline void processDevice2USB(BridgeContext* ctx) {
    int bytesRead = 0;
    const int maxBytesPerLoop = 256;
    
    while (ctx->interfaces.usbInterface->available() && bytesRead < maxBytesPerLoop) {
        // Commands from GCS are critical - immediate byte-by-byte transfer
        int data = ctx->interfaces.usbInterface->read();
        if (data >= 0) {
            ctx->interfaces.uartBridgeSerial->write((uint8_t)data);
            g_deviceStats.device1.txBytes.fetch_add(1, std::memory_order_relaxed);
            g_deviceStats.device2.rxBytes.fetch_add(1, std::memory_order_relaxed);
            g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
            // lastActivity updated directly in g_deviceStats
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
    // Poll UART2 DMA events
    if (ctx->interfaces.device2Serial) {
        static_cast<UartDMA*>(ctx->interfaces.device2Serial)->pollEvents();
    }

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
            g_deviceStats.device2.rxBytes.fetch_add(actuallyRead, std::memory_order_relaxed);
            
            // Write to Device 1
            if (ctx->interfaces.uartBridgeSerial->availableForWrite() >= actuallyRead) {
                ctx->interfaces.uartBridgeSerial->write(uartReadBuffer, actuallyRead);
                g_deviceStats.device1.txBytes.fetch_add(actuallyRead, std::memory_order_relaxed);
                g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
            }
        }
    }
}


#endif // BRIDGE_PROCESSING_H