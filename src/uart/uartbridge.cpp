#include "uartbridge.h"
#include "../bridge_processing.h"
#include "../adaptive_buffer.h"
#include "../protocols/protocol_pipeline.h"
#include "../protocols/buffer_manager.h"
#include "../device_init.h"
#include "../diagnostics.h"
#include "../logging.h"
#include "../leds.h"
#include "../defines.h"
#include "../config.h"
#include "../scheduler_tasks.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <AsyncUDP.h>

// DMA support includes
#include "uart_interface.h"
#include "uart_dma.h"
#include "uart1_tx_service.h"

// External objects from main.cpp
extern BridgeMode bridgeMode;
extern Config config;
extern SystemState systemState;
extern UartInterface* uartBridgeSerial;

// ADD: Global pipeline pointer for sender task
static ProtocolPipeline* g_protocolPipeline = nullptr;

// External USB interface pointer from device_init.cpp
extern UsbInterface* g_usbInterface;

// External UDP RX buffer from main.cpp  
extern CircularBuffer* udpRxBuffer;

// Device 2 UART (when configured as Secondary UART)
UartInterface* device2Serial = nullptr;

// Sender task - processes all packet senders
void senderTask(void* parameter) {
    log_msg(LOG_INFO, "Sender task started on core %d", xPortGetCoreID());
    
    // Wait for pipeline initialization
    while (g_protocolPipeline == nullptr) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    log_msg(LOG_INFO, "Sender task: Pipeline ready, starting processing");
    
    
    // Main sender loop
    while (1) {
        
        if (g_protocolPipeline) {
            // Process all senders (USB, UDP, UART2, UART3)
            g_protocolPipeline->processSenders();
        }
        
        // Run at ~200Hz (5ms delay)
        //vTaskDelay(pdMS_TO_TICKS(5));
        // Run at ~250Hz (4ms delay) | TEMP
        vTaskDelay(pdMS_TO_TICKS(4));
    }
}

