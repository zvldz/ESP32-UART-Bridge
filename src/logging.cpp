#include "logging.h"
#include <Arduino.h>
#include "types.h"
#include "usb_interface.h"

// External mutex and flags
extern SemaphoreHandle_t logMutex;
extern const int DEBUG_MODE;
extern UsbMode usbMode;

// Logging system internal data
static String logBuffer[LOG_BUFFER_SIZE];
static int logIndex = 0;
static int logCount = 0;
static bool initialized = false;

// Initialize logging system
void logging_init() {
  if (!initialized) {
    for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
      logBuffer[i] = "";
      logBuffer[i].reserve(128);  // Pre-allocate memory
    }
    initialized = true;
  }
  logIndex = 0;
  logCount = 0;
}

// Thread-safe log with optional Serial output
void log_msg(String message) {
  if (!message || message.length() == 0) return;

  if (xSemaphoreTake(logMutex, 0) == pdTRUE) {
    if (message.length() > 120) {
      message = message.substring(0, 120) + "...";
    }

    unsigned long uptimeMs = millis();
    unsigned long seconds = uptimeMs / 1000;
    unsigned long ms = uptimeMs % 1000;
    String timestamp = "[" + String(seconds) + "." +
                      String(ms / 100) + "s] ";

    logBuffer[logIndex].clear();
    logBuffer[logIndex] = timestamp + message;

    logIndex = (logIndex + 1) % LOG_BUFFER_SIZE;
    if (logCount < LOG_BUFFER_SIZE) {
      logCount++;
    }
    xSemaphoreGive(logMutex);
  }

  // Conditionally forward log to Serial
  if (DEBUG_MODE == 1 && usbMode != USB_MODE_HOST) {
    Serial.println(message);
  } else if (DEBUG_MODE == 1 && usbMode == USB_MODE_HOST) {
    // Serial output suppressed in USB Host mode
    // Could add web-only logging indicator here if needed
  }
}

// Return logs for web interface
void logging_get_recent_logs(String* buffer, int maxCount, int* actualCount) {
  if (xSemaphoreTake(logMutex, 100) == pdTRUE) {
    *actualCount = 0;
    int maxEntries = min(maxCount, logCount);

    int startIndex;
    if (logCount < LOG_BUFFER_SIZE) {
      startIndex = max(0, logCount - maxEntries);
    } else {
      startIndex = (logIndex - maxEntries + LOG_BUFFER_SIZE) % LOG_BUFFER_SIZE;
    }

    for (int i = 0; i < maxEntries; i++) {
      int index = (startIndex + i) % LOG_BUFFER_SIZE;
      if (logBuffer[index].length() > 0) {
        buffer[*actualCount] = logBuffer[index];
        (*actualCount)++;
      }
    }

    xSemaphoreGive(logMutex);
  }
}

// Clear all logs
void logging_clear() {
  if (xSemaphoreTake(logMutex, 100) == pdTRUE) {
    logIndex = 0;
    logCount = 0;
    for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
      logBuffer[i].clear();
    }
    xSemaphoreGive(logMutex);
  }
}