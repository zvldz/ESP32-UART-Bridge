#include "logging.h"
#include <Arduino.h>
#include "types.h"
#include "usb_interface.h"

// External mutex
extern SemaphoreHandle_t logMutex;

// Logging system internal data
static String logBuffer[LOG_BUFFER_SIZE];
static int logIndex = 0;
static int logCount = 0;
static bool initialized = false;

// Global logging configuration
static LogConfig logConfig;

// Get configuration for external access
LogConfig* logging_get_config() {
    return &logConfig;
}

// Helper function to get short level name
const char* getLogLevelName(LogLevel level) {
    switch(level) {
        case LOG_ERROR:   return "ERR";
        case LOG_WARNING: return "WRN";  
        case LOG_INFO:    return "INF";
        case LOG_DEBUG:   return "DBG";
        default:          return "???";
    }
}

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

// Main logging function with log level
void log_msg(String message, LogLevel level) {
  if (!message || message.length() == 0) return;

  // Always save to buffer for web interface
  if (xSemaphoreTake(logMutex, 0) == pdTRUE) {
    if (message.length() > 120) {
      message = message.substring(0, 120) + "...";
    }

    unsigned long uptimeMs = millis();
    unsigned long seconds = uptimeMs / 1000;
    unsigned long ms = uptimeMs % 1000;
    
    // Add level to timestamp
    String timestamp = "[" + String(seconds) + "." +
                      String(ms / 100) + "s][" + 
                      String(getLogLevelName(level)) + "] ";

    logBuffer[logIndex].clear();
    logBuffer[logIndex] = timestamp + message;

    logIndex = (logIndex + 1) % LOG_BUFFER_SIZE;
    if (logCount < LOG_BUFFER_SIZE) {
      logCount++;
    }
    xSemaphoreGive(logMutex);
  }

  // TODO: Add UART output (Device 3) when role system implemented
  // if (logConfig.uartEnabled && level <= logConfig.uartLevel) {
  //     // Output to UART on GPIO 11
  // }
  
  // TODO: Add Network output (Device 4) when role system implemented  
  // if (logConfig.networkEnabled && level <= logConfig.networkLevel) {
  //     // Send via UDP
  // }
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