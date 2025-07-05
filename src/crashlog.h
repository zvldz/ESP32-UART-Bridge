#ifndef CRASHLOG_H
#define CRASHLOG_H

#include <Arduino.h>
#include <esp_system.h>

// Structure for crash log entry
struct CrashLogEntry {
    uint8_t number;          // Crash number (1-255)
    uint8_t reason;          // esp_reset_reason_t
    uint32_t uptime;         // Seconds before crash
    uint32_t free_heap;      // Free heap before crash
    uint32_t min_heap;       // Minimum heap seen during session
};

// Public interface
void crashlog_check_and_save();   // Called from main.cpp on startup
String crashlog_get_json();       // Called from web_api.cpp for display
void crashlog_clear();            // Called from web_api.cpp to clear history

// Helper functions
String crashlog_get_reset_reason_string(esp_reset_reason_t reason);
String crashlog_format_uptime(uint32_t seconds);

#endif // CRASHLOG_H