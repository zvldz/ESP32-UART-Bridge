#ifndef LOGGING_H
#define LOGGING_H

#include <Arduino.h>
#include "defines.h"

// Log levels
enum LogLevel {
    LOG_ERROR   = 0,  // Critical errors only
    LOG_WARNING = 1,  // Warnings and important events
    LOG_INFO    = 2,  // General information (default)
    LOG_DEBUG   = 3   // Detailed debug information
};

// Logging configuration structure
struct LogConfig {
    // Channel enable flags
    bool serialEnabled = true;     // Temporary, will be removed after Priority 1
    bool webEnabled = true;        // Always enabled for web interface
    bool uartEnabled = false;      // Device 3 - GPIO 11/12
    bool networkEnabled = false;   // Device 4 - UDP
    
    // Log levels for each channel
    LogLevel serialLevel = LOG_INFO;    // Temporary for compatibility
    LogLevel webLevel = LOG_DEBUG;      // Web shows everything
    LogLevel uartLevel = LOG_DEBUG;     // UART logger - most verbose
    LogLevel networkLevel = LOG_ERROR;  // Network - minimal traffic
};

// Existing functions
void logging_init();
void logging_get_recent_logs(String* buffer, int maxCount, int* actualCount);
void logging_clear();

// Overloaded log functions
void log_msg(String message);                    // Old function for compatibility
void log_msg(String message, LogLevel level);    // New function with level

// Helper function to get level name
const char* getLogLevelName(LogLevel level);

// Configuration access (for future web interface)
LogConfig* logging_get_config();

#endif // LOGGING_H