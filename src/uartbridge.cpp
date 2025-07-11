#include "uartbridge.h"
#include "logging.h"
#include "leds.h"
#include "defines.h"
#include <Arduino.h>
#include <esp_task_wdt.h>

// External objects from main.cpp
extern HardwareSerial uartBridgeSerial;
extern DeviceMode currentMode;
extern const int DEBUG_MODE;
extern Config config;
extern UartStats uartStats;
extern FlowControlStatus flowControlStatus;

// Initialize UART bridge
void uartbridge_init(HardwareSerial* serial, Config* config, UartStats* stats) {
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
          String(config->databits) + "d" + config->parity.substring(0,1) + String(config->stopbits));

  // Detect flow control if enabled
  if (config->flowcontrol) {
    pinMode(CTS_PIN, INPUT_PULLUP);
    detectFlowControl();
  } else {
    pinMode(CTS_PIN, INPUT_PULLUP);
    pinMode(RTS_PIN, INPUT_PULLUP);
  }
}

// UART Bridge Task - runs with high priority (on Core 0 for multi-core systems)
void uartBridgeTask(void* parameter) {
  // wait init
  vTaskDelay(pdMS_TO_TICKS(1000));

  log_msg("UART task started on core " + String(xPortGetCoreID()));

  unsigned long localUartToUsb = 0;
  unsigned long localUsbToUart = 0;
  unsigned long localLastActivity = 0;

  // Adaptive buffering variables (optimized for UART protocols)
  uint8_t adaptiveBuffer[UART_BUFFER_SIZE];  // Use defined constant
  int bufferIndex = 0;
  unsigned long lastByteTime = 0;
  unsigned long bufferStartTime = 0;

  // Add counter for WiFi mode
  int wifiModeYieldCounter = 0;

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

  log_msg("UART Bridge Task started");

  while (1) {
    // Periodic statistics every 30 seconds - shows system is alive
    if (millis() - lastPeriodicStats > 30000) {
      String mode = (currentMode == MODE_CONFIG) ? "WiFi" : "Normal";
      log_msg("UART bridge alive [" + mode + " mode]: TX=" + String(localUartToUsb) +
              " bytes, RX=" + String(localUsbToUart) + " bytes" +
              (totalDroppedBytes > 0 ? ", dropped=" + String(totalDroppedBytes) : ""));
      lastPeriodicStats = millis();
    }

    // Stack diagnostics in debug mode only
    if (DEBUG_MODE == 1) {
      static unsigned long lastDiagnostic = 0;
      if (millis() - lastDiagnostic > 5000) {  // Every 5 seconds
        UBaseType_t stackFree = uxTaskGetStackHighWaterMark(NULL);
        log_msg("UART task: Stack free=" + String(stackFree * 4) +
                " bytes, Heap free=" + String(ESP.getFreeHeap()) +
                " bytes, Largest block=" + String(ESP.getMaxAllocHeap()) + " bytes");
        lastDiagnostic = millis();
      }
    }

    // Only run UART bridge in normal mode or config mode
    if (currentMode == MODE_NORMAL || currentMode == MODE_CONFIG) {

      // Add yield in WiFi mode
      if (currentMode == MODE_CONFIG) {
        wifiModeYieldCounter++;
        if (wifiModeYieldCounter >= 10) {  // Every 10 iterations
          vTaskDelay(pdMS_TO_TICKS(5));   // Give time to WiFi stack
          wifiModeYieldCounter = 0;
        }
      }

      // UART → USB (adaptive buffering optimized for protocol efficiency)
      unsigned long currentTime = micros();

      while (uartBridgeSerial.available()) {
        // Notify LED with rate limiting
        if (currentMode == MODE_NORMAL && millis() - lastUartLedNotify > LED_NOTIFY_INTERVAL) {
          led_notify_uart_rx();
          lastUartLedNotify = millis();
        }

        if (DEBUG_MODE == 1) {
          // In debug mode, read data for statistics but don't forward
          uartBridgeSerial.read();  // Read and discard
          localUartToUsb++;
          uartStats.totalUartPackets++;
        } else {
          // In production mode, use adaptive buffering
          uint8_t data = uartBridgeSerial.read();
          uartStats.totalUartPackets++;

          // Start buffer timing on first byte
          if (bufferIndex == 0) {
            bufferStartTime = currentTime;
          }

          // Add byte to buffer
          adaptiveBuffer[bufferIndex++] = data;
          lastByteTime = currentTime;
          localUartToUsb++;

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
            int availableSpace = Serial.availableForWrite();

            if (availableSpace >= bufferIndex) {
              // Can write entire buffer
              Serial.write(adaptiveBuffer, bufferIndex);
              bufferIndex = 0;  // Reset buffer
            } else if (availableSpace > 0) {
              // USB buffer has some space - write what we can
              Serial.write(adaptiveBuffer, availableSpace);
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
                          " bytes), max packet: " + String(maxDropSize) + " bytes");
                  lastDropLog = millis();
                  droppedBytes = 0;
                  dropEvents = 0;
                  maxDropSize = 0;  // Reset for next period
                }
              }
              // If buffer not full yet, keep data and try again next iteration
            }
          }
        }

        localLastActivity = millis();
      }

      // Handle any remaining data in buffer (timeout-based flush) - production mode only
      if (DEBUG_MODE == 0 && bufferIndex > 0) {
        unsigned long timeInBuffer = micros() - bufferStartTime;
        if (timeInBuffer >= 15000) {  // 15ms emergency timeout
          int availableSpace = Serial.availableForWrite();
          if (availableSpace >= bufferIndex) {
            Serial.write(adaptiveBuffer, bufferIndex);
            bufferIndex = 0;
          } else if (availableSpace > 0) {
            // Write what we can
            Serial.write(adaptiveBuffer, availableSpace);
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
                      " bytes). " + sizes);
              lastDropLog = millis();
              droppedBytes = 0;
              dropEvents = 0;
              // Clear timeout sizes for next period
              for(int i = 0; i < 10; i++) timeoutDropSizes[i] = 0;
            }
          }
        }
      }

      // USB → UART (immediate forwarding for commands) - production mode only
      if (DEBUG_MODE == 0) {
        while (Serial.available()) {
          // Notify LED with rate limiting
          if (currentMode == MODE_NORMAL && millis() - lastUsbLedNotify > LED_NOTIFY_INTERVAL) {
            led_notify_usb_rx();
            lastUsbLedNotify = millis();
          }

          // Commands from GCS are critical - immediate byte-by-byte transfer
          uint8_t data = Serial.read();
          uartBridgeSerial.write(data);
          localUsbToUart++;
          localLastActivity = millis();
        }
      }

      // Update shared statistics periodically (interval defined by UART_STATS_UPDATE_INTERVAL_MS)
      static unsigned long lastStatsUpdate = 0;
      if (millis() - lastStatsUpdate > UART_STATS_UPDATE_INTERVAL_MS) {
        updateSharedStats(localUartToUsb, localUsbToUart, localLastActivity);
        lastStatsUpdate = millis();
      }
    }

    // Adaptive delay based on core count and mode
    #ifdef CONFIG_FREERTOS_UNICORE
      if (currentMode == MODE_CONFIG) {
        vTaskDelay(pdMS_TO_TICKS(5));  // 5ms for WiFi mode on single core
      } else {
        vTaskDelay(pdMS_TO_TICKS(2));  // 2ms for normal mode on single core
      }
    #else
      vTaskDelay(pdMS_TO_TICKS(1));  // 1ms for multi-core systems
    #endif
  }
}

// Thread-safe function to update shared statistics using critical sections
void updateSharedStats(unsigned long uartToUsb, unsigned long usbToUart, unsigned long activity) {
  enterStatsCritical();
  uartStats.bytesUartToUsb = uartToUsb;
  uartStats.bytesUsbToUart = usbToUart;
  if (activity > 0) {
    uartStats.lastActivityTime = activity;
  }
  exitStatsCritical();
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
    log_msg("Flow control detected and activated");
  } else {
    log_msg("Flow control enabled but no RTS/CTS detected");
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

// Reset all statistics using critical sections
void resetStatistics(UartStats* stats) {
  enterStatsCritical();
  stats->bytesUartToUsb = 0;
  stats->bytesUsbToUart = 0;
  stats->lastActivityTime = 0;
  stats->deviceStartTime = millis();
  stats->totalUartPackets = 0;
  exitStatsCritical();
}