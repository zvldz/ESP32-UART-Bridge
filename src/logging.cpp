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
extern SystemState systemState;

// Device 4 logging support
extern SemaphoreHandle_t device4LogMutex;
extern uint8_t device4LogBuffer[];
extern int device4LogHead;
extern int device4LogTail;
#define DEVICE4_LOG_BUFFER_SIZE 2048

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
    log_msg(LOG_INFO, "UART logging initialized on GPIO%d at 115200 baud (UART0, DMA)", DEVICE3_UART_TX_PIN);
  } else {
    log_msg(LOG_ERROR, "Failed to create UART logger interface");
  }
}

// Printf-style logging - thread-safe and heap-safe
void log_msg(LogLevel level, const char* fmt, ...) {
    if (!fmt) return;
    
    // Stack buffer for formatted message
    char msgBuf[256];
    
    // Format the message
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);
    va_end(args);
    
    // Ensure null termination
    msgBuf[sizeof(msgBuf) - 1] = '\0';
    
    // Truncation warning for debug builds
    #ifdef DEBUG
    if (len >= sizeof(msgBuf)) {
        // Message was truncated
        const char* truncMsg = " [TRUNCATED]";
        size_t truncLen = strlen(truncMsg);
        if (sizeof(msgBuf) > truncLen) {
            strcpy(msgBuf + sizeof(msgBuf) - truncLen - 1, truncMsg);
        }
    }
    #endif
    
    // Check if this message should be logged to web interface
    bool shouldLogToWeb = (config.log_level_web != LOG_OFF && level <= config.log_level_web);
    
    // Web interface logging (with mutex)
    if (shouldLogToWeb && xSemaphoreTake(logMutex, 0) == pdTRUE) {
        unsigned long uptimeMs = millis();
        unsigned long seconds = uptimeMs / 1000;
        unsigned long ms = uptimeMs % 1000;
        
        // Format timestamp + level + message
        String logLine;
        logLine.reserve(300);  // Pre-allocate to avoid fragmentation
        logLine = "[" + String(seconds) + "." + String(ms / 100) + "s][" + 
                  String(getLogLevelName(level)) + "] " + String(msgBuf);
        
        // Truncate if too long
        if (logLine.length() > 250) {
            logLine = logLine.substring(0, 247) + "...";
        }
        
        // Store in circular buffer
        logBuffer[logIndex] = logLine;
        logIndex = (logIndex + 1) % LOG_BUFFER_SIZE;
        if (logCount < LOG_BUFFER_SIZE) {
            logCount++;
        }
        xSemaphoreGive(logMutex);
    }
    
    // UART logging (non-blocking)
    if (logSerial && config.device3.role == D3_UART3_LOG && 
        config.log_level_uart != LOG_OFF && level <= config.log_level_uart) {
        
        // Format for UART with timestamp
        char uartBuf[320];
        unsigned long uptimeMs = millis();
        unsigned long seconds = uptimeMs / 1000;
        unsigned long ms = uptimeMs % 1000;
        
        int uartLen = snprintf(uartBuf, sizeof(uartBuf), 
                              "[%lu.%lus][%s] %s\r\n", 
                              seconds, ms/100, getLogLevelName(level), msgBuf);
        
        if (uartLen > 0 && uartLen < sizeof(uartBuf)) {
            // Non-blocking write
            if (logSerial->availableForWrite() > uartLen) {
                logSerial->write((const uint8_t*)uartBuf, uartLen);
            }
        }
    }
    
    // Network logging
    if (systemState.networkActive &&
        config.device4.role == D4_LOG_NETWORK &&
        config.log_level_network != LOG_OFF && 
        level <= config.log_level_network &&
        device4LogMutex) {
        
        // Format for network
        char netBuf[320];
        unsigned long uptimeMs = millis();
        unsigned long seconds = uptimeMs / 1000;
        unsigned long ms = uptimeMs % 1000;
        
        int netLen = snprintf(netBuf, sizeof(netBuf),
                             "[%lu.%lus][%s] %s\n",
                             seconds, ms/100, getLogLevelName(level), msgBuf);
        
        // Add to ring buffer (non-blocking)
        if (netLen > 0 && netLen < sizeof(netBuf) && 
            xSemaphoreTake(device4LogMutex, 0) == pdTRUE) {
            
            // Copy to ring buffer
            for (int i = 0; i < netLen; i++) {
                int nextHead = (device4LogHead + 1) % DEVICE4_LOG_BUFFER_SIZE;
                if (nextHead != device4LogTail) {
                    device4LogBuffer[device4LogHead] = netBuf[i];
                    device4LogHead = nextHead;
                } else {
                    break;  // Buffer full
                }
            }
            
            xSemaphoreGive(device4LogMutex);
        }
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