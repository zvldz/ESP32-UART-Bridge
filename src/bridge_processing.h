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
#include "circular_buffer.h"
#include <Arduino.h>

// Forward declaration for Pipeline
class ProtocolPipeline;

#include "adaptive_buffer.h"

// TEMPORARY: Keep existing functions for stability - unification can be done later

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

// Process Device3 Bridge RX - renamed from processDevice3BridgeRx
static inline void processDevice3Input(BridgeContext* ctx) {
    // Poll Device3 DMA events
    static_cast<UartDMA*>(ctx->interfaces.device3Serial)->pollEvents();
    
    // Transfer data from Device3 to Device1 (UART bridge)
    uint8_t buffer[256];
    size_t totalTransferred = 0;
    
    while (ctx->interfaces.device3Serial->available() > 0 && totalTransferred < 256) {
        size_t canWrite = ctx->interfaces.uartBridgeSerial->availableForWrite();
        if (canWrite == 0) break;
        
        size_t toRead = min((size_t)ctx->interfaces.device3Serial->available(), 
                           min(canWrite, sizeof(buffer)));
        
        size_t actual = 0;
        for (int i = 0; i < toRead; i++) {
            int byte = ctx->interfaces.device3Serial->read();
            if (byte >= 0) {
                buffer[actual++] = (uint8_t)byte;
            } else {
                break;
            }
        }
        if (actual > 0) {
            ctx->interfaces.uartBridgeSerial->write(buffer, actual);
            totalTransferred += actual;
        } else {
            break;
        }
    }
    
    if (totalTransferred > 0) {
        g_deviceStats.device3.rxBytes.fetch_add(totalTransferred, std::memory_order_relaxed);
        g_deviceStats.device1.txBytes.fetch_add(totalTransferred, std::memory_order_relaxed);
        g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
    }
}

static inline void processDevice2USB(BridgeContext* ctx) {
    uint8_t buffer[64];
    const int maxBytesPerLoop = 256;
    int totalProcessed = 0;
    
    while (totalProcessed < maxBytesPerLoop) {
        int available = ctx->interfaces.usbInterface->available();
        if (available <= 0) break;
        
        int canWrite = ctx->interfaces.uartBridgeSerial->availableForWrite();
        if (canWrite <= 0) break;  // Critical! Without this we can block
        
        // Read up to 64 bytes at a time
        int toRead = min(min(available, canWrite), 64);
        int actual = 0;
        
        // Read into buffer
        for (int i = 0; i < toRead; i++) {
            int byte = ctx->interfaces.usbInterface->read();
            if (byte >= 0) {
                buffer[actual++] = (uint8_t)byte;
            } else {
                break;
            }
        }
        
        // Write in one call
        if (actual > 0) {
            ctx->interfaces.uartBridgeSerial->write(buffer, actual);
            
            // Statistics
            g_deviceStats.device2.rxBytes.fetch_add(actual, std::memory_order_relaxed);
            g_deviceStats.device1.txBytes.fetch_add(actual, std::memory_order_relaxed);
            g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
            
            totalProcessed += actual;
            
            // Device3 mirror (commented for review)
            // if (ctx->devices.device3IsBridge && ctx->interfaces.device3Serial) {
            //     size_t d3written = ctx->interfaces.device3Serial->write(buffer, actual);
            // }
        } else {
            break;
        }
    }
}

static inline void processDevice2UART(BridgeContext* ctx) {
    // Poll Device2 DMA events first
    static_cast<UartDMA*>(ctx->interfaces.device2Serial)->pollEvents();
    
    // Process data in blocks for efficiency
    uint8_t buffer[256];
    size_t totalProcessed = 0;
    
    while (ctx->interfaces.device2Serial->available() > 0 && totalProcessed < 512) {
        size_t toRead = min((size_t)ctx->interfaces.device2Serial->available(), sizeof(buffer));
        size_t actualRead = 0;
        for (int i = 0; i < toRead; i++) {
            int byte = ctx->interfaces.device2Serial->read();
            if (byte >= 0) {
                buffer[actualRead++] = (uint8_t)byte;
            } else {
                break;
            }
        }
        
        if (actualRead > 0) {
            ctx->interfaces.uartBridgeSerial->write(buffer, actualRead);
            totalProcessed += actualRead;
            
            // Update statistics
            g_deviceStats.device2.rxBytes.fetch_add(actualRead, std::memory_order_relaxed);
            g_deviceStats.device1.txBytes.fetch_add(actualRead, std::memory_order_relaxed);
            g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
        } else {
            break;
        }
    }
}

static inline void processDevice4Input(BridgeContext* ctx) {
    // Role check is done outside - process UDP data if available
    if (!ctx->buffers.udpRxBuffer || !ctx->buffers.udpRxBuffer->available()) 
        return;
    
    auto segments = ctx->buffers.udpRxBuffer->getReadSegments();
    if (segments.first.size == 0) return;
    
    size_t canWrite = ctx->interfaces.uartBridgeSerial->availableForWrite();
    size_t toWrite = min(segments.first.size, canWrite);
    
    if (toWrite > 0) {
        ctx->interfaces.uartBridgeSerial->write(segments.first.data, toWrite);
        ctx->buffers.udpRxBuffer->consume(toWrite);
        
        g_deviceStats.device4.rxBytes.fetch_add(toWrite, std::memory_order_relaxed);
        g_deviceStats.device1.txBytes.fetch_add(toWrite, std::memory_order_relaxed);
        g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
    }
}


#endif // BRIDGE_PROCESSING_H