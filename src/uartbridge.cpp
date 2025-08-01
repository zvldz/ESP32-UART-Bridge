#include "uartbridge.h"
#include "bridge_processing.h"
#include "adaptive_buffer.h"
#include "flow_control.h"
#include "device_init.h"
#include "diagnostics.h"
#include "logging.h"
#include "leds.h"
#include "defines.h"
#include "config.h"
#include "scheduler_tasks.h"
#include "device3_task.h"
#include "device4_task.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <AsyncUDP.h>

// DMA support includes
#include "uart_interface.h"
#include "uart_dma.h"

// External objects from main.cpp
extern BridgeMode bridgeMode;
extern Config config;
extern UartStats uartStats;
extern SystemState systemState;
extern FlowControlStatus flowControlStatus;
extern UartInterface* uartBridgeSerial;

// External USB interface pointer from device_init.cpp
extern UsbInterface* g_usbInterface;

// Device 2 UART (when configured as Secondary UART)
UartInterface* device2Serial = nullptr;




// Static variables for stats functions
static unsigned long* s_device1RxBytes = nullptr;
static unsigned long* s_device1TxBytes = nullptr;
static unsigned long* s_device2RxBytes = nullptr;
static unsigned long* s_device2TxBytes = nullptr;
static unsigned long* s_device3RxBytes = nullptr;
static unsigned long* s_device3TxBytes = nullptr;
static unsigned long* s_lastActivity = nullptr;

// Function implementations for scheduler
void updateMainStats() {
    if (!s_device1RxBytes) return;  // Not initialized yet
    
    updateSharedStats(*s_device1RxBytes, *s_device1TxBytes,
                     *s_device2RxBytes, *s_device2TxBytes,
                     *s_device3RxBytes, *s_device3TxBytes,
                     *s_lastActivity);
}



