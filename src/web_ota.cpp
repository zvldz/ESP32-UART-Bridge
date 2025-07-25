#include "web_ota.h"
#include "web_interface.h"
#include "logging.h"
#include "uart_interface.h"
#include <Update.h>
#include <ESPAsyncWebServer.h>

// External objects from main.cpp
extern UartInterface* uartBridgeSerial;
extern TaskHandle_t uartBridgeTaskHandle;
extern TaskHandle_t device3TaskHandle;

// External Device 3 UART interface from uartbridge.cpp
extern UartInterface* device3Serial;

// Handle OTA update for async web server
void handleOTA(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  static bool updateStarted = false;
  
  if (!index) {
    // File start - this is the first chunk
    log_msg("Firmware update started: " + filename, LOG_INFO);
    updateStarted = false;

    // Suspend UART tasks during update to prevent interference
    if (uartBridgeTaskHandle) {
      log_msg("Suspending UART bridge task for OTA update", LOG_DEBUG);
      vTaskSuspend(uartBridgeTaskHandle);
    }
    
    // Also suspend Device 3 task if it exists
    if (device3TaskHandle) {
      log_msg("Suspending Device 3 task for OTA update", LOG_DEBUG);
      vTaskSuspend(device3TaskHandle);
    }

    // Ensure all UART data is flushed before starting update
    if (uartBridgeSerial) {
      uartBridgeSerial->flush();
      vTaskDelay(pdMS_TO_TICKS(100)); // Give time for flush to complete
    }
    
    // IMPORTANT: Deinitialize Device 3 UART0 to prevent conflicts
    if (device3Serial) {
      log_msg("Deinitializing Device 3 UART0 for clean OTA update", LOG_DEBUG);
      device3Serial->end();
      vTaskDelay(pdMS_TO_TICKS(50)); // Give time for UART to fully release
    }

    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { // Start with max available size
      log_msg("Failed to begin firmware update: " + String(Update.errorString()), LOG_ERROR);
      
      // Resume tasks if update failed to start
      if (uartBridgeTaskHandle) {
        vTaskResume(uartBridgeTaskHandle);
      }
      if (device3TaskHandle) {
        vTaskResume(device3TaskHandle);
      }
      return;
    }
    
    updateStarted = true;
  }

  if (len && updateStarted) {
    // Write firmware chunk
    if (Update.write(data, len) != len) {
      log_msg("Firmware write failed: " + String(Update.errorString()), LOG_ERROR);
      updateStarted = false;
      
      // Resume tasks on write failure
      if (uartBridgeTaskHandle) {
        vTaskResume(uartBridgeTaskHandle);
      }
      if (device3TaskHandle) {
        vTaskResume(device3TaskHandle);
      }
      return;
    }

    // Show progress occasionally
    static unsigned long lastProgress = 0;
    unsigned long progress = Update.progress();
    if (progress - lastProgress > 50000) { // Log every 50KB
      log_msg("Firmware update progress: " + String(progress) + " bytes", LOG_DEBUG);
      lastProgress = progress;
    }
  }

  if (final && updateStarted) {
    // File end - finalize update
    if (Update.end(true)) { // True to set the size to the current progress
      log_msg("Firmware update successful: " + String(index + len) + " bytes", LOG_INFO);
      log_msg("Rebooting device...", LOG_INFO);
      updateStarted = false;
    } else {
      log_msg("Firmware update failed at end: " + String(Update.errorString()), LOG_ERROR);
      updateStarted = false;
      
      // Resume tasks if update failed
      if (uartBridgeTaskHandle) {
        vTaskResume(uartBridgeTaskHandle);
      }
      if (device3TaskHandle) {
        vTaskResume(device3TaskHandle);
      }
    }
  }
}

// Handle update end for async web server
void handleUpdateEnd(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", Update.hasError() ? 
    ("Update failed: " + String(Update.errorString())) : 
    "Update successful! Rebooting...");
  
  if (Update.hasError()) {
    // Resume tasks after failed update
    if (uartBridgeTaskHandle) {
      vTaskResume(uartBridgeTaskHandle);
    }
    if (device3TaskHandle) {
      vTaskResume(device3TaskHandle);
    }
  } else {
    vTaskDelay(pdMS_TO_TICKS(500)); // Give time for response to be sent
    ESP.restart();
  }
}