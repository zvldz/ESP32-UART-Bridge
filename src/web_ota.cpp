#include "web_ota.h"
#include "web_interface.h"
#include "logging.h"
#include "uart_interface.h"
#include "scheduler_tasks.h"
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
    log_msg(LOG_INFO, "Firmware update started: %s", filename.c_str());
    updateStarted = false;

    // Suspend UART tasks during update to prevent interference
    if (uartBridgeTaskHandle) {
      log_msg(LOG_DEBUG, "Suspending UART bridge task for OTA update");
      vTaskSuspend(uartBridgeTaskHandle);
    }
    
    // Also suspend Device 3 task if it exists
    if (device3TaskHandle) {
      log_msg(LOG_DEBUG, "Suspending Device 3 task for OTA update");
      vTaskSuspend(device3TaskHandle);
    }

    // Ensure all UART data is flushed before starting update
    if (uartBridgeSerial) {
      uartBridgeSerial->flush();
      vTaskDelay(pdMS_TO_TICKS(100)); // Give time for flush to complete
    }
    
    // IMPORTANT: Deinitialize Device 3 UART0 to prevent conflicts
    if (device3Serial) {
      log_msg(LOG_DEBUG, "Deinitializing Device 3 UART0 for clean OTA update");
      device3Serial->end();
      vTaskDelay(pdMS_TO_TICKS(50)); // Give time for UART to fully release
    }

    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { // Start with max available size
      log_msg(LOG_ERROR, "Failed to begin firmware update: %s", Update.errorString());
      
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
      log_msg(LOG_ERROR, "Firmware write failed: %s", Update.errorString());
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
      log_msg(LOG_DEBUG, "Firmware update progress: %lu bytes", progress);
      lastProgress = progress;
    }
  }

  if (final && updateStarted) {
    // File end - finalize update
    if (Update.end(true)) { // True to set the size to the current progress
      log_msg(LOG_INFO, "Firmware update successful: %zu bytes", index + len);
      log_msg(LOG_INFO, "Rebooting device...");
      updateStarted = false;
    } else {
      log_msg(LOG_ERROR, "Firmware update failed at end: %s", Update.errorString());
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
  if (Update.hasError()) {
    String errorMsg = "{\"status\":\"error\",\"message\":\"Update failed: " + String(Update.errorString()) + "\"}";
    request->send(400, "application/json", errorMsg);
    
    // Resume tasks after failed update
    if (uartBridgeTaskHandle) {
      vTaskResume(uartBridgeTaskHandle);
    }
    if (device3TaskHandle) {
      vTaskResume(device3TaskHandle);
    }
  } else {
    request->send(200, "application/json", 
      "{\"status\":\"ok\",\"message\":\"Firmware updated successfully! Device rebooting...\"}");
    
    scheduleReboot(3000);
  }
}