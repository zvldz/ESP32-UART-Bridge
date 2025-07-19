#include "uartbridge.h"
#include "logging.h"
#include "leds.h"
#include "defines.h"
#include "diagnostics.h"  // For updateSharedStats
#include <Arduino.h>
#include <esp_task_wdt.h>

// External objects from main.cpp
extern HardwareSerial uartBridgeSerial;
extern DeviceMode currentMode;
extern Config config;
extern UartStats uartStats;
extern FlowControlStatus flowControlStatus;

// USB interface pointer (set by uartbridge_init)
static UsbInterface* g_usbInterface = nullptr;

// Device 2 UART (when configured as Secondary UART)
static HardwareSerial* device2Serial = nullptr;

// Device 3 UART (for Mirror/Bridge modes)
static HardwareSerial* device3Serial = nullptr;

// Shared buffers for Device 3 operations
static uint8_t device3TxBuffer[DEVICE3_UART_BUFFER_SIZE];
static uint8_t device3RxBuffer[DEVICE3_UART_BUFFER_SIZE];
static volatile int device3TxHead = 0;
static volatile int device3TxTail = 0;
static volatile int device3RxHead = 0;
static volatile int device3RxTail = 0;

// Mutex for Device 3 buffer access
static SemaphoreHandle_t device3Mutex = nullptr;

// Initialize UART bridge
void uartbridge_init(HardwareSerial* serial, Config* config, UartStats* stats, UsbInterface* usb) {
  // Store USB interface pointer
  g_usbInterface = usb;
  
  // Configure UART with loaded settings
  uint32_t serialConfig = SERIAL_8N1;

  pinMode(UART_RX_PIN, INPUT_PULLUP);

  // Set data bits, parity, stop bits
  if (config->databits == 7) {
    serialConfig = config->stopbits == 2 ? SERIAL_7N2 : SERIAL_7N1;
  } else {
    serialConfig = config->stopbits == 2 ? SERIAL_8N2 : SERIAL_8N1;
  }

  if (config->parity == "even") {
    serialConfig = config->databits == 7 ?
      (config->stopbits == 2 ? SERIAL_7E2 : SERIAL_7E1) :
      (config->stopbits == 2 ? SERIAL_8E2 : SERIAL_8E1);
  } else if (config->parity == "odd") {
    serialConfig = config->databits == 7 ?
      (config->stopbits == 2 ? SERIAL_7O2 : SERIAL_7O1) :
      (config->stopbits == 2 ? SERIAL_8O2 : SERIAL_8O1);
  }

  serial->begin(config->baudrate, serialConfig, UART_RX_PIN, UART_TX_PIN);

  log_msg("UART configured: " + String(config->baudrate) + " baud, " +
          String(config->databits) + "d" + config->parity.substring(0,1) + String(config->stopbits), LOG_INFO);

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
  if (config->device3.role != D3_NONE && config->device3.role != D3_UART3_LOG) {
    initDevice3(config->device3.role);
  }
  
  // Create mutex for Device 3 operations
  if (device3Mutex == nullptr) {
    device3Mutex = xSemaphoreCreateMutex();
  }
}

// Initialize Device 2 as Secondary UART
void initDevice2UART() {
  device2Serial = new HardwareSerial(2);
  if (device2Serial) {
    uint32_t serialConfig = SERIAL_8N1;
    
    // Use same config as Device 1 for now
    // TODO: Allow independent configuration in future
    if (config.databits == 7) {
      serialConfig = config.stopbits == 2 ? SERIAL_7N2 : SERIAL_7N1;
    } else {
      serialConfig = config.stopbits == 2 ? SERIAL_8N2 : SERIAL_8N1;
    }
    
    if (config.parity == "even") {
      serialConfig = config.databits == 7 ?
        (config.stopbits == 2 ? SERIAL_7E2 : SERIAL_7E1) :
        (config.stopbits == 2 ? SERIAL_8E2 : SERIAL_8E1);
    } else if (config.parity == "odd") {
      serialConfig = config.databits == 7 ?
        (config.stopbits == 2 ? SERIAL_7O2 : SERIAL_7O1) :
        (config.stopbits == 2 ? SERIAL_8O2 : SERIAL_8O1);
    }
    
    device2Serial->begin(config.baudrate, serialConfig, DEVICE2_UART_RX_PIN, DEVICE2_UART_TX_PIN);
    log_msg("Device 2 UART initialized on GPIO" + String(DEVICE2_UART_RX_PIN) + "/" + 
            String(DEVICE2_UART_TX_PIN) + " at " + String(config.baudrate) + " baud", LOG_INFO);
  } else {
    log_msg("Failed to create Device 2 UART", LOG_ERROR);
  }
}