// UART Bridge Task - runs with high priority on Core 0
void uartBridgeTask(void* parameter) {
  // Wait for system initialization
  vTaskDelay(pdMS_TO_TICKS(1000));

  log_msg(LOG_INFO, "UART task started on core %d", xPortGetCoreID());

  // Dynamic buffer allocation based on baudrate and protocol setting
  const size_t adaptiveBufferSize = calculateAdaptiveBufferSize(
    config.baudrate, 
    config.protocolOptimization != PROTOCOL_NONE
  );

  uint8_t* adaptiveBuffer = (uint8_t*)pvPortMalloc(adaptiveBufferSize);
  if (!adaptiveBuffer) {
    log_msg(LOG_ERROR, "Failed to allocate adaptive buffer!");
    vTaskDelete(NULL);
    return;
  }

  log_msg(LOG_INFO, "Adaptive buffering: %zu bytes (for %u baud). Thresholds: 200μs/1ms/5ms/15ms", 
          adaptiveBufferSize, config.baudrate);

  // Local counters for all devices
  unsigned long localDevice1RxBytes = 0;
  unsigned long localDevice1TxBytes = 0;
  unsigned long localDevice2RxBytes = 0;
  unsigned long localDevice2TxBytes = 0;
  unsigned long localDevice3RxBytes = 0;
  unsigned long localDevice3TxBytes = 0;
  unsigned long localLastActivity = 0;
  unsigned long localTotalUartPackets = 0;

  // Store pointers for stats functions
  s_device1RxBytes = &localDevice1RxBytes;
  s_device1TxBytes = &localDevice1TxBytes;
  s_device2RxBytes = &localDevice2RxBytes;
  s_device2TxBytes = &localDevice2TxBytes;
  s_device3RxBytes = &localDevice3RxBytes;
  s_device3TxBytes = &localDevice3TxBytes;
  s_lastActivity = &localLastActivity;

  // Adaptive buffering variables
  int bufferIndex = 0;
  unsigned long lastByteTime = 0;
  unsigned long bufferStartTime = 0;

  // Timing variables
  unsigned long lastWifiYield = 0;
  unsigned long lastUartLedNotify = 0;
  unsigned long lastUsbLedNotify = 0;

  // Diagnostic counters
  unsigned long droppedBytes = 0;
  unsigned long totalDroppedBytes = 0;
  unsigned long lastDropLog = 0;
  unsigned long dropEvents = 0;
  int maxDropSize = 0;
  int timeoutDropSizes[10] = {0};
  int timeoutDropIndex = 0;

  // Cache device roles at start to avoid repeated checks
  bool device3Active = (config.device3.role == D3_UART3_MIRROR || config.device3.role == D3_UART3_BRIDGE);
  bool device2IsUSB = (config.device2.role == D2_USB && g_usbInterface);
  bool device2IsUART2 = (config.device2.role == D2_UART2 && device2Serial);
  bool device3IsBridge = (config.device3.role == D3_UART3_BRIDGE);

  // Initialize BridgeContext
  BridgeContext ctx;
  initBridgeContext(&ctx,
    // Statistics
    &localDevice1RxBytes, &localDevice1TxBytes,
    &localDevice2RxBytes, &localDevice2TxBytes,
    &localDevice3RxBytes, &localDevice3TxBytes,
    &localLastActivity, &localTotalUartPackets,
    // Adaptive buffer
    adaptiveBuffer, adaptiveBufferSize, &bufferIndex,
    &lastByteTime, &bufferStartTime,
    // Device flags
    device2IsUSB, device2IsUART2, device3Active, device3IsBridge,
    // Diagnostics
    &droppedBytes, &totalDroppedBytes,
    &dropEvents, &maxDropSize,
    timeoutDropSizes, &timeoutDropIndex,
    // Interfaces
    uartBridgeSerial, g_usbInterface,
    device2Serial, device3Serial,
    // Timing
    &lastUartLedNotify, &lastUsbLedNotify,
    &lastWifiYield, &lastDropLog,
    // Sync
    &device3Mutex,
    // System
    &bridgeMode, &config, &uartStats
  );

  // Set bridge context for diagnostics
  setBridgeContext(&ctx);

  // Initialize protocol detection
  initProtocolDetection(&ctx, &config);

  // Configure hardware for selected protocol
  configureHardwareForProtocol(&ctx, config.protocolOptimization);

  log_msg(LOG_INFO, "UART Bridge Task started");
  log_msg(LOG_DEBUG, "Device optimization: D2 USB=%d, D2 UART2=%d, D3 Active=%d, D3 Bridge=%d", 
          device2IsUSB, device2IsUART2, device3Active, device3IsBridge);

  while (1) {
    // HOOK: Update protocol state
    updateProtocolState(&ctx);
    
    // Poll Device 2 UART if configured
    if (device2IsUART2 && device2Serial) {
      static_cast<UartDMA*>(device2Serial)->pollEvents();
    }

    // Yield CPU time to WiFi stack periodically in network mode
    if (shouldYieldToWiFi(&ctx, bridgeMode)) {
      vTaskDelay(pdMS_TO_TICKS(5));
    }

    processDevice1Input(&ctx);
    
    // Process Device 2 based on type
    if (device2IsUSB) {
      processDevice2USB(&ctx);
    } else if (device2IsUART2) {
      processDevice2UART(&ctx);
    }
    
    // Process Device 3 Bridge mode input
    if (device3IsBridge) {
      processDevice3BridgeInput(&ctx);
    }
    
    // Process Device 4 Bridge mode input (UDP->UART)
    if (config.device4.role == D4_NETWORK_BRIDGE && systemState.networkActive) {
      processDevice4BridgeToUart(&ctx);
    }

    // Handle any remaining data in buffer (timeout-based flush) for USB
    if (device2IsUSB && bufferIndex > 0) {
      handleBufferTimeout(&ctx);
    }

    // HOOK: Check protocol timeouts
    checkProtocolTimeouts(&ctx);

    // Fixed delay for multi-core systems (always 1ms)
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  
  // This point is never reached in normal operation
  // But if task ever exits, free the allocated buffer
  vPortFree(adaptiveBuffer);
}

