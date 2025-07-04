#include "logging.h"
#include <Arduino.h>

// External mutex created in main.cpp
extern SemaphoreHandle_t logMutex;
extern const int DEBUG_MODE;

// Logging system internal data
static String logBuffer[LOG_BUFFER_SIZE];
static int logIndex = 0;
static int logCount = 0;
static bool initialized = false;

// Initialize logging system
void logging_init() {
  // Ensure proper initialization to prevent heap fragmentation
  if (!initialized) {
    for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
      logBuffer[i] = "";
      logBuffer[i].reserve(128);  // Pre-allocate memory for each log entry
    }
    initialized = true;
  }
  logIndex = 0;
  logCount = 0;
}

// Thread-safe logging implementation
void log_msg(String message) {
  // Thread-safe logging
  if (xSemaphoreTake(logMutex, portMAX_DELAY) == pdTRUE) {
    // Limit message length to prevent memory issues
    if (message.length() > 120) {
      message = message.substring(0, 120) + "...";
    }
    
    // Add timestamp with uptime
    unsigned long uptimeMs = millis();
    unsigned long seconds = uptimeMs / 1000;
    unsigned long ms = uptimeMs % 1000;
    String timestamp = "[" + String(seconds) + "." + 
                      String(ms / 100) + "s] ";  // One decimal place
    
    // Clear old string before assignment to prevent memory fragmentation
    logBuffer[logIndex].clear();
    logBuffer[logIndex] = timestamp + message;
    
    logIndex = (logIndex + 1) % LOG_BUFFER_SIZE;
    if (logCount < LOG_BUFFER_SIZE) {
      logCount++;
    }
    xSemaphoreGive(logMutex);
  }
  
  // Duplicate to Serial in debug mode (without timestamp)
  if (DEBUG_MODE == 1) {
    Serial.println(message);
  }
}

// Get recent log entries for web interface
void logging_get_recent_logs(String* buffer, int maxCount, int* actualCount) {
  if (xSemaphoreTake(logMutex, 100) == pdTRUE) {
    *actualCount = 0;
    int maxEntries = min(maxCount, logCount);
    
    // Calculate starting index for last N entries
    int startIndex;
    if (logCount < LOG_BUFFER_SIZE) {
      // Buffer not full yet, start from beginning
      startIndex = max(0, logCount - maxEntries);
    } else {
      // Buffer is full, calculate from current position
      startIndex = (logIndex - maxEntries + LOG_BUFFER_SIZE) % LOG_BUFFER_SIZE;
    }
    
    // Copy entries to output buffer
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
    // Clear strings but keep reserved memory
    for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
      logBuffer[i].clear();
    }
    xSemaphoreGive(logMutex);
  }
}