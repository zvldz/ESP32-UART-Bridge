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

// Device 3 UART (for Mirror/Bridge modes)
UartInterface* device3Serial = nullptr;

// Shared buffers for Device 3 operations
uint8_t device3TxBuffer[DEVICE3_UART_BUFFER_SIZE];
uint8_t device3RxBuffer[DEVICE3_UART_BUFFER_SIZE];
// Buffer indices protected by mutex - no need for volatile
int device3TxHead = 0;
int device3TxTail = 0;
int device3RxHead = 0;
int device3RxTail = 0;

// Mutex for Device 3 buffer access
SemaphoreHandle_t device3Mutex = nullptr;

// Device 4 log buffer
uint8_t device4LogBuffer[DEVICE4_LOG_BUFFER_SIZE];
int device4LogHead = 0;
int device4LogTail = 0;
SemaphoreHandle_t device4LogMutex = nullptr;

// Device 4 statistics
unsigned long globalDevice4TxBytes = 0;
unsigned long globalDevice4TxPackets = 0;
unsigned long globalDevice4RxBytes = 0;
unsigned long globalDevice4RxPackets = 0;

// AsyncUDP instance
AsyncUDP* device4UDP = nullptr;

// Device 4 Bridge buffers (only if Bridge mode is used)
uint8_t device4BridgeTxBuffer[DEVICE4_BRIDGE_BUFFER_SIZE];
uint8_t device4BridgeRxBuffer[DEVICE4_BRIDGE_BUFFER_SIZE];
int device4BridgeTxHead = 0;
int device4BridgeTxTail = 0;
int device4BridgeRxHead = 0;
int device4BridgeRxTail = 0;
SemaphoreHandle_t device4BridgeMutex = nullptr;

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

void updateDevice3Stats() {
    // This function is called from TaskScheduler for Device 3 stats
    // The actual update happens in device3Task() itself
}

