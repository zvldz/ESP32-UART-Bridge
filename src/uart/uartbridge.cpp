#include "uartbridge.h"
#include "../bridge_processing.h"
#include "../adaptive_buffer.h"
#include "../protocols/protocol_pipeline.h"
#include "flow_control.h"
#include "../devices/device_init.h"
#include "../diagnostics.h"
#include "../logging.h"
#include "../leds.h"
#include "../defines.h"
#include "../config.h"
#include "../scheduler_tasks.h"
#include "../devices/device3_task.h"
#include "../devices/device4_task.h"
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

// Global pointer for Device4 access (cross-core communication)
ProtocolPipeline* g_pipeline = nullptr;

// UART Bridge Task - runs with high priority on Core 0
void uartBridgeTask(void* parameter) {
  // Wait for system initialization
  vTaskDelay(pdMS_TO_TICKS(1000));

  log_msg(LOG_INFO, "UART task started on core %d", xPortGetCoreID());

  // Dynamic buffer allocation based on baudrate
  const size_t adaptiveBufferSize = calculateAdaptiveBufferSize(config.baudrate);

  log_msg(LOG_INFO, "Adaptive buffering: %zu bytes (for %u baud). Thresholds: 200μs/1ms/5ms/15ms", 
          adaptiveBufferSize, config.baudrate);

  // Local counters for all devices
  unsigned long localDevice1RxBytes = 0;
  unsigned long localDevice1TxBytes = 0;
  unsigned long localDevice2RxBytes = 0;
  unsigned long localDevice2TxBytes = 0;
  unsigned long localDevice3RxBytes = 0;
  unsigned long localDevice3TxBytes = 0;
  unsigned long localDevice4RxBytes = 0;      // Device 4 RX bytes
  unsigned long localDevice4TxBytes = 0;      // Device 4 TX bytes
  unsigned long localDevice4RxPackets = 0;    // Device 4 RX packets
  unsigned long localDevice4TxPackets = 0;    // Device 4 TX packets
  unsigned long localLastActivity = 0;
  unsigned long localTotalUartPackets = 0;

  // Adaptive buffering variables
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
    &localDevice4RxBytes, &localDevice4TxBytes,
    &localDevice4RxPackets, &localDevice4TxPackets,
    &localLastActivity, &localTotalUartPackets,
    // Adaptive buffer
    adaptiveBufferSize,
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

  // Create protocol statistics BEFORE pipeline initialization
  ctx.protocol.stats = new ProtocolStats();
  log_msg(LOG_INFO, "Protocol statistics created");

  // Initialize CircularBuffer for new architecture
  initAdaptiveBuffer(&ctx, adaptiveBufferSize);

  // NEW: Initialize protocol pipeline instead of old detection
  ctx.protocolPipeline = new ProtocolPipeline(&ctx);
  ctx.protocolPipeline->init(&config);
  
  // Export for Device4 (cross-core access)
  g_pipeline = ctx.protocolPipeline;
  __sync_synchronize();  // Ensure visibility to other cores
  log_msg(LOG_INFO, "[Pipeline] Exported for Device4 access");
  
  // OLD: Keep old protocol detection as fallback (will be removed later)
  // initProtocolDetection(&ctx, &config);  // Removed - Pipeline does this

  // Configure hardware for selected protocol
  // configureHardwareForProtocol(&ctx, config.protocolOptimization);  // Removed - Pipeline does this

  log_msg(LOG_INFO, "UART Bridge Task started");
  log_msg(LOG_DEBUG, "Device optimization: D2 USB=%d, D2 UART2=%d, D3 Active=%d, D3 Bridge=%d", 
          device2IsUSB, device2IsUART2, device3Active, device3IsBridge);

  while (1) {
    // HOOK: Update protocol state
    // updateProtocolState(&ctx);  // Removed - Pipeline handles this
    
    // Poll Device 2 UART if configured
    if (device2IsUART2 && device2Serial) {
      static_cast<UartDMA*>(device2Serial)->pollEvents();
    }

    // Yield CPU time to WiFi stack periodically in network mode
    if (shouldYieldToWiFi(&ctx, bridgeMode)) {
      vTaskDelay(pdMS_TO_TICKS(5));
    }

    processDevice1Input(&ctx);
    
    // Process protocol pipeline
    if (ctx.protocolPipeline) {
        ctx.protocolPipeline->process();
    }
    
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
    
    // Device 4 Bridge mode (UDP->UART) now handled directly in device4_task.cpp

    // === TEMPORARY DIAGNOSTIC BLOCK START ===
    // Pipeline statistics output (every 10 seconds)
    static uint32_t lastPipelineStats = 0;
    if (millis() - lastPipelineStats > 10000) {
        char statBuf[512];  // Increased buffer size
        ctx.protocolPipeline->getStatsString(statBuf, sizeof(statBuf));
        log_msg(LOG_INFO, "Pipeline stats: %s", statBuf);
        
        // Memory pool statistics
        PacketMemoryPool::getInstance()->getStats(statBuf, sizeof(statBuf));
        log_msg(LOG_INFO, "%s", statBuf);
        
        lastPipelineStats = millis();
    }
    // === TEMPORARY DIAGNOSTIC BLOCK END ===

    // Fixed delay for multi-core systems (always 1ms)
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  
  // This point is never reached in normal operation, but cleanup if we get here
  if (ctx.protocolPipeline) {
    delete ctx.protocolPipeline;
    ctx.protocolPipeline = nullptr;
  }
  
  // Cleanup adaptive buffer
  cleanupAdaptiveBuffer(&ctx);
}

