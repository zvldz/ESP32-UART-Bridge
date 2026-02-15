#include "buffer_manager.h"
#include "../logging.h"
#include "../adaptive_buffer.h"
#include "../defines.h"

void initProtocolBuffers(BridgeContext* ctx, Config* config) {
    // UART1 input buffer - always needed for Device1 data (UART or SBUS)
    size_t size;
    if (config->device1.role == D1_SBUS_IN) {
        size = 512;  // Fixed size for SBUS frames
        log_msg(LOG_INFO, "Device1 SBUS_IN: 512B buffer");
    } else {
        size = calculateAdaptiveBufferSize(config->baudrate);
        log_msg(LOG_INFO, "Device1 UART1: %zu bytes buffer", size);
    }
    ctx->buffers.uart1InputBuffer = new CircularBuffer();
    ctx->buffers.uart1InputBuffer->init(size);
    
    // Log buffer - only for Logger mode
    if (config->device4.role == D4_LOG_NETWORK) {
        ctx->buffers.logBuffer = new CircularBuffer();
        ctx->buffers.logBuffer->init(1024, true);  // true = use PSRAM if available
        log_msg(LOG_INFO, "Log buffer allocated: 1024 bytes (PSRAM preferred)");
    } else {
        ctx->buffers.logBuffer = nullptr;
    }
    
    // Input buffers for bidirectional pipeline

    // USB input buffer
    if (config->device2.role == D2_USB || config->device2.role == D2_USB_CRSF_BRIDGE) {
        size_t inputBufferSize = INPUT_BUFFER_SIZE;  // Use defined constant (4096)
        ctx->buffers.usbInputBuffer = new CircularBuffer();
        ctx->buffers.usbInputBuffer->init(inputBufferSize);
        log_msg(LOG_INFO, "USB input buffer allocated: %zu bytes", inputBufferSize);
    } else {
        ctx->buffers.usbInputBuffer = nullptr;
    }
    
    // UART2 input buffer
    if (config->device2.role == D2_UART2 || config->device2.role == D2_SBUS_IN || config->device2.role == D2_SBUS_OUT) {
        ctx->buffers.uart2InputBuffer = new CircularBuffer();
        size_t bufferSize;
        if (config->device2.role == D2_SBUS_IN || config->device2.role == D2_SBUS_OUT) {
            bufferSize = 256;  // Physical SBUS
        } else {
            bufferSize = 4096;  // UART2
        }
        ctx->buffers.uart2InputBuffer->init(bufferSize);
        log_msg(LOG_INFO, "UART2 buffer allocated: %zu bytes", bufferSize);
    } else {
        ctx->buffers.uart2InputBuffer = nullptr;
    }
    
    // UART3 input buffer
    if (config->device3.role == D3_UART3_BRIDGE || config->device3.role == D3_SBUS_IN || config->device3.role == D3_CRSF_BRIDGE) {
        ctx->buffers.uart3InputBuffer = new CircularBuffer();
        size_t bufferSize;
        if (config->device3.role == D3_SBUS_IN) {
            bufferSize = 256;  // Physical SBUS
        } else {
            bufferSize = 4096;  // UART3
        }
        ctx->buffers.uart3InputBuffer->init(bufferSize);
        log_msg(LOG_INFO, "UART3 buffer allocated: %zu bytes", bufferSize);
    } else {
        ctx->buffers.uart3InputBuffer = nullptr;
    }
    
    // UDP input buffer
    if (config->device4.role == D4_NETWORK_BRIDGE ||
        config->device4.role == D4_SBUS_UDP_TX || config->device4.role == D4_SBUS_UDP_RX) {
        ctx->buffers.udpInputBuffer = new CircularBuffer();
        size_t bufferSize;
        if (config->device4.role == D4_SBUS_UDP_TX || config->device4.role == D4_SBUS_UDP_RX) {
            bufferSize = 1024;  // Network SBUS
        } else {
            bufferSize = 4096;  // Network Bridge (MAVLink/RAW)
        }
        ctx->buffers.udpInputBuffer->init(bufferSize);
        log_msg(LOG_INFO, "UDP input buffer allocated: %zu bytes", bufferSize);
    } else {
        ctx->buffers.udpInputBuffer = nullptr;
    }

#if defined(MINIKIT_BT_ENABLED)
    // Bluetooth SPP input buffer (MiniKit with BT enabled)
    if (config->device5_config.role == D5_BT_BRIDGE) {
        ctx->buffers.btInputBuffer = new CircularBuffer();
        ctx->buffers.btInputBuffer->init(2048);  // Smaller buffer for memory-constrained MiniKit
        log_msg(LOG_INFO, "BT input buffer allocated: 2048 bytes");
    } else {
        ctx->buffers.btInputBuffer = nullptr;
    }
#endif
}

void freeProtocolBuffers(BridgeContext* ctx) {
    if (ctx->buffers.uart1InputBuffer) {
        delete ctx->buffers.uart1InputBuffer;
        ctx->buffers.uart1InputBuffer = nullptr;
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
#if defined(MINIKIT_BT_ENABLED)
    if (ctx->buffers.btInputBuffer) {
        delete ctx->buffers.btInputBuffer;
        ctx->buffers.btInputBuffer = nullptr;
    }
#endif
}