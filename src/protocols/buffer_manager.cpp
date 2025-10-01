#include "buffer_manager.h"
#include "../logging.h"
#include "../adaptive_buffer.h"
#include "../defines.h"

// Calculate optimal buffer size based on device role and protocol type
size_t getOptimalBufferSize(uint8_t deviceRole, uint8_t protocolType) {
    // SBUS devices need minimal buffers (SBUS frame = 25 bytes)
    if (deviceRole == D1_SBUS_IN || deviceRole == D2_SBUS_IN ||
        deviceRole == D2_SBUS_OUT || deviceRole == D3_SBUS_OUT) {
        return 256;  // 10 SBUS frames buffer
    }

    // UDP for SBUS needs smaller buffer than for MAVLink/RAW
    if ((deviceRole == D4_NETWORK_BRIDGE && protocolType == PROTOCOL_SBUS) ||
        deviceRole == D4_SBUS_UDP_TX || deviceRole == D4_SBUS_UDP_RX) {
        return 1024;  // 40 SBUS frames, enough for network jitter
    }

    // Default for MAVLink/RAW telemetry
    return INPUT_BUFFER_SIZE;  // 4096 bytes
}

void initProtocolBuffers(BridgeContext* ctx, Config* config) {
    // Telemetry buffer - always needed for Device1 data (UART or SBUS)
    // Size depends on protocol: 512B for SBUS, adaptive for UART
    if (true) {  // Always create telemetryBuffer for Device1
        size_t size;
        if (config->device1.role == D1_SBUS_IN) {
            size = 512;  // Fixed size for SBUS frames
            log_msg(LOG_INFO, "Device1 SBUS_IN: Using 512B telemetryBuffer");
        } else {
            size = calculateAdaptiveBufferSize(config->baudrate);
            log_msg(LOG_INFO, "Device1 UART: Using adaptive telemetryBuffer (%zu bytes)", size);
        }
        ctx->buffers.telemetryBuffer = new CircularBuffer();
        ctx->buffers.telemetryBuffer->init(size);
    }
    
    // Log buffer - only for Logger mode
    if (config->device4.role == D4_LOG_NETWORK) {
        ctx->buffers.logBuffer = new CircularBuffer();
        ctx->buffers.logBuffer->init(1024, true);  // true = use PSRAM if available
        log_msg(LOG_INFO, "Log buffer allocated: 1024 bytes (PSRAM preferred)");
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
        size_t bufferSize = getOptimalBufferSize(config->device2.role, config->protocolOptimization);
        ctx->buffers.uart2InputBuffer->init(bufferSize);
        log_msg(LOG_INFO, "UART2 buffer allocated: %zu bytes", bufferSize);
    } else {
        ctx->buffers.uart2InputBuffer = nullptr;
    }
    
    // UART3 input buffer
    if (config->device3.role == D3_UART3_BRIDGE) {
        // D3_SBUS_OUT removed - Fast Path writes directly, doesn't read
        ctx->buffers.uart3InputBuffer = new CircularBuffer();
        size_t bufferSize = getOptimalBufferSize(config->device3.role, config->protocolOptimization);
        ctx->buffers.uart3InputBuffer->init(bufferSize);
        log_msg(LOG_INFO, "UART3 buffer allocated: %zu bytes", bufferSize);
    } else {
        ctx->buffers.uart3InputBuffer = nullptr;
    }
    
    // UDP input buffer
    if (config->device4.role == D4_NETWORK_BRIDGE ||
        config->device4.role == D4_SBUS_UDP_TX || config->device4.role == D4_SBUS_UDP_RX) {
        ctx->buffers.udpInputBuffer = new CircularBuffer();
        size_t bufferSize = getOptimalBufferSize(config->device4.role, config->protocolOptimization);
        ctx->buffers.udpInputBuffer->init(bufferSize);
        log_msg(LOG_INFO, "UDP input buffer allocated: %zu bytes", bufferSize);
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