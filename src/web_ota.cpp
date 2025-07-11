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
    log_msg("Firmware update started: " + upload.filename);

    // Stop UART bridge during update
    uartBridgeSerial.end();

    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { // Start with max available size
      log_msg("ERROR: Failed to begin firmware update");
      return;
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // Flash firmware chunk
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      log_msg("ERROR: Firmware write failed");
      return;
    }

    // Show progress occasionally
    static unsigned long lastProgress = 0;
    unsigned long progress = Update.progress();
    if (progress - lastProgress > 50000) { // Log every 50KB
      log_msg("Firmware update progress: " + String(progress) + " bytes");
      lastProgress = progress;
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) { // True to set the size to the current progress
      log_msg("Firmware update successful: " + String(upload.totalSize) + " bytes");
      log_msg("Rebooting device...");
    } else {
      log_msg("ERROR: Firmware update failed at end");
    }

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.end();
    log_msg("Firmware update aborted");
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