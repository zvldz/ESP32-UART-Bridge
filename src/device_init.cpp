#include "device_init.h"
#include "uart/uart_dma.h"
#include "uart/uart_interface.h"
#include "uart/uartbridge.h"
#include "usb/usb_interface.h"
#include "diagnostics.h"
#include "defines.h"
#include "logging.h"
#include "config.h"
#include "types.h"
#include "protocols/sbus_common.h"
#include "protocols/sbus_router.h"
#include "protocols/protocol_pipeline.h"
#include "protocols/packet_sender.h"
#include <freertos/semphr.h>

// External objects from main.cpp
extern Config config;

// External objects from uartbridge.cpp
extern UartInterface* device2Serial;

// Device 3 UART interface (defined here, used via extern in device3_task.h)
UartInterface* device3Serial = nullptr;

// Global variable for USB interface (used in uartbridge.cpp)
UsbInterface* g_usbInterface = nullptr;

// Initialize main UART bridge (Device 1)
void initMainUART(UartInterface* serial, Config* config, UsbInterface* usb) {
    // Initialize Device1 for SBUS_IN mode
    if (config->device1.role == D1_SBUS_IN) {
        // Store USB interface pointer
        g_usbInterface = usb;

        // Configure GPIO for SBUS
        pinMode(UART_RX_PIN, INPUT_PULLUP);

        // SBUS configuration: 100000 8E2 with signal inversion
        UartConfig sbusCfg = {
            .baudrate = 100000,
            .databits = UART_DATA_8_BITS,
            .parity = UART_PARITY_EVEN,
            .stopbits = UART_STOP_BITS_2,
            .flowcontrol = false
        };

        // Initialize DMA with SBUS configuration (RX only)
        serial->begin(sbusCfg, UART_RX_PIN, -1);

        // Enable signal inversion for SBUS
        uart_set_line_inverse(UART_NUM_1, UART_SIGNAL_RXD_INV);

        log_msg(LOG_INFO, "Device1 SBUS_IN initialized: 100000 8E2 INV (DMA active)");

        // Initialize other devices normally
        if (config->device2.role == D2_UART2) {
            initDevice2UART();
        } else if (config->device2.role == D2_SBUS_IN || config->device2.role == D2_SBUS_OUT) {
            initDevice2SBUS();
        }

        if (config->device3.role != D3_NONE) {
            if (config->device3.role == D3_SBUS_IN || config->device3.role == D3_SBUS_OUT) {
                initDevice3SBUS();
            } else {
                initDevice3(config->device3.role);
            }
        }

        return;
    }

    // Store USB interface pointer
    g_usbInterface = usb;

    // Configure UART with loaded settings
    pinMode(UART_RX_PIN, INPUT_PULLUP);

    // Create UartConfig from global Config
    UartConfig uartCfg = {
        .baudrate = config->baudrate,
        .databits = config->databits,
        .parity = config->parity,
        .stopbits = config->stopbits,
        .flowcontrol = config->flowcontrol
    };

    // Initialize serial port with full configuration
    serial->begin(uartCfg, UART_RX_PIN, UART_TX_PIN);

    // Log configuration
    log_msg(LOG_INFO, "UART configured: %u baud, %s%c%s%s",
            config->baudrate,
            word_length_to_string(config->databits),
            parity_to_string(config->parity)[0],
            stop_bits_to_string(config->stopbits),
            config->flowcontrol ? ", HW Flow Control" : "");

    log_msg(LOG_INFO, "Using DMA-accelerated UART");


    // Initialize Device 2 if configured
    if (config->device2.role == D2_UART2) {
        initDevice2UART();
    } else if (config->device2.role == D2_SBUS_IN || config->device2.role == D2_SBUS_OUT) {
        initDevice2SBUS();
    }

    // Initialize Device 3 if configured
    if (config->device3.role != D3_NONE) {
        if (config->device3.role == D3_SBUS_IN || config->device3.role == D3_SBUS_OUT) {
            initDevice3SBUS();  // Initialize SBUS for Device 3
        } else {
            initDevice3(config->device3.role);  // Existing UART modes
        }
    }

}

