#include "web_ota.h"
#include "web_interface.h"
#include "logging.h"
#include <Update.h>
#include <WebServer.h>

// External objects from main.cpp
extern HardwareSerial uartBridgeSerial;

// Handle OTA update
void handleOTA() {
  WebServer* server = getWebServer();
  if (!server) return;

  HTTPUpload& upload = server->upload();

  if (upload.status == UPLOAD_FILE_START) {
    log_msg("Firmware update started: " + upload.filename, LOG_INFO);

    // Stop UART bridge during update
    uartBridgeSerial.end();

    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { // Start with max available size
      log_msg("Failed to begin firmware update", LOG_ERROR);
      return;
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // Flash firmware chunk
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      log_msg("Firmware write failed", LOG_ERROR);
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
      log_msg("Firmware update failed at end", LOG_ERROR);
    }

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.end();
    log_msg("Firmware update aborted", LOG_WARNING);
  }
}

// Handle update end
void handleUpdateEnd() {
  WebServer* server = getWebServer();
  if (!server) return;

  server->sendHeader("Connection", "close");
  server->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  ESP.restart();
}