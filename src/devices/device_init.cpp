#include "device_init.h"
#include "../uart/uart_dma.h"
#include "../uart/uart_interface.h"
#include "../usb/usb_interface.h"
#include "../uart/flow_control.h"
#include "diagnostics.h"
#include "defines.h"
#include "logging.h"
#include "config.h"
#include "types.h"
#include <freertos/semphr.h>

// External objects from main.cpp
extern Config config;
extern FlowControlStatus flowControlStatus;

// External objects from uartbridge.cpp
extern UartInterface* device2Serial;


// Device 3 UART interface (defined here, used via extern in device3_task.h)
UartInterface* device3Serial = nullptr;

// Global variable for USB interface (used in uartbridge.cpp)
UsbInterface* g_usbInterface = nullptr;

// Initialize main UART bridge (Device 1)
void initMainUART(UartInterface* serial, Config* config, UartStats* stats, UsbInterface* usb) {
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
  log_msg(LOG_INFO, "UART configured: %u baud, %s%c%s", config->baudrate,
          word_length_to_string(config->databits),
          parity_to_string(config->parity)[0],  // First char only
          stop_bits_to_string(config->stopbits));

  log_msg(LOG_INFO, "Using DMA-accelerated UART");

  // Detect flow control if enabled
  if (config->flowcontrol) {
    pinMode(CTS_PIN, INPUT_PULLUP);
    detectFlowControl();
  } else {
    pinMode(CTS_PIN, INPUT_PULLUP);
    pinMode(RTS_PIN, INPUT_PULLUP);
  }
  
  // Initialize Device 2 if configured
  if (config->device2.role == D2_UART2) {
    initDevice2UART();
  }
  
  // Initialize Device 3 if configured
  if (config->device3.role != D3_NONE) {
    initDevice3(config->device3.role);
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

// Initialize and log device configuration
void initDevices() {
  // Log device configuration using helper functions
  log_msg(LOG_INFO, "Device configuration:");
  log_msg(LOG_INFO, "- Device 1: Main UART Bridge (always enabled)");
  
  // Device 2 with role name
  String d2Info = "- Device 2: " + String(getDevice2RoleName(config.device2.role));
  if (config.device2.role == D2_USB) {
    d2Info += " (" + String(config.usb_mode == USB_MODE_HOST ? "Host" : "Device") + " mode)";
  }
  log_msg(LOG_INFO, "%s", d2Info.c_str());
  
  // Device 3 with role name
  log_msg(LOG_INFO, "- Device 3: %s", getDevice3RoleName(config.device3.role));
  
  // Device 4
  log_msg(LOG_INFO, "- Device 4: %s", config.device4.role == D4_NONE ? "Disabled" : "Future feature");
  
  // Initialize UART logger if Device 3 is configured for logging
  /*
  if (config.device3.role == D3_UART3_LOG) {
    logging_init_uart();
  }
  */
  
  // Log logging configuration
  log_msg(LOG_INFO, "Logging configuration:");
  log_msg(LOG_INFO, "- Web logs: %s", getLogLevelName(config.log_level_web));
  log_msg(LOG_INFO, "- UART logs: %s%s", getLogLevelName(config.log_level_uart),
          config.device3.role == D3_UART3_LOG ? " (Device 3)" : " (inactive)");
  log_msg(LOG_INFO, "- Network logs: %s (future)", getLogLevelName(config.log_level_network));
}