// Initialize Device 2 as Secondary UART
void initDevice2UART() {
    // Create UART configuration (Device 2 doesn't use flow control)
    UartConfig uartCfg = {
        .baudrate = config.baudrate,
        .databits = config.databits,
        .parity = config.parity,
        .stopbits = config.stopbits,
        .flowcontrol = false  // Device 2 doesn't use flow control
    };

    // Use UartDMA with polling mode for Device 2
    UartDMA::DmaConfig dmaCfg = {
        .useEventTask = false,     // Polling mode - no separate event task
        .dmaRxBufSize = 4096,     // Smaller buffers for secondary device
        .dmaTxBufSize = 4096,
        .ringBufSize = 8192       // Adequate for most protocols
    };

    device2Serial = new UartDMA(UART_NUM_2, dmaCfg);

    if (device2Serial) {
        // Initialize with full UART configuration
        device2Serial->begin(uartCfg, DEVICE2_UART_RX_PIN, DEVICE2_UART_TX_PIN);

        log_msg(LOG_INFO, "Device 2 UART initialized on GPIO%d/%d at %u baud (DMA polling mode)",
                DEVICE2_UART_RX_PIN, DEVICE2_UART_TX_PIN, config.baudrate);
    } else {
        log_msg(LOG_ERROR, "Failed to create Device 2 UART");
    }
}

// Initialize Device 2 as SBUS
void initDevice2SBUS() {
    // SBUS uses fixed configuration
    UartConfig sbusCfg = {
        .baudrate = SBUS_BAUDRATE,        // 100000
        .databits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,
        .stopbits = UART_STOP_BITS_2,
        .flowcontrol = false               // SBUS never uses flow control
    };

    // Use UartDMA with polling mode for Device 2
    UartDMA::DmaConfig dmaCfg = {
        .useEventTask = false,     // Polling mode
        .dmaRxBufSize = 4096,
        .dmaTxBufSize = 4096,
        .ringBufSize = 8192
    };

    device2Serial = new UartDMA(UART_NUM_2, dmaCfg);

    if (device2Serial) {
        // Initialize with SBUS configuration
        device2Serial->begin(sbusCfg, DEVICE2_UART_RX_PIN, DEVICE2_UART_TX_PIN);

        // Enable signal inversion for SBUS
        uart_set_line_inverse(UART_NUM_2, UART_SIGNAL_RXD_INV | UART_SIGNAL_TXD_INV);

        const char* modeStr = (config.device2.role == D2_SBUS_IN) ? "SBUS IN" : "SBUS OUT";
        log_msg(LOG_INFO, "Device 2 %s initialized on GPIO%d/%d (100000 8E2 INV)",
                modeStr, DEVICE2_UART_RX_PIN, DEVICE2_UART_TX_PIN);
    } else {
        log_msg(LOG_ERROR, "Failed to create Device 2 SBUS interface");
    }
}

