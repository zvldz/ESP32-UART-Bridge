#include "device_init.h"
#include "uart_dma.h"
#include "uart_interface.h"
#include "defines.h"
#include "logging.h"
#include "config.h"
#include "types.h"

// External objects from main.cpp
extern Config config;

// External objects from uartbridge.cpp
extern UartInterface* device2Serial;
extern UartInterface* device3Serial;

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
    
    log_msg("Device 2 UART initialized on GPIO" + String(DEVICE2_UART_RX_PIN) + "/" + 
            String(DEVICE2_UART_TX_PIN) + " at " + String(config.baudrate) + 
            " baud (DMA polling mode)", LOG_INFO);
  } else {
    log_msg("Failed to create Device 2 UART", LOG_ERROR);
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
      log_msg("Device 3 Mirror mode initialized on GPIO" + String(DEVICE3_UART_TX_PIN) + 
              " (TX only) at " + String(config.baudrate) + " baud (UART0, DMA polling)", LOG_INFO);
    } else if (role == D3_UART3_BRIDGE) {
      // Bridge mode - full duplex
      device3Serial->begin(uartCfg, DEVICE3_UART_RX_PIN, DEVICE3_UART_TX_PIN);
      log_msg("Device 3 Bridge mode initialized on GPIO" + String(DEVICE3_UART_RX_PIN) + "/" + 
              String(DEVICE3_UART_TX_PIN) + " at " + String(config.baudrate) + 
              " baud (UART0, DMA polling)", LOG_INFO);
    }
  } else {
    log_msg("Failed to create Device 3 UART", LOG_ERROR);
  }
}