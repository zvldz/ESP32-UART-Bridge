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
}