// Initialize Device 3 based on role
void initDevice3(uint8_t role) {
    // Create UART configuration (Device 3 doesn't use flow control)
    UartConfig uartCfg = {
        .baudrate = config.baudrate,
        .databits = config.databits,
        .parity = config.parity,
        .stopbits = config.stopbits,
        .flowcontrol = false  // Device 3 doesn't use flow control
    };

    // Use UartDMA with polling mode for Device 3
    UartDMA::DmaConfig dmaCfg = {
        .useEventTask = false,     // Polling mode - no separate event task
        .dmaRxBufSize = 4096,     // Smaller buffers for secondary device
        .dmaTxBufSize = 4096,
        .ringBufSize = 8192       // Adequate for most protocols
    };

    device3Serial = new UartDMA(UART_NUM_0, dmaCfg);

    if (device3Serial) {
        if (role == D3_UART3_MIRROR) {
            // Mirror mode - TX only
            device3Serial->begin(uartCfg, -1, DEVICE3_UART_TX_PIN);
            log_msg(LOG_INFO, "Device 3 Mirror mode initialized on GPIO%d (TX only) at %u baud (UART0, DMA polling)",
                    DEVICE3_UART_TX_PIN, config.baudrate);
        } else if (role == D3_UART3_BRIDGE) {
            // Bridge mode - full duplex
            device3Serial->begin(uartCfg, DEVICE3_UART_RX_PIN, DEVICE3_UART_TX_PIN);
            log_msg(LOG_INFO, "Device 3 Bridge mode initialized on GPIO%d/%d at %u baud (UART0, DMA polling)",
                    DEVICE3_UART_RX_PIN, DEVICE3_UART_TX_PIN, config.baudrate);
        } else if (role == D3_UART3_LOG) {
            // Log mode - TX only with fixed 115200 baud
            UartConfig logCfg = {
                .baudrate = 115200,
                .databits = UART_DATA_8_BITS,
                .parity = UART_PARITY_DISABLE,
                .stopbits = UART_STOP_BITS_1,
                .flowcontrol = false
            };
            device3Serial->begin(logCfg, -1, DEVICE3_UART_TX_PIN);
            log_msg(LOG_INFO, "Device 3 Log mode initialized on GPIO%d (TX only) at 115200 baud (UART0, DMA polling)",
                    DEVICE3_UART_TX_PIN);
            logging_init_uart();
        }
    } else {
        log_msg(LOG_ERROR, "Failed to create Device 3 UART");
    }
}

// Initialize Device 3 as SBUS
void initDevice3SBUS() {
    // SBUS uses fixed configuration
    UartConfig sbusCfg = {
        .baudrate = SBUS_BAUDRATE,        // 100000
        .databits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,
        .stopbits = UART_STOP_BITS_2,
        .flowcontrol = false               // SBUS never uses flow control
    };

    // Use UartDMA with polling mode for Device 3
    UartDMA::DmaConfig dmaCfg = {
        .useEventTask = false,     // Polling mode
        .dmaRxBufSize = 4096,
        .dmaTxBufSize = 4096,
        .ringBufSize = 8192
    };

    device3Serial = new UartDMA(UART_NUM_0, dmaCfg);

    if (device3Serial) {
        // Initialize with SBUS configuration
        device3Serial->begin(sbusCfg, DEVICE3_UART_RX_PIN, DEVICE3_UART_TX_PIN);

        // Enable signal inversion for SBUS
        uart_set_line_inverse(UART_NUM_0, UART_SIGNAL_RXD_INV | UART_SIGNAL_TXD_INV);

        const char* modeStr = (config.device3.role == D3_SBUS_IN) ? "SBUS IN" : "SBUS OUT";
        log_msg(LOG_INFO, "Device 3 %s initialized on GPIO%d/%d (100000 8E2 INV, UART0)",
                modeStr, DEVICE3_UART_RX_PIN, DEVICE3_UART_TX_PIN);
    } else {
        log_msg(LOG_ERROR, "Failed to create Device 3 SBUS interface");
    }
}

// Helper to check if any SBUS device is configured
bool hasSbusDevice() {
    return (config.device1.role == D1_SBUS_IN ||
            config.device2.role == D2_SBUS_IN ||
            config.device2.role == D2_SBUS_OUT ||
            config.device3.role == D3_SBUS_IN ||
            config.device3.role == D3_SBUS_OUT ||
            config.device4.role == D4_SBUS_UDP_RX ||
            config.device4.role == D4_SBUS_UDP_TX);
}