// Initialize Device 3 based on role
void initDevice3(uint8_t role) {
  device3Serial = new HardwareSerial(1);  // Use UART1 for Device 3
  if (device3Serial) {
    uint32_t serialConfig = SERIAL_8N1;
    
    // Use same config as Device 1 for now
    if (config.databits == 7) {
      serialConfig = config.stopbits == 2 ? SERIAL_7N2 : SERIAL_7N1;
    } else {
      serialConfig = config.stopbits == 2 ? SERIAL_8N2 : SERIAL_8N1;
    }
    
    if (config.parity == "even") {
      serialConfig = config.databits == 7 ?
        (config.stopbits == 2 ? SERIAL_7E2 : SERIAL_7E1) :
        (config.stopbits == 2 ? SERIAL_8E2 : SERIAL_8E1);
    } else if (config.parity == "odd") {
      serialConfig = config.databits == 7 ?
        (config.stopbits == 2 ? SERIAL_7O2 : SERIAL_7O1) :
        (config.stopbits == 2 ? SERIAL_8O2 : SERIAL_8O1);
    }
    
    if (role == D3_UART3_MIRROR) {
      // Mirror mode - TX only
      device3Serial->begin(config.baudrate, serialConfig, -1, DEVICE3_UART_TX_PIN);
      log_msg("Device 3 Mirror mode initialized on GPIO" + String(DEVICE3_UART_TX_PIN) + 
              " (TX only) at " + String(config.baudrate) + " baud", LOG_INFO);
    } else if (role == D3_UART3_BRIDGE) {
      // Bridge mode - full duplex
      device3Serial->begin(config.baudrate, serialConfig, DEVICE3_UART_RX_PIN, DEVICE3_UART_TX_PIN);
      log_msg("Device 3 Bridge mode initialized on GPIO" + String(DEVICE3_UART_RX_PIN) + "/" + 
              String(DEVICE3_UART_TX_PIN) + " at " + String(config.baudrate) + " baud", LOG_INFO);
    }
  } else {
    log_msg("Failed to create Device 3 UART", LOG_ERROR);
  }
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
  static unsigned long lastDevice3TxLed = 0;
  static unsigned long lastDevice3RxLed = 0;
  const unsigned long LED_NOTIFY_INTERVAL = 10;
  
  while (1) {
    if (config.device3.role == D3_UART3_MIRROR) {
      // Mirror mode - copy data from buffer to Device 3 TX
      if (xSemaphoreTake(device3Mutex, 0) == pdTRUE) {
        while (device3TxHead != device3TxTail) {
          if (device3Serial && device3Serial->availableForWrite() > 0) {
            device3Serial->write(device3TxBuffer[device3TxTail]);
            device3TxTail = (device3TxTail + 1) % DEVICE3_UART_BUFFER_SIZE;
            localDevice3TxBytes++;
            
            // LED notification with rate limiting
            if (millis() - lastDevice3TxLed > LED_NOTIFY_INTERVAL) {
              led_notify_device3_tx();
              lastDevice3TxLed = millis();
            }
          } else {
            break;  // No space in TX buffer, try again later
          }
        }
        xSemaphoreGive(device3Mutex);
      }
    }
    else if (config.device3.role == D3_UART3_BRIDGE) {
      // Bridge mode - bidirectional data transfer
      
      // Device 3 RX -> Buffer (to be sent to Device 1)
      if (device3Serial && device3Serial->available()) {
        if (xSemaphoreTake(device3Mutex, 0) == pdTRUE) {
          while (device3Serial->available()) {
            int nextHead = (device3RxHead + 1) % DEVICE3_UART_BUFFER_SIZE;
            if (nextHead != device3RxTail) {  // Buffer not full
              device3RxBuffer[device3RxHead] = device3Serial->read();
              device3RxHead = nextHead;
              localDevice3RxBytes++;
              
              // LED notification with rate limiting
              if (millis() - lastDevice3RxLed > LED_NOTIFY_INTERVAL) {
                led_notify_device3_rx();
                lastDevice3RxLed = millis();
              }
            } else {
              // Buffer full, drop data
              device3Serial->read();  // Read and discard
              log_msg("Device 3 RX buffer full, dropping data", LOG_INFO);
            }
          }
          xSemaphoreGive(device3Mutex);
        }
      }
      
      // Buffer -> Device 3 TX (from Device 1)
      if (xSemaphoreTake(device3Mutex, 0) == pdTRUE) {
        while (device3TxHead != device3TxTail) {
          if (device3Serial && device3Serial->availableForWrite() > 0) {
            device3Serial->write(device3TxBuffer[device3TxTail]);
            device3TxTail = (device3TxTail + 1) % DEVICE3_UART_BUFFER_SIZE;
            localDevice3TxBytes++;
            
            // LED notification
            if (millis() - lastDevice3TxLed > LED_NOTIFY_INTERVAL) {
              led_notify_device3_tx();
              lastDevice3TxLed = millis();
            }
          } else {
            break;
          }
        }
        xSemaphoreGive(device3Mutex);
      }
    }
    
    // Update statistics periodically (less frequently than main task)
    static unsigned long lastDevice3StatsUpdate = 0;
    if (millis() - lastDevice3StatsUpdate > UART_STATS_UPDATE_INTERVAL_MS * 2) {
      // Update only Device 3 statistics
      enterStatsCritical();
      uartStats.device3TxBytes = localDevice3TxBytes;
      uartStats.device3RxBytes = localDevice3RxBytes;
      exitStatsCritical();
      lastDevice3StatsUpdate = millis();
    }
    
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// UART Bridge Task - runs with high priority on Core 0
void uartBridgeTask(void* parameter) {
  // Wait for system initialization
  vTaskDelay(pdMS_TO_TICKS(1000));

  log_msg("UART task started on core " + String(xPortGetCoreID()), LOG_INFO);

  // Local counters for all devices
  unsigned long localDevice1RxBytes = 0;
  unsigned long localDevice1TxBytes = 0;
  unsigned long localDevice2RxBytes = 0;
  unsigned long localDevice2TxBytes = 0;
  unsigned long localDevice3RxBytes = 0;
  unsigned long localDevice3TxBytes = 0;
  unsigned long localLastActivity = 0;

  // Adaptive buffering variables (optimized for UART protocols)
  uint8_t adaptiveBuffer[UART_BUFFER_SIZE];  // Use defined constant
  int bufferIndex = 0;
  unsigned long lastByteTime = 0;
  unsigned long bufferStartTime = 0;

  // Timer for WiFi mode yielding (replaces counter)
  static unsigned long lastWifiYield = 0;

  // WDT protection
  static unsigned long lastWdtFeed = 0;

  // Diagnostic counters for dropped data
  static unsigned long droppedBytes = 0;
  static unsigned long totalDroppedBytes = 0;
  static unsigned long lastDropLog = 0;
  static unsigned long dropEvents = 0;

  // Packet size diagnostics
  static int maxDropSize = 0;
  static int timeoutDropSizes[10] = {0};
  static int timeoutDropIndex = 0;

  // Periodic statistics
  static unsigned long lastPeriodicStats = 0;

  // Rate limiting for LED notifications
  static unsigned long lastUartLedNotify = 0;
  static unsigned long lastUsbLedNotify = 0;
  const unsigned long LED_NOTIFY_INTERVAL = 10; // Minimum 10ms between LED notifications

  // Stack diagnostics
  static unsigned long lastStackCheck = 0;

  // Optimization: Cache device roles at start to avoid repeated checks
  bool device3Active = (config.device3.role == D3_UART3_MIRROR || config.device3.role == D3_UART3_BRIDGE);
  bool device2IsUSB = (config.device2.role == D2_USB && g_usbInterface);
  bool device2IsUART2 = (config.device2.role == D2_UART2 && device2Serial);
  bool device3IsBridge = (config.device3.role == D3_UART3_BRIDGE);

  log_msg("UART Bridge Task started", LOG_INFO);
  log_msg("Device optimization: D2 USB=" + String(device2IsUSB) + ", D2 UART2=" + String(device2IsUART2) + 
          ", D3 Active=" + String(device3Active) + ", D3 Bridge=" + String(device3IsBridge), LOG_DEBUG);

  while (1) {
    // Periodic statistics every 30 seconds - shows system is alive
    if (millis() - lastPeriodicStats > 30000) {
      String mode = (currentMode == MODE_CONFIG) ? "WiFi" : "Normal";
      log_msg("UART bridge alive [" + mode + " mode]: D1 RX=" + String(localDevice1RxBytes) +
              " TX=" + String(localDevice1TxBytes) + " bytes" +
              (totalDroppedBytes > 0 ? ", dropped=" + String(totalDroppedBytes) : ""), LOG_DEBUG);
      lastPeriodicStats = millis();
    }

    // Stack diagnostics every 5 seconds
    if (millis() - lastStackCheck > 5000) {
      UBaseType_t stackFree = uxTaskGetStackHighWaterMark(NULL);
      log_msg("UART task: Stack free=" + String(stackFree * 4) +
              " bytes, Heap free=" + String(ESP.getFreeHeap()) +
              " bytes, Largest block=" + String(ESP.getMaxAllocHeap()) + " bytes", LOG_DEBUG);
      lastStackCheck = millis();
    }

    // Yield CPU time to WiFi stack periodically in config mode
    if (currentMode == MODE_CONFIG && millis() - lastWifiYield > 50) {
      vTaskDelay(pdMS_TO_TICKS(5));   // Give time to WiFi stack
      lastWifiYield = millis();
    }

    // Device 1 UART → Device 2 (USB or UART2) - adaptive buffering
    unsigned long currentTime = micros();

    while (uartBridgeSerial.available()) {
      // Notify LED with rate limiting
      if (currentMode == MODE_NORMAL && millis() - lastUartLedNotify > LED_NOTIFY_INTERVAL) {
        led_notify_uart_rx();
        lastUartLedNotify = millis();
      }

      uint8_t data = uartBridgeSerial.read();
      localDevice1RxBytes++;
      
      // Copy to Device 3 if in Mirror or Bridge mode (using cached flag)
      if (device3Active) {
        if (xSemaphoreTake(device3Mutex, 0) == pdTRUE) {
          int nextHead = (device3TxHead + 1) % DEVICE3_UART_BUFFER_SIZE;
          if (nextHead != device3TxTail) {
            device3TxBuffer[device3TxHead] = data;
            device3TxHead = nextHead;
          }
          xSemaphoreGive(device3Mutex);
        }
      }

      // Route to Device 2 based on its role (using cached flags)
      if (device2IsUSB) {
        // Device 2 is USB - use adaptive buffering
        uartStats.totalUartPackets++;

        // Start buffer timing on first byte
        if (bufferIndex == 0) {
          bufferStartTime = currentTime;
        }

        // Add byte to buffer
        adaptiveBuffer[bufferIndex++] = data;
        lastByteTime = currentTime;

        // Determine if we should transmit the buffer
        bool shouldTransmit = false;
        unsigned long timeSinceLastByte = 0;
        unsigned long timeInBuffer = currentTime - bufferStartTime;

        // Calculate pause since last byte (only if we have previous timing)
        if (lastByteTime > 0 && !uartBridgeSerial.available()) {
          // Small delay to detect real pause vs processing delay
          delayMicroseconds(50);
          if (!uartBridgeSerial.available()) {
            timeSinceLastByte = micros() - lastByteTime;
          }
        }

        // Transmission decision logic (prioritized for low latency):

        // 1. Small critical packets (heartbeat, commands) - immediate transmission
        if (bufferIndex <= 12 && timeSinceLastByte >= 200) {
          shouldTransmit = true;
        }
        // 2. Medium packets (attitude, GPS) - quick transmission
        else if (bufferIndex <= 64 && timeSinceLastByte >= 1000) {
          shouldTransmit = true;
        }
        // 3. Natural packet boundary detected (works for any size)
        else if (timeSinceLastByte >= 5000) {
          shouldTransmit = true;
        }
        // 4. Emergency timeout (prevent excessive latency)
        else if (timeInBuffer >= 15000) {
          shouldTransmit = true;
        }
        // 5. Buffer safety limit (prevent overflow)
        else if (bufferIndex >= sizeof(adaptiveBuffer)) {
          shouldTransmit = true;
        }

        // Transmit accumulated data if conditions met
        if (shouldTransmit) {
          // Check USB buffer availability to prevent blocking
          int availableSpace = g_usbInterface->availableForWrite();

          if (availableSpace >= bufferIndex) {
            // Can write entire buffer
            g_usbInterface->write(adaptiveBuffer, bufferIndex);
            localDevice2TxBytes += bufferIndex;
            bufferIndex = 0;  // Reset buffer
          } else if (availableSpace > 0) {
            // USB buffer has some space - write what we can
            g_usbInterface->write(adaptiveBuffer, availableSpace);
            localDevice2TxBytes += availableSpace;
            // Move remaining data to front of buffer
            memmove(adaptiveBuffer, adaptiveBuffer + availableSpace, bufferIndex - availableSpace);
            bufferIndex -= availableSpace;
          } else {
            // No space available - USB buffer completely full
            if (bufferIndex >= sizeof(adaptiveBuffer)) {
              // Must drop data to prevent our buffer overflow
              droppedBytes += bufferIndex;
              totalDroppedBytes += bufferIndex;
              dropEvents++;

              // Track maximum dropped packet size
              if (bufferIndex > maxDropSize) {
                maxDropSize = bufferIndex;
              }

              bufferIndex = 0;  // Drop buffer to prevent overflow

              // Log periodically with statistics
              if (millis() - lastDropLog > 5000) {  // Every 5 seconds
                log_msg("USB buffer full: dropped " + String(droppedBytes) + " bytes in " +
                        String(dropEvents) + " events (total: " + String(totalDroppedBytes) +
                        " bytes), max packet: " + String(maxDropSize) + " bytes", LOG_INFO);
                lastDropLog = millis();
                droppedBytes = 0;
                dropEvents = 0;
                maxDropSize = 0;  // Reset for next period
              }
            }
            // If buffer not full yet, keep data and try again next iteration
          }
        }
      } else if (device2IsUART2) {
        // Device 2 is UART2 - direct byte transfer
        if (device2Serial->availableForWrite() > 0) {
          device2Serial->write(data);
          localDevice2TxBytes++;
        }
      }

      localLastActivity = millis();
    }
    
    // Device 3 Bridge mode - handle RX data from Device 3
    if (device3IsBridge) {
      if (xSemaphoreTake(device3Mutex, 0) == pdTRUE) {
        while (device3RxHead != device3RxTail) {
          if (uartBridgeSerial.availableForWrite() > 0) {
            uartBridgeSerial.write(device3RxBuffer[device3RxTail]);
            device3RxTail = (device3RxTail + 1) % DEVICE3_UART_BUFFER_SIZE;
            localDevice1TxBytes++;
          } else {
            break;  // No space in TX buffer
          }
        }
        xSemaphoreGive(device3Mutex);
      }
    }

    // Handle any remaining data in buffer (timeout-based flush) for USB
    if (device2IsUSB && bufferIndex > 0) {
      unsigned long timeInBuffer = micros() - bufferStartTime;
      if (timeInBuffer >= 15000) {  // 15ms emergency timeout
        int availableSpace = g_usbInterface->availableForWrite();
        if (availableSpace >= bufferIndex) {
          g_usbInterface->write(adaptiveBuffer, bufferIndex);
          localDevice2TxBytes += bufferIndex;
          bufferIndex = 0;
        } else if (availableSpace > 0) {
          // Write what we can
          g_usbInterface->write(adaptiveBuffer, availableSpace);
          localDevice2TxBytes += availableSpace;
          memmove(adaptiveBuffer, adaptiveBuffer + availableSpace, bufferIndex - availableSpace);
          bufferIndex -= availableSpace;
        } else {
          // Buffer stuck - drop data
          droppedBytes += bufferIndex;
          totalDroppedBytes += bufferIndex;
          dropEvents++;

          // Track timeout drop sizes
          timeoutDropSizes[timeoutDropIndex % 10] = bufferIndex;
          timeoutDropIndex++;

          bufferIndex = 0;

          // Log if haven't logged recently
          if (millis() - lastDropLog > 5000) {
            // Build string with last 10 timeout drop sizes
            String sizes = "Sizes: ";
            int sizeCount = 0;
            for(int i = 0; i < 10; i++) {
              if (timeoutDropSizes[i] > 0) {
                if (sizeCount > 0) sizes += ", ";
                sizes += String(timeoutDropSizes[i]);
                sizeCount++;
              }
            }

            log_msg("USB timeout: dropped " + String(droppedBytes) + " bytes in " +
                    String(dropEvents) + " events (total: " + String(totalDroppedBytes) +
                    " bytes). " + sizes, LOG_INFO);
            lastDropLog = millis();
            droppedBytes = 0;
            dropEvents = 0;
            // Clear timeout sizes for next period
            for(int i = 0; i < 10; i++) timeoutDropSizes[i] = 0;
          }
        }
      }
    }

    // Device 2 → Device 1 UART (USB or UART2 to main UART)
    if (device2IsUSB) {
      // USB → UART (immediate forwarding for commands)
      while (g_usbInterface->available()) {
        // Notify LED with rate limiting
        if (currentMode == MODE_NORMAL && millis() - lastUsbLedNotify > LED_NOTIFY_INTERVAL) {
          led_notify_usb_rx();
          lastUsbLedNotify = millis();
        }

        // Commands from GCS are critical - immediate byte-by-byte transfer
        int data = g_usbInterface->read();
        if (data >= 0) {
          uartBridgeSerial.write((uint8_t)data);
          localDevice1TxBytes++;
          localDevice2RxBytes++;
          localLastActivity = millis();
          
          // Also send to Device 3 if in Bridge mode (using cached flag)
          if (device3IsBridge) {
            if (device3Serial && device3Serial->availableForWrite() > 0) {
              device3Serial->write((uint8_t)data);
              // Note: Device3 TX bytes counted in device3Task
            }
          }
        }
      }
    } else if (device2IsUART2) {
      // UART2 → UART1
      while (device2Serial->available()) {
        uint8_t data = device2Serial->read();
        localDevice2RxBytes++;
        
        if (uartBridgeSerial.availableForWrite() > 0) {
          uartBridgeSerial.write(data);
          localDevice1TxBytes++;
          localLastActivity = millis();
        }
      }
    }

    // Update shared statistics periodically (interval defined by UART_STATS_UPDATE_INTERVAL_MS)
    static unsigned long lastStatsUpdate = 0;
    if (millis() - lastStatsUpdate > UART_STATS_UPDATE_INTERVAL_MS) {
      updateSharedStats(localDevice1RxBytes, localDevice1TxBytes,
                       localDevice2RxBytes, localDevice2TxBytes,
                       localDevice3RxBytes, localDevice3TxBytes,
                       localLastActivity);
      lastStatsUpdate = millis();
    }

    // Fixed delay for multi-core systems (always 1ms)
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// Detect flow control hardware presence
void detectFlowControl() {
  if (!config.flowcontrol) {
    flowControlStatus.flowControlDetected = false;
    flowControlStatus.flowControlActive = false;
    return;
  }

  // Configure RTS/CTS pins for testing
  pinMode(RTS_PIN, OUTPUT);
  pinMode(CTS_PIN, INPUT_PULLUP);

  // Test RTS/CTS functionality
  digitalWrite(RTS_PIN, HIGH);
  delay(1);
  bool ctsHigh = digitalRead(CTS_PIN);

  digitalWrite(RTS_PIN, LOW);
  delay(1);
  bool ctsLow = digitalRead(CTS_PIN);

  // If CTS responds to RTS changes, flow control is connected
  flowControlStatus.flowControlDetected = (ctsHigh != ctsLow);

  if (flowControlStatus.flowControlDetected) {
    // Configure UART with hardware flow control
    uartBridgeSerial.setPins(UART_RX_PIN, UART_TX_PIN, CTS_PIN, RTS_PIN);
    flowControlStatus.flowControlActive = true;
    log_msg("Flow control detected and activated", LOG_INFO);
  } else {
    log_msg("Flow control enabled but no RTS/CTS detected", LOG_WARNING);
    flowControlStatus.flowControlActive = false;
  }
}

// Get flow control status string
String getFlowControlStatus() {
  if (!config.flowcontrol) {
    return "Disabled";
  } else if (flowControlStatus.flowControlDetected && flowControlStatus.flowControlActive) {
    return "Enabled (Active)";
  } else {
    return "Enabled (No RTS/CTS detected)";
  }
}