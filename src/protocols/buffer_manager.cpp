#include "buffer_manager.h"
#include "../logging.h"
#include "../adaptive_buffer.h"
#include <Arduino.h>  // For ESP.getFreeHeap()

void initProtocolBuffers(BridgeContext* ctx, Config* config) {
    // Telemetry buffer - needed ALWAYS when there's USB (independent of Logger)
    bool needTelemetryBuffer = 
        (config->device2.role == D2_USB) ||  // USB always needs buffer
        (config->device2.role == D2_UART2) ||
        (config->device2.role == D2_SBUS_OUT) ||  // SBUS OUT needs telemetry from UART1
        (config->device3.role == D3_UART3_MIRROR) ||
        (config->device3.role == D3_UART3_BRIDGE) ||
        (config->device3.role == D3_SBUS_OUT) ||  // SBUS OUT needs telemetry from UART1
        (config->device4.role == D4_NETWORK_BRIDGE);
    
    if (needTelemetryBuffer) {
        size_t size = calculateAdaptiveBufferSize(config->baudrate);
        ctx->buffers.telemetryBuffer = new CircularBuffer();
        ctx->buffers.telemetryBuffer->init(size);
        log_msg(LOG_INFO, "Telemetry buffer allocated: %zu bytes", size);
    } else {
        ctx->buffers.telemetryBuffer = nullptr;
    }
    
    // Log buffer - only for Logger mode
    if (config->device4.role == D4_LOG_NETWORK) {
        ctx->buffers.logBuffer = new CircularBuffer();
        ctx->buffers.logBuffer->init(1024);
        log_msg(LOG_INFO, "Log buffer allocated: 1024 bytes");
    } else {
        ctx->buffers.logBuffer = nullptr;
    }
    
    // Input buffers for bidirectional pipeline
    size_t inputBufferSize = INPUT_BUFFER_SIZE;  // Use defined constant (4096)
    
    // USB input buffer
    if (config->device2.role == D2_USB) {
        ctx->buffers.usbInputBuffer = new CircularBuffer();
        ctx->buffers.usbInputBuffer->init(inputBufferSize);
        log_msg(LOG_INFO, "USB input buffer allocated: %zu bytes", inputBufferSize);
    } else {
        ctx->buffers.usbInputBuffer = nullptr;
    }
    
    // UART2 input buffer
    if (config->device2.role == D2_UART2 || config->device2.role == D2_SBUS_IN || config->device2.role == D2_SBUS_OUT) {
        ctx->buffers.uart2InputBuffer = new CircularBuffer();
        
        // SBUS needs smaller buffers (25 bytes per frame)
        if (config->device2.role == D2_SBUS_IN || config->device2.role == D2_SBUS_OUT) {
            ctx->buffers.uart2InputBuffer->init(512);  // ~20 frames buffer
            log_msg(LOG_INFO, "UART2 SBUS buffer allocated: 512 bytes");
        } else {
            ctx->buffers.uart2InputBuffer->init(inputBufferSize);
            log_msg(LOG_INFO, "UART2 input buffer allocated: %zu bytes", inputBufferSize);
        }
    } else {
        ctx->buffers.uart2InputBuffer = nullptr;
    }
    
    // UART3 input buffer
    if (config->device3.role == D3_UART3_BRIDGE || config->device3.role == D3_SBUS_IN || config->device3.role == D3_SBUS_OUT) {
        ctx->buffers.uart3InputBuffer = new CircularBuffer();
        
        // SBUS needs smaller buffers (25 bytes per frame)
        if (config->device3.role == D3_SBUS_IN || config->device3.role == D3_SBUS_OUT) {
            ctx->buffers.uart3InputBuffer->init(512);  // ~20 frames buffer
            log_msg(LOG_INFO, "UART3 SBUS buffer allocated: 512 bytes");
        } else {
            ctx->buffers.uart3InputBuffer->init(inputBufferSize);
            log_msg(LOG_INFO, "UART3 input buffer allocated: %zu bytes", inputBufferSize);
        }
    } else {
        ctx->buffers.uart3InputBuffer = nullptr;
    }
    
    // UDP input buffer
    if (config->device4.role == D4_NETWORK_BRIDGE) {
        ctx->buffers.udpInputBuffer = new CircularBuffer();
        ctx->buffers.udpInputBuffer->init(inputBufferSize);
        log_msg(LOG_INFO, "UDP input buffer allocated: %zu bytes", inputBufferSize);
    } else {
        ctx->buffers.udpInputBuffer = nullptr;
    }

    // --- TEMPORARY DIAGNOSTICS START ---
    size_t total = 0;

    if (ctx->buffers.telemetryBuffer) {
        size_t size = ctx->buffers.telemetryBuffer->getCapacity();
        log_msg(LOG_INFO, "[BUFFERS] Telemetry: %zu bytes", size);
        total += size;
    }

    if (ctx->buffers.usbInputBuffer) {
        size_t size = ctx->buffers.usbInputBuffer->getCapacity();
        log_msg(LOG_INFO, "[BUFFERS] USB Input: %zu bytes", size);
        total += size;
    }

    if (ctx->buffers.uart2InputBuffer) {
        size_t size = ctx->buffers.uart2InputBuffer->getCapacity();
        log_msg(LOG_INFO, "[BUFFERS] UART2 Input: %zu bytes", size);
        total += size;
    }

    if (ctx->buffers.uart3InputBuffer) {
        size_t size = ctx->buffers.uart3InputBuffer->getCapacity();
        log_msg(LOG_INFO, "[BUFFERS] UART3 Input: %zu bytes", size);
        total += size;
    }

    if (ctx->buffers.udpInputBuffer) {
        size_t size = ctx->buffers.udpInputBuffer->getCapacity();
        log_msg(LOG_INFO, "[BUFFERS] UDP Input: %zu bytes", size);
        total += size;
    }

    if (ctx->buffers.logBuffer) {
        size_t size = ctx->buffers.logBuffer->getCapacity();
        log_msg(LOG_INFO, "[BUFFERS] Log: %zu bytes", size);
        total += size;
    }

    log_msg(LOG_INFO, "[BUFFERS] TOTAL ALLOCATED: %zu bytes", total);

    // --- TEMPORARY DIAGNOSTICS: FREE HEAP START ---
    log_msg(LOG_INFO, "[BUFFERS] Free heap after allocation: %d", ESP.getFreeHeap());
    // --- TEMPORARY DIAGNOSTICS: FREE HEAP END ---
    // --- TEMPORARY DIAGNOSTICS END ---
}

void freeProtocolBuffers(BridgeContext* ctx) {
    if (ctx->buffers.telemetryBuffer) {
        delete ctx->buffers.telemetryBuffer;
        ctx->buffers.telemetryBuffer = nullptr;
    }
    if (ctx->buffers.logBuffer) {
        delete ctx->buffers.logBuffer;
        ctx->buffers.logBuffer = nullptr;
    }
    
    // Free input buffers
    if (ctx->buffers.usbInputBuffer) {
        delete ctx->buffers.usbInputBuffer;
        ctx->buffers.usbInputBuffer = nullptr;
    }
    if (ctx->buffers.uart2InputBuffer) {
        delete ctx->buffers.uart2InputBuffer;
        ctx->buffers.uart2InputBuffer = nullptr;
    }
    if (ctx->buffers.uart3InputBuffer) {
        delete ctx->buffers.uart3InputBuffer;
        ctx->buffers.uart3InputBuffer = nullptr;
    }
    if (ctx->buffers.udpInputBuffer) {
        delete ctx->buffers.udpInputBuffer;
        ctx->buffers.udpInputBuffer = nullptr;
    }
}