// Register SBUS outputs after UART interfaces are created
void registerSbusOutputs() {
    if (!hasSbusDevice()) {
        return;
    }

    SbusRouter* router = SbusRouter::getInstance();

    // Get protocol pipeline to access senders
    extern ProtocolPipeline* getProtocolPipeline();
    ProtocolPipeline* pipeline = getProtocolPipeline();

    if (!pipeline) {
        log_msg(LOG_ERROR, "Pipeline not available for SBUS output registration");
        return;
    }

    // Register Device2 SBUS output
    if (config.device2.role == D2_SBUS_OUT) {
        PacketSender* sender = pipeline->getSender(IDX_DEVICE2_UART2);
        if (sender) {
            router->registerOutput(sender);
        } else {
            log_msg(LOG_ERROR, "Failed to get Device2 sender for SBUS output");
        }
    }

    // Register Device3 SBUS output
    if (config.device3.role == D3_SBUS_OUT) {
        PacketSender* sender = pipeline->getSender(IDX_DEVICE3);
        if (sender) {
            router->registerOutput(sender);
        } else {
            log_msg(LOG_ERROR, "Failed to get Device3 sender for SBUS output");
        }
    }

    // Register Device4 UDP output
    if (config.device4.role == D4_SBUS_UDP_TX) {
        PacketSender* sender = pipeline->getSender(IDX_DEVICE4);
        if (sender) {
            router->registerOutput(sender);
        } else {
            log_msg(LOG_ERROR, "Failed to get Device4 sender for SBUS UDP output");
        }
    }
}

// Initialize and log device configuration
void initDevices() {
    // Log device configuration using helper functions
    log_msg(LOG_INFO, "Device configuration:");
    log_msg(LOG_INFO, "- Device 1: Main UART Bridge (always enabled)");

    // Device 2 with role name - use direct logging instead of String concatenation
    if (config.device2.role == D2_USB) {
        log_msg(LOG_INFO, "- Device 2: %s (%s mode)",
                getDevice2RoleName(config.device2.role),
                config.usb_mode == USB_MODE_HOST ? "Host" : "Device");
    } else {
        log_msg(LOG_INFO, "- Device 2: %s", getDevice2RoleName(config.device2.role));
    }

    // Device 3 with role name
    log_msg(LOG_INFO, "- Device 3: %s", getDevice3RoleName(config.device3.role));

    // Device 4
    log_msg(LOG_INFO, "- Device 4: %s", config.device4.role == D4_NONE ? "Disabled" : "Future feature");

    // Log logging configuration
    log_msg(LOG_INFO, "Logging configuration:");
    log_msg(LOG_INFO, "- Web logs: %s", getLogLevelName(config.log_level_web));
    log_msg(LOG_INFO, "- UART logs: %s%s", getLogLevelName(config.log_level_uart),
            config.device3.role == D3_UART3_LOG ? " (Device 3)" : " (inactive)");
    log_msg(LOG_INFO, "- Network logs: %s (future)", getLogLevelName(config.log_level_network));

    // Initialize SBUS Router if any SBUS device is configured
    if (hasSbusDevice()) {
        SbusRouter* router = SbusRouter::getInstance();

        // Register sources with priorities
        if (config.device1.role == D1_SBUS_IN) {
            router->registerSource(SBUS_SOURCE_DEVICE1, 0);  // Highest priority
            log_msg(LOG_INFO, "SBUS source registered: Device1 (priority 0)");
        }

        if (config.device2.role == D2_SBUS_IN) {
            router->registerSource(SBUS_SOURCE_DEVICE2, 1);
            log_msg(LOG_INFO, "SBUS source registered: Device2 (priority 1)");
        }

        if (config.device3.role == D3_SBUS_IN) {
            router->registerSource(SBUS_SOURCE_DEVICE3, 2);
            log_msg(LOG_INFO, "SBUS source registered: Device3 (priority 2)");
        }

        if (config.device4.role == D4_SBUS_UDP_RX) {
            router->registerSource(SBUS_SOURCE_UDP, 3);      // Lowest priority
            log_msg(LOG_INFO, "SBUS source registered: UDP (priority 3)");
        }

        // Set Timing Keeper from config
        router->setTimingKeeper(config.sbusTimingKeeper);
        if (config.sbusTimingKeeper) {
            log_msg(LOG_INFO, "SBUS Timing Keeper enabled");
        }

        log_msg(LOG_INFO, "SBUS Router initialization complete");
    }
}