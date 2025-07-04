#ifndef WEB_API_H
#define WEB_API_H

#include <Arduino.h>

// Template processor
String mainPageProcessor(const String& var);

// API handlers
void handleStatus();
void handleLogs();
void handleSave();
void handleResetStats();

#endif // WEB_API_H