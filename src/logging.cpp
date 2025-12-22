#include "logging.h"
#include <Arduino.h>
#include "types.h"
#include "usb/usb_interface.h"
#include "config.h"
#include "uart/uart_interface.h"
#include "uart/uart_dma.h"
#include "defines.h"

// External objects
extern SemaphoreHandle_t logMutex;
extern Config config;
extern SystemState systemState;

// Device 3 UART interface (for D3_UART3_LOG mode)
extern UartInterface* device3Serial;

// UDP log buffer definitions - allocated only when Device 4 is in logger role
uint8_t* udpLogBuffer = nullptr;
int udpLogHead = 0;
int udpLogTail = 0;
SemaphoreHandle_t udpLogMutex = nullptr;

// Logging system internal data
static String logBuffer[LOG_BUFFER_SIZE];
static int logIndex = 0;
static int logCount = 0;
static bool initialized = false;

// Global logging configuration
static LogConfig logConfig;

// UART logger interface - now using UartInterface instead of HardwareSerial
static UartInterface* logSerial = nullptr;

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

    // Always allocate UDP log buffer at startup to capture early logs
    // Will be freed later if Device 4 is not in logger role
    if (!udpLogBuffer) {
        udpLogBuffer = (uint8_t*)malloc(UDP_LOG_BUFFER_SIZE);
        if (udpLogBuffer) {
            memset(udpLogBuffer, 0, UDP_LOG_BUFFER_SIZE);
        }
    }
    if (!udpLogMutex) {
        udpLogMutex = xSemaphoreCreateMutex();
    }
    udpLogHead = 0;
    udpLogTail = 0;
}

// Free UDP log buffer if not needed (call after config_load)
void logging_free_udp_if_unused() {
    if (config.device4.role != D4_LOG_NETWORK) {
        if (udpLogBuffer) {
            free(udpLogBuffer);
            udpLogBuffer = nullptr;
        }
        if (udpLogMutex) {
            vSemaphoreDelete(udpLogMutex);
            udpLogMutex = nullptr;
        }
    }
}

// Initialize UART logging on Device 3
void logging_init_uart() {
    // Check if Device 3 is configured for UART logging
    if (config.device3.role != D3_UART3_LOG) {
        return;
    }

    // Use existing device3Serial interface
    logSerial = device3Serial;

    if (logSerial) {
        log_msg(LOG_INFO, "UART logging using existing Device 3 interface (D3_UART3_LOG mode)");
    } else {
        log_msg(LOG_ERROR, "Device 3 UART interface not available for logging");
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
    vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);
    va_end(args);

    // Ensure null termination
    msgBuf[sizeof(msgBuf) - 1] = '\0';
    
    // Check if this message should be logged to web interface
    bool shouldLogToWeb = (config.log_level_web != LOG_OFF && level <= config.log_level_web);
    
    // Web interface logging (with mutex)
    if (shouldLogToWeb && xSemaphoreTake(logMutex, 0) == pdTRUE) {
        unsigned long uptimeMs = millis();
        unsigned long seconds = uptimeMs / 1000;
        unsigned long ms = uptimeMs % 1000;

        // Format directly into a temporary buffer
        char tempBuf[300];
        int webLen = snprintf(tempBuf, sizeof(tempBuf), "[%lu.%lus][%s] %s",
                              seconds, ms/100, getLogLevelName(level), msgBuf);

        // Ensure null termination and check length
        if (webLen > 0 && webLen < (int)sizeof(tempBuf)) {
            // Only now assign to String in one operation
            logBuffer[logIndex] = tempBuf;
        } else {
            // Fallback for too long messages
            logBuffer[logIndex] = "[LOG TOO LONG]";
        }

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

        if (uartLen > 0 && uartLen < (int)sizeof(uartBuf)) {
            // Non-blocking write
            if (logSerial->availableForWrite() > uartLen) {
                logSerial->write(reinterpret_cast<const uint8_t*>(uartBuf), uartLen);
            }
        }
    }
    
    // Network logging
    if (systemState.networkActive &&
        config.device4.role == D4_LOG_NETWORK &&
        config.log_level_network != LOG_OFF &&
        level <= config.log_level_network &&
        udpLogBuffer && udpLogMutex) {

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
            xSemaphoreTake(udpLogMutex, 0) == pdTRUE) {

            // Copy to ring buffer
            for (int i = 0; i < netLen; i++) {
                int nextHead = (udpLogHead + 1) % UDP_LOG_BUFFER_SIZE;
                if (nextHead != udpLogTail) {
                    udpLogBuffer[udpLogHead] = netBuf[i];
                    udpLogHead = nextHead;
                } else {
                    break;  // Buffer full
                }
            }

            xSemaphoreGive(udpLogMutex);
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