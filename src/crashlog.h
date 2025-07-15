#ifndef CRASHLOG_H
#define CRASHLOG_H

#include <Arduino.h>
#include <esp_system.h>

// Public interface
void crashlog_check_and_save();   // Called from main.cpp on startup
String crashlog_get_json();       // Called from web_api.cpp for display
void crashlog_clear();            // Called from web_api.cpp to clear history

// Helper functions
String crashlog_get_reset_reason_string(esp_reset_reason_t reason);
String crashlog_format_uptime(uint32_t seconds);

#endif // CRASHLOG_H