// UART Bridge Task - runs with high priority on Core 0
void uartBridgeTask(void* parameter) {
  // Wait for system initialization
  vTaskDelay(pdMS_TO_TICKS(1000));

  log_msg(LOG_INFO, "UART task started on core %d", xPortGetCoreID());

  // Dynamic buffer allocation based on baudrate
  const size_t adaptiveBufferSize = calculateAdaptiveBufferSize(config.baudrate);

  log_msg(LOG_INFO, "Adaptive buffering: %zu bytes (for %u baud). Thresholds: 200μs/1ms/5ms/15ms", 
          adaptiveBufferSize, config.baudrate);


  // Adaptive buffering variables
  unsigned long lastByteTime = 0;
  unsigned long bufferStartTime = 0;

  // Timing variables
  unsigned long lastWifiYield = 0;

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
  bool device2IsUART2 = ((config.device2.role == D2_UART2 || 
                        config.device2.role == D2_SBUS_IN || 
                        config.device2.role == D2_SBUS_OUT) && device2Serial);
  bool device3IsBridge = (config.device3.role == D3_UART3_BRIDGE);

  // Initialize BridgeContext
  BridgeContext ctx;
  initBridgeContext(&ctx,
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
    &lastWifiYield, &lastDropLog,
    // System
    &bridgeMode, &config
  );

  // Set bridge context for diagnostics
  setBridgeContext(&ctx);
  
  // Add UDP RX buffer to context
  ctx.buffers.udpRxBuffer = udpRxBuffer;
  
  // Create protocol statistics BEFORE pipeline initialization
  ctx.protocol.stats = new ProtocolStats();
  log_msg(LOG_INFO, "Protocol statistics created");

  // Initialize protocol buffers based on configuration
  initProtocolBuffers(&ctx, &config);
  
  // Initialize adaptive buffer timing
  initAdaptiveBuffer(&ctx, adaptiveBufferSize);

  // Initialize protocol pipeline
  ctx.protocolPipeline = new ProtocolPipeline(&ctx);
  ctx.protocolPipeline->init(&config);

  // ADD: Save pipeline pointer for sender task
  g_protocolPipeline = ctx.protocolPipeline;
  
  log_msg(LOG_INFO, "UART Bridge Task started");
  log_msg(LOG_DEBUG, "Device optimization: D2 USB=%d, D2 UART2=%d, D3 Active=%d, D3 Bridge=%d", 
          device2IsUSB, device2IsUART2, device3Active, device3IsBridge);

  // Main loop performance diagnostics (KEEP - important metric)
  static uint32_t loopCounter = 0;
  static uint32_t lastReport = 0;

  while (1) {
    loopCounter++;
    
    // Report every second
    if (millis() - lastReport > 1000) {
        log_msg(LOG_INFO, "Main loop: %u iterations/sec", loopCounter);
        loopCounter = 0;
        lastReport = millis();
    }

    // Poll Device 2 UART if configured
    if (device2IsUART2 && device2Serial) {
      static_cast<UartDMA*>(device2Serial)->pollEvents();
    }

    // Yield CPU time to WiFi stack periodically in network mode
    if (shouldYieldToWiFi(&ctx, bridgeMode)) {
      vTaskDelay(pdMS_TO_TICKS(5));
    }

    
    // === TEMPORARY DIAGNOSTIC BLOCK START ===
    // Performance profiling - measure all phases to find bottlenecks
    static uint32_t timings[10] = {0};
    static uint32_t counter = 0;

    uint32_t t0 = micros();
    // === TEMPORARY DIAGNOSTIC BLOCK END ===
    
    processDevice1Input(&ctx);
    
    // === TEMPORARY DIAGNOSTIC BLOCK START ===
    uint32_t t1 = micros();
    // === TEMPORARY DIAGNOSTIC BLOCK END ===
    
    // Process Device 2 input (USB or UART2)  
    if (device2IsUSB) {
        processDevice2USB(&ctx);
    } else if (device2IsUART2) {
        processDevice2UART(&ctx);
    }

    // Process Device 3 input (Bridge mode only)
    if (device3IsBridge && device3Serial) {
        processDevice3UART(&ctx);
    }

    // Process Device 4 input (UDP Bridge mode only)
    if (ctx.buffers.udpRxBuffer) {
        processDevice4UDP(&ctx);
    }
    
    // Process input flows through bidirectional pipeline
    if (ctx.protocolPipeline) {
        // Only process input if there's data (optimization)
        if (ctx.protocolPipeline->hasInputData()) {
            ctx.protocolPipeline->processInputFlows();
        }
    }
    
    // === TEMPORARY DIAGNOSTIC BLOCK START ===
    uint32_t t2 = micros();
    // === TEMPORARY DIAGNOSTIC BLOCK END ===
        
    // Always process telemetry (FC->GCS is critical)
    if (ctx.protocolPipeline) {
        ctx.protocolPipeline->processTelemetryFlow();
    }
    
    // === TEMPORARY DIAGNOSTIC BLOCK START ===
    uint32_t t3 = micros();
    // === TEMPORARY DIAGNOSTIC BLOCK END ===
    
    // Process UART1 TX queue (CRITICAL for single-writer mechanism)
    Uart1TxService::getInstance()->processTxQueue();
    
    // === TEMPORARY DIAGNOSTIC BLOCK START ===
    uint32_t t4 = micros();

    // Accumulate averages
    timings[0] += (t1 - t0); // Device1 input
    timings[1] += (t2 - t1); // Input flows
    timings[2] += (t3 - t2); // Telemetry flow
    timings[3] += (t4 - t3); // TX queue
    timings[4] += (t4 - t0); // Total

    if (++counter >= 1000) {
        log_msg(LOG_INFO, "[PROFILE] D1in=%u InputF=%u TelF=%u TxQ=%u Total=%u us",
                timings[0]/1000, timings[1]/1000, timings[2]/1000, 
                timings[3]/1000, timings[4]/1000);
        memset(timings, 0, sizeof(timings));
        counter = 0;
    }
    // === TEMPORARY DIAGNOSTIC BLOCK END ===
    
    
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

    // Fixed delay for multi-core systems (always 1ms)
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  
  // This point is never reached in normal operation, but cleanup if we get here
  if (ctx.protocolPipeline) {
    delete ctx.protocolPipeline;
    ctx.protocolPipeline = nullptr;
  }
  
  // Cleanup protocol buffers
  freeProtocolBuffers(&ctx);
  
  // Cleanup adaptive buffer timing
  cleanupAdaptiveBuffer(&ctx);
}

