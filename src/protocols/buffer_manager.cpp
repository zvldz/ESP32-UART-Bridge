#include "buffer_manager.h"
#include "../logging.h"
#include "../adaptive_buffer.h"

void initProtocolBuffers(BridgeContext* ctx, Config* config) {
    // Telemetry buffer - needed ALWAYS when there's USB (independent of Logger)
    bool needTelemetryBuffer = 
        (config->device2.role == D2_USB) ||  // USB always needs buffer
        (config->device2.role == D2_UART2) ||
        (config->device3.role == D3_UART3_MIRROR) ||
        (config->device3.role == D3_UART3_BRIDGE) ||
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
    if (config->device2.role == D2_UART2) {
        ctx->buffers.uart2InputBuffer = new CircularBuffer();
        ctx->buffers.uart2InputBuffer->init(inputBufferSize);
        log_msg(LOG_INFO, "UART2 input buffer allocated: %zu bytes", inputBufferSize);
    } else {
        ctx->buffers.uart2InputBuffer = nullptr;
    }
    
    // UART3 input buffer
    if (config->device3.role == D3_UART3_BRIDGE) {
        ctx->buffers.uart3InputBuffer = new CircularBuffer();
        ctx->buffers.uart3InputBuffer->init(inputBufferSize);
        log_msg(LOG_INFO, "UART3 input buffer allocated: %zu bytes", inputBufferSize);
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