#include "logging.h"
#include <Arduino.h>
#include "types.h"
#include "usb_interface.h"
#include "config.h"
#include "uart_interface.h"
#include "uart_dma.h"
#include "defines.h"

// External objects
extern SemaphoreHandle_t logMutex;
extern Config config;

// Logging system internal data
static String logBuffer[LOG_BUFFER_SIZE];
static int logIndex = 0;
static int logCount = 0;
static bool initialized = false;

// Global logging configuration
static LogConfig logConfig;

// UART logger interface - now using UartInterface instead of HardwareSerial
static UartInterface* logSerial = nullptr;

// Get configuration for external access
LogConfig* logging_get_config() {
    return &logConfig;
}

// Helper function to get short level name
const char* getLogLevelName(LogLevel level) {
    switch(level) {
        case LOG_OFF:     return "OFF";
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

// Initialize UART logging on Device 3
void logging_init_uart() {
  // Check if Device 3 is configured for UART logging
  if (config.device3.role != D3_UART3_LOG) {
    return;
  }
  
  // Create UartDMA for logging output with minimal buffers
  UartDMA::DmaConfig dmaCfg = {
    .useEventTask = false,     // Polling mode for logging
    .dmaRxBufSize = 1024,     // Small RX buffer (not used for TX-only)
    .dmaTxBufSize = DEVICE3_LOG_BUFFER_SIZE * 2,  // Adequate TX buffer for logging
    .ringBufSize = 1024,      // Small ring buffer (not used for TX-only)
    .eventTaskPriority = 10,  // Low priority
    .eventQueueSize = 10      // Small queue
  };
  
  logSerial = new UartDMA(UART_NUM_0, dmaCfg);
  
  if (logSerial) {
    // Create UART configuration for logging (fixed at 115200 8N1)
    UartConfig uartCfg = {
      .baudrate = 115200,
      .databits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stopbits = UART_STOP_BITS_1,
      .flowcontrol = false
    };
    
    // Initialize UART on Device 3 pins (TX only for logging)
    logSerial->begin(uartCfg, -1, DEVICE3_UART_TX_PIN);
    log_msg("UART logging initialized on GPIO" + String(DEVICE3_UART_TX_PIN) + 
            " at 115200 baud (UART0, DMA)", LOG_INFO);
  } else {
    log_msg("Failed to create UART logger interface", LOG_ERROR);
  }
}

// Main logging function with log level
void log_msg(String message, LogLevel level) {
  if (!message || message.length() == 0) return;

  // Check if this message should be logged to web interface
  bool shouldLogToWeb = (config.log_level_web != LOG_OFF && level <= config.log_level_web);
  
  // Save to buffer for web interface only if level is appropriate
  if (shouldLogToWeb && xSemaphoreTake(logMutex, 0) == pdTRUE) {
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

  // Output to UART if Device 3 is configured as logger
  if (logSerial && config.device3.role == D3_UART3_LOG && 
      config.log_level_uart != LOG_OFF && level <= config.log_level_uart) {
    // Non-blocking write to avoid slowing down main thread
    if (logSerial->availableForWrite() > message.length() + 20) {
      unsigned long uptimeMs = millis();
      unsigned long seconds = uptimeMs / 1000;
      unsigned long ms = uptimeMs % 1000;
      
      String logLine = "[" + String(seconds) + "." + String(ms / 100) + "s][" + 
                       String(getLogLevelName(level)) + "] " + message + "\r\n";
      
      // Write the log line as bytes
      logSerial->write((const uint8_t*)logLine.c_str(), logLine.length());
    }
    // If buffer full - skip this log to avoid blocking
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