#include "web_ota.h"
#include "web_interface.h"
#include "logging.h"
#include "uart_interface.h"
#include <Update.h>
#include <WebServer.h>

// External objects from main.cpp
extern UartInterface* uartBridgeSerial;
extern TaskHandle_t uartBridgeTaskHandle;
extern TaskHandle_t device3TaskHandle;

// External Device 3 UART interface from uartbridge.cpp
extern UartInterface* device3Serial;

// Handle OTA update
void handleOTA() {
  WebServer* server = getWebServer();
  if (!server) return;

  HTTPUpload& upload = server->upload();

  if (upload.status == UPLOAD_FILE_START) {
    log_msg("Firmware update started: " + upload.filename, LOG_INFO);

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

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // Flash firmware chunk
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      log_msg("Firmware write failed: " + String(Update.errorString()), LOG_ERROR);
      return;
    }

    // Show progress occasionally
    static unsigned long lastProgress = 0;
    unsigned long progress = Update.progress();
    if (progress - lastProgress > 50000) { // Log every 50KB
      log_msg("Firmware update progress: " + String(progress) + " bytes", LOG_DEBUG);
      lastProgress = progress;
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) { // True to set the size to the current progress
      log_msg("Firmware update successful: " + String(upload.totalSize) + " bytes", LOG_INFO);
      log_msg("Rebooting device...", LOG_INFO);
    } else {
      log_msg("Firmware update failed at end: " + String(Update.errorString()), LOG_ERROR);
      
      // Resume tasks if update failed
      if (uartBridgeTaskHandle) {
        vTaskResume(uartBridgeTaskHandle);
      }
      if (device3TaskHandle) {
        vTaskResume(device3TaskHandle);
      }
    }

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.end();
    log_msg("Firmware update aborted", LOG_WARNING);
    
    // Resume tasks after abort
    if (uartBridgeTaskHandle) {
      vTaskResume(uartBridgeTaskHandle);
    }
    if (device3TaskHandle) {
      vTaskResume(device3TaskHandle);
    }
  }
}

// Handle update end
void handleUpdateEnd() {
  WebServer* server = getWebServer();
  if (!server) return;

  server->sendHeader("Connection", "close");
  
  if (Update.hasError()) {
    server->send(500, "text/plain", "Update failed: " + String(Update.errorString()));
    
    // Resume tasks after failed update
    if (uartBridgeTaskHandle) {
      vTaskResume(uartBridgeTaskHandle);
    }
    if (device3TaskHandle) {
      vTaskResume(device3TaskHandle);
    }
  } else {
    server->send(200, "text/plain", "Update successful! Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(500)); // Give time for response to be sent
    ESP.restart();
  }
}