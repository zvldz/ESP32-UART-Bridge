#ifndef WEB_API_H
#define WEB_API_H

#include <Arduino.h>

// Generate configuration JSON
String getConfigJson();

// API handlers
void handleStatus();
void handleLogs();
void handleSave();
void handleResetStats();
void handleCrashLogJson();
void handleClearCrashLog();

#endif // WEB_API_H