// Device 3 Task - runs on Core 0 with UART task
void device3Task(void* parameter) {
  // Wait for system initialization
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  log_msg("Device 3 task started on core " + String(xPortGetCoreID()), LOG_INFO);
  
  // Local statistics counters
  unsigned long localDevice3TxBytes = 0;
  unsigned long localDevice3RxBytes = 0;
  
  // Rate limiting for LED notifications
  unsigned long lastDevice3TxLed = 0;
  unsigned long lastDevice3RxLed = 0;
  const unsigned long LED_NOTIFY_INTERVAL = 10;
  
  // Buffer for batch operations
  const int UART_BLOCK_SIZE = 64;
  uint8_t readBuffer[UART_BLOCK_SIZE];
  
  while (1) {
    // Poll DMA events for Device 3
    if (device3Serial) {
      static_cast<UartDMA*>(device3Serial)->pollEvents();
    }
    
    if (config.device3.role == D3_UART3_MIRROR) {
      // Mirror mode - copy data from buffer to Device 3 TX
      if (xSemaphoreTake(device3Mutex, 0) == pdTRUE) {
        // Process in blocks for efficiency
        int bytesToWrite = 0;
        uint8_t writeBuffer[UART_BLOCK_SIZE];
        
        while (device3TxHead != device3TxTail && bytesToWrite < UART_BLOCK_SIZE) {
          writeBuffer[bytesToWrite++] = device3TxBuffer[device3TxTail];
          device3TxTail = (device3TxTail + 1) % DEVICE3_UART_BUFFER_SIZE;
        }
        
        xSemaphoreGive(device3Mutex);
        
        // Write collected data
        if (bytesToWrite > 0 && device3Serial) {
          int written = device3Serial->write(writeBuffer, bytesToWrite);
          localDevice3TxBytes += written;
          
          // LED notification with rate limiting
          if (millis() - lastDevice3TxLed > LED_NOTIFY_INTERVAL) {
            led_notify_device3_tx();
            lastDevice3TxLed = millis();
          }
        }
      }
    }
    else if (config.device3.role == D3_UART3_BRIDGE) {
      // Bridge mode - bidirectional data transfer
      
      // Device 3 RX -> Buffer (to be sent to Device 1)
      if (device3Serial && device3Serial->available()) {
        if (xSemaphoreTake(device3Mutex, 0) == pdTRUE) {
          // Read in blocks for efficiency
          int toRead = min(device3Serial->available(), UART_BLOCK_SIZE);
          toRead = min(toRead, (int)sizeof(readBuffer));
          
          int actuallyRead = 0;
          for (int i = 0; i < toRead; i++) {
            int byte = device3Serial->read();
            if (byte >= 0) {
              readBuffer[actuallyRead++] = (uint8_t)byte;
            }
          }
          
          // Store in circular buffer
          for (int i = 0; i < actuallyRead; i++) {
            int nextHead = (device3RxHead + 1) % DEVICE3_UART_BUFFER_SIZE;
            if (nextHead != device3RxTail) {  // Buffer not full
              device3RxBuffer[device3RxHead] = readBuffer[i];
              device3RxHead = nextHead;
              localDevice3RxBytes++;
            } else {
              // Buffer full
              log_msg("Device 3 RX buffer full, dropping data", LOG_WARNING);
              break;
            }
          }
          
          xSemaphoreGive(device3Mutex);
          
          // LED notification with rate limiting
          if (actuallyRead > 0 && millis() - lastDevice3RxLed > LED_NOTIFY_INTERVAL) {
            led_notify_device3_rx();
            lastDevice3RxLed = millis();
          }
        }
      }
      
      // Buffer -> Device 3 TX (from Device 1)
      if (xSemaphoreTake(device3Mutex, 0) == pdTRUE) {
        // Process in blocks for efficiency
        int bytesToWrite = 0;
        uint8_t writeBuffer[UART_BLOCK_SIZE];
        
        while (device3TxHead != device3TxTail && bytesToWrite < UART_BLOCK_SIZE) {
          writeBuffer[bytesToWrite++] = device3TxBuffer[device3TxTail];
          device3TxTail = (device3TxTail + 1) % DEVICE3_UART_BUFFER_SIZE;
        }
        
        xSemaphoreGive(device3Mutex);
        
        // Write collected data
        if (bytesToWrite > 0 && device3Serial) {
          int written = device3Serial->write(writeBuffer, bytesToWrite);
          localDevice3TxBytes += written;
          
          // LED notification
          if (millis() - lastDevice3TxLed > LED_NOTIFY_INTERVAL) {
            led_notify_device3_tx();
            lastDevice3TxLed = millis();
          }
        }
      }
    }
    
    // Update statistics - called by TaskScheduler instead
    enterStatsCritical();
    uartStats.device3TxBytes = localDevice3TxBytes;
    uartStats.device3RxBytes = localDevice3RxBytes;
    exitStatsCritical();
    
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// UART Bridge Task - runs with high priority on Core 0
void uartBridgeTask(void* parameter) {
  // Wait for system initialization
  vTaskDelay(pdMS_TO_TICKS(1000));

  log_msg("UART task started on core " + String(xPortGetCoreID()), LOG_INFO);

  // Dynamic buffer allocation based on baudrate
  const size_t adaptiveBufferSize = calculateAdaptiveBufferSize(config.baudrate);
  uint8_t* adaptiveBuffer = (uint8_t*)pvPortMalloc(adaptiveBufferSize);
  if (!adaptiveBuffer) {
    log_msg("Failed to allocate adaptive buffer!", LOG_ERROR);
    vTaskDelete(NULL);
    return;
  }

  log_msg("Adaptive buffering: " + String(adaptiveBufferSize) + 
          " bytes (for " + String(config.baudrate) + " baud). " +
          "Thresholds: 200μs/1ms/5ms/15ms", LOG_INFO);

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

  log_msg("UART Bridge Task started", LOG_INFO);
  log_msg("Device optimization: D2 USB=" + String(device2IsUSB) + ", D2 UART2=" + String(device2IsUART2) + 
          ", D3 Active=" + String(device3Active) + ", D3 Bridge=" + String(device3IsBridge), LOG_DEBUG);

  // ===== TEMPORARY DIAGNOSTIC CODE =====
  // Added to diagnose FIFO overflow issue 
  /*
  static unsigned long maxProcessTime = 0;
  static unsigned long totalProcessTime = 0;
  static unsigned long processCount = 0;
  */
  // ===== END TEMPORARY DIAGNOSTIC CODE =====

  while (1) {
    // Poll Device 2 UART if configured
    if (device2IsUART2 && device2Serial) {
      static_cast<UartDMA*>(device2Serial)->pollEvents();
    }

    // Yield CPU time to WiFi stack periodically in network mode
    if (shouldYieldToWiFi(&ctx, bridgeMode)) {
      vTaskDelay(pdMS_TO_TICKS(5));
    }

    // ===== TEMPORARY DIAGNOSTIC CODE =====
    /*
    unsigned long processStart = micros();
    */
    // ===== END TEMPORARY DIAGNOSTIC CODE =====

    processDevice1Input(&ctx);

    // ===== TEMPORARY DIAGNOSTIC CODE =====
    /*    
    unsigned long processTime = micros() - processStart;

    totalProcessTime += processTime;
    processCount++;
    if (processTime > maxProcessTime) {
        maxProcessTime = processTime;
    }

    static unsigned long lastProcessLog = 0;
    if (millis() - lastProcessLog > 5000 && processCount > 0) {
        log_msg("[TEMP DIAG] processDevice1Input: avg=" + String(totalProcessTime / processCount) + 
                "us, max=" + String(maxProcessTime) + "us, count=" + String(processCount), LOG_WARNING);
        maxProcessTime = 0;
        totalProcessTime = 0;
        processCount = 0;
        lastProcessLog = millis();
    }
    */
    // ===== END TEMPORARY DIAGNOSTIC CODE =====
    
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

    // Fixed delay for multi-core systems (always 1ms)
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  
  // This point is never reached in normal operation
  // But if task ever exits, free the allocated buffer
  vPortFree(adaptiveBuffer);
}

void device4Task(void* parameter) {
    log_msg("Device 4 task started on core " + String(xPortGetCoreID()), LOG_INFO);
    
    // Wait for network ready with timeout
    const uint32_t maxWaitTime = 3000;  // 3 seconds
    uint32_t waited = 0;
    
    while (!systemState.networkActive && waited < maxWaitTime) {
        vTaskDelay(pdMS_TO_TICKS(100));
        waited += 100;
    }
    
    if (!systemState.networkActive) {
        log_msg("Device 4: Network not ready after 3s, exiting", LOG_ERROR);
        vTaskDelete(NULL);
        return;
    }
    
    // Additional delay for WiFi stack stabilization
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    log_msg("Device 4: Network ready, initializing AsyncUDP", LOG_INFO);
    
    // Create AsyncUDP instance
    device4UDP = new AsyncUDP();
    if (!device4UDP) {
        log_msg("Device 4: Failed to create AsyncUDP", LOG_ERROR);
        vTaskDelete(NULL);
        return;
    }
    
    // Create Bridge mutex if needed
    if (config.device4.role == D4_NETWORK_BRIDGE) {
        device4BridgeMutex = xSemaphoreCreateMutex();
        if (!device4BridgeMutex) {
            log_msg("Device 4: Failed to create bridge mutex", LOG_ERROR);
            delete device4UDP;
            vTaskDelete(NULL);
            return;
        }
    }
    
    // Determine broadcast or unicast
    bool isBroadcast = (strcmp(config.device4_config.target_ip, "192.168.4.255") == 0) ||
                       (strstr(config.device4_config.target_ip, ".255") != nullptr);
    
    // Setup listener for Bridge mode
    if (config.device4.role == D4_NETWORK_BRIDGE) {
        if (!device4UDP->listen(config.device4_config.port)) {
            log_msg("Device 4: Failed to listen on port " + 
                    String(config.device4_config.port), LOG_ERROR);
        } else {
            log_msg("Device 4: Listening on port " + 
                    String(config.device4_config.port), LOG_INFO);
            
            device4UDP->onPacket([](AsyncUDPPacket packet) {
                if (config.device4.role == D4_NETWORK_BRIDGE && device4BridgeMutex) {
                    if (xSemaphoreTake(device4BridgeMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        size_t len = packet.length();
                        uint8_t* data = packet.data();
                        
                        // Store incoming UDP data in Bridge RX buffer
                        for (size_t i = 0; i < len; i++) {
                            int nextHead = (device4BridgeRxHead + 1) % DEVICE4_BRIDGE_BUFFER_SIZE;
                            if (nextHead != device4BridgeRxTail) {  // Buffer not full
                                device4BridgeRxBuffer[device4BridgeRxHead] = data[i];
                                device4BridgeRxHead = nextHead;
                            } else {
                                // Buffer full, drop packet
                                log_msg("Device 4: Bridge RX buffer full, dropping packet", LOG_WARNING);
                                break;
                            }
                        }
                        
                        xSemaphoreGive(device4BridgeMutex);
                        
                        // Update statistics
                        globalDevice4RxBytes += len;
                        globalDevice4RxPackets++;
                    }
                }
            });
        }
    }
    
    // Main loop for log transmission and Bridge mode
    while (1) {
        // Logger mode: Check log buffer
        if (config.device4.role == D4_LOG_NETWORK && device4LogMutex) {
            if (xSemaphoreTake(device4LogMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                if (device4LogHead != device4LogTail) {
                    // Collect data from buffer
                    uint8_t tempBuffer[512];
                    int count = 0;
                    
                    while (device4LogHead != device4LogTail && count < sizeof(tempBuffer)) {
                        tempBuffer[count++] = device4LogBuffer[device4LogTail];
                        device4LogTail = (device4LogTail + 1) % DEVICE4_LOG_BUFFER_SIZE;
                    }
                    
                    xSemaphoreGive(device4LogMutex);
                    
                    // Send collected data
                    if (count > 0) {
                        size_t sent = 0;
                        
                        if (isBroadcast) {
                            sent = device4UDP->broadcastTo(tempBuffer, count, 
                                                          config.device4_config.port);
                        } else {
                            IPAddress targetIP;
                            if (targetIP.fromString(config.device4_config.target_ip)) {
                                sent = device4UDP->writeTo(tempBuffer, count, targetIP, 
                                                         config.device4_config.port);
                            }
                        }
                        
                        if (sent == count) {
                            globalDevice4TxBytes += count;
                            globalDevice4TxPackets++;
                        }
                    }
                } else {
                    xSemaphoreGive(device4LogMutex);
                }
            }
        }
        
        // Bridge mode: Check Bridge TX buffer and send UDP->UART data
        if (config.device4.role == D4_NETWORK_BRIDGE && device4BridgeMutex) {
            if (xSemaphoreTake(device4BridgeMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                // Send Bridge TX data via UDP
                if (device4BridgeTxHead != device4BridgeTxTail) {
                    uint8_t tempBuffer[512];
                    int count = 0;
                    
                    while (device4BridgeTxHead != device4BridgeTxTail && count < sizeof(tempBuffer)) {
                        tempBuffer[count++] = device4BridgeTxBuffer[device4BridgeTxTail];
                        device4BridgeTxTail = (device4BridgeTxTail + 1) % DEVICE4_BRIDGE_BUFFER_SIZE;
                    }
                    
                    xSemaphoreGive(device4BridgeMutex);
                    
                    // Send collected data via UDP
                    if (count > 0) {
                        size_t sent = 0;
                        
                        if (isBroadcast) {
                            sent = device4UDP->broadcastTo(tempBuffer, count, 
                                                          config.device4_config.port);
                        } else {
                            IPAddress targetIP;
                            if (targetIP.fromString(config.device4_config.target_ip)) {
                                sent = device4UDP->writeTo(tempBuffer, count, targetIP, 
                                                         config.device4_config.port);
                            }
                        }
                        
                        if (sent == count) {
                            globalDevice4TxBytes += count;
                            globalDevice4TxPackets++;
                        }
                    }
                } else {
                    xSemaphoreGive(device4BridgeMutex);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));  // 50ms for low latency
    }
}

void updateDevice4Stats() {
    enterStatsCritical();
    uartStats.device4TxBytes = globalDevice4TxBytes;
    uartStats.device4TxPackets = globalDevice4TxPackets;
    uartStats.device4RxBytes = globalDevice4RxBytes;
    uartStats.device4RxPackets = globalDevice4RxPackets;
    exitStatsCritical();
}