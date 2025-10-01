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

// Forward declaration
class ProtocolPipeline;

#include "adaptive_buffer.h"

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
    // CRITICAL: Check Device1 role first!
    extern Config config;

    // Note: Device1 SBUS_IN must still read data from UART!
    // Old code that skipped reading was from when only Device2/3 could be SBUS
    // Now Device1 can be SBUS source, so it must read its own data

    // Poll Device1 DMA events first (for both UART and SBUS modes)
    // TODO: Simplify architecture - add typed DMA pointers to BridgeContext to avoid static_cast
    if (ctx->interfaces.uartBridgeSerial) {
        static_cast<UartDMA*>(ctx->interfaces.uartBridgeSerial)->pollEvents();
    }

    // Normal UART bridge processing for D1_UART1
    // Local buffer for batch reading (sized for MAVLink v2 max frame + margin)
    uint8_t buffer[320];
    
    // Process all available data in batches
    while (ctx->interfaces.uartBridgeSerial->available() > 0) {
        // Batch read - reads all available bytes (up to buffer size)
        // CRITICAL: Does NOT wait for buffer to fill - reads what's available
        size_t bytesRead = ctx->interfaces.uartBridgeSerial->readBytes(buffer, sizeof(buffer));

        if (bytesRead == 0) {
            // No data read (mutex was busy or no data)
            break;
        }
        
        // Write to telemetryBuffer (always exists for Device1)
        if (ctx->buffers.telemetryBuffer) {
            size_t written = ctx->buffers.telemetryBuffer->write(buffer, bytesRead);
        }
        
        // Update statistics once per batch (not per byte)
        g_deviceStats.device1.rxBytes.fetch_add(bytesRead, std::memory_order_relaxed);
        g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
    }
}

// Process Device3 UART Bridge RX
static inline void processDevice3UART(BridgeContext* ctx) {
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
        if (actual > 0 && ctx->buffers.uart3InputBuffer) {
            // FIFO with eviction if buffer full
            size_t free = ctx->buffers.uart3InputBuffer->freeSpace();
            if (free < actual) {
                // Drop oldest data to make room
                ctx->buffers.uart3InputBuffer->consume(actual - free);
                // TODO: Add dropped bytes counter
            }
            
            ctx->buffers.uart3InputBuffer->write(buffer, actual);
            totalTransferred += actual;
        } else {
            break;
        }
    }
    
    if (totalTransferred > 0) {
        g_deviceStats.device3.rxBytes.fetch_add(totalTransferred, std::memory_order_relaxed);
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
        
        // Write to input buffer with backpressure
        if (actual > 0 && ctx->buffers.usbInputBuffer) {
            // FIFO with eviction if buffer full
            size_t free = ctx->buffers.usbInputBuffer->freeSpace();
            if (free < (size_t)actual) {
                // Drop oldest data to make room
                ctx->buffers.usbInputBuffer->consume(actual - free);
                // TODO: Add dropped bytes counter
            }
            
            ctx->buffers.usbInputBuffer->write(buffer, actual);
            
            // Statistics
            g_deviceStats.device2.rxBytes.fetch_add(actual, std::memory_order_relaxed);
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

    extern Config config;

    // Process data in blocks for efficiency
    uint8_t buffer[256];
    size_t totalProcessed = 0;

    while (ctx->interfaces.device2Serial->available() > 0 && totalProcessed < 512) {
        // For SBUS_IN, don't check UART1 canWrite - data goes to buffer, not UART1
        size_t toRead;
        if (config.device2.role == D2_SBUS_IN) {
            // SBUS_IN: read all available data (up to buffer size)
            toRead = min((size_t)ctx->interfaces.device2Serial->available(), sizeof(buffer));
        } else {
            // UART2/SBUS_OUT: check UART1 write space
            size_t canWrite = ctx->interfaces.uartBridgeSerial->availableForWrite();
            toRead = min(min((size_t)ctx->interfaces.device2Serial->available(),
                            sizeof(buffer)), canWrite);
        }

        size_t actualRead = 0;
        for (int i = 0; i < toRead; i++) {
            int byte = ctx->interfaces.device2Serial->read();
            if (byte >= 0) {
                buffer[actualRead++] = (uint8_t)byte;
            } else {
                break;
            }
        }
        
        if (actualRead > 0 && ctx->buffers.uart2InputBuffer) {
            // FIFO with eviction if buffer full
            size_t free = ctx->buffers.uart2InputBuffer->freeSpace();
            if (free < actualRead) {
                // Drop oldest data to make room
                ctx->buffers.uart2InputBuffer->consume(actualRead - free);
                // TODO: Add dropped bytes counter
            }
            
            ctx->buffers.uart2InputBuffer->write(buffer, actualRead);
            totalProcessed += actualRead;
            
            // Update statistics
            g_deviceStats.device2.rxBytes.fetch_add(actualRead, std::memory_order_relaxed);
            g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
        } else {
            break;
        }
    }
}

static inline void processDevice4UDP(BridgeContext* ctx) {
    // Role check is done outside - process UDP data if available
    if (!ctx->buffers.udpRxBuffer || !ctx->buffers.udpRxBuffer->available()) 
        return;
    
    auto segments = ctx->buffers.udpRxBuffer->getReadSegments();
    if (segments.first.size == 0) return;
    
    size_t toWrite = segments.first.size;
    
    if (toWrite > 0 && ctx->buffers.udpInputBuffer) {
        // FIFO with eviction if buffer full
        size_t free = ctx->buffers.udpInputBuffer->freeSpace();
        if (free < toWrite) {
            // Drop oldest data to make room
            ctx->buffers.udpInputBuffer->consume(toWrite - free);
            // TODO: Add dropped bytes counter
        }
        
        ctx->buffers.udpInputBuffer->write(segments.first.data, toWrite);
        ctx->buffers.udpRxBuffer->consume(toWrite);
        
        g_deviceStats.device4.rxBytes.fetch_add(toWrite, std::memory_order_relaxed);
        g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
    }
    
    // Process second segment if available
    if (segments.second.size > 0 && ctx->buffers.udpInputBuffer) {
        size_t toWrite = segments.second.size;
        // FIFO with eviction if buffer full
        size_t free = ctx->buffers.udpInputBuffer->freeSpace();
        if (free < toWrite) {
            // Drop oldest data to make room
            ctx->buffers.udpInputBuffer->consume(toWrite - free);
            // TODO: Add dropped bytes counter
        }
        
        ctx->buffers.udpInputBuffer->write(segments.second.data, toWrite);
        ctx->buffers.udpRxBuffer->consume(toWrite);
        // Update statistics
        g_deviceStats.device4.rxBytes.fetch_add(toWrite, std::memory_order_relaxed);
    }
}


#endif // BRIDGE_PROCESSING_H