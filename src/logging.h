#ifndef LOGGING_H
#define LOGGING_H

#include <Arduino.h>
#include "types.h"      // For LogLevel enum
#include "defines.h"

// Logging configuration structure
struct LogConfig {
    // Channel enable flags
    bool webEnabled = true;        // Always enabled for web interface
    bool uartEnabled = false;      // Device 3 - GPIO 11/12
    bool networkEnabled = false;   // Device 4 - UDP
    
    // Log levels for each channel
    LogLevel webLevel = LOG_DEBUG;      // Web shows everything
    LogLevel uartLevel = LOG_DEBUG;     // UART logger - most verbose
    LogLevel networkLevel = LOG_ERROR;  // Network - minimal traffic
};

// Existing functions
void logging_init();
void logging_get_recent_logs(String* buffer, int maxCount, int* actualCount);
void logging_clear();

// Log functions
void log_msg(LogLevel level, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

// UART logging initialization
void logging_init_uart();

// Helper function to get level name
const char* getLogLevelName(LogLevel level);

// Configuration access (for future web interface)
LogConfig* logging_get_config();

// Safe logging macro for hot path - avoids String allocations
// TEMPORARY: For debugging protocol implementation
#define LOG_HOT_PATH(level, fmt, ...) do { \
    if (config.log_level_uart >= level) { \
        char _logBuf[128]; \
        snprintf(_logBuf, sizeof(_logBuf), fmt, ##__VA_ARGS__); \
        log_msg(_logBuf, level); \
    } \
} while(0)

// Usage example:
// LOG_HOT_PATH(LOG_DEBUG, "Packet detected: size=%d, offset=%zu", size, offset);

#endif // LOGGING_H