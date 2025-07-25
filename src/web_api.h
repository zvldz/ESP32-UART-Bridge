#ifndef WEB_API_H
#define WEB_API_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// Generate configuration JSON
String getConfigJson();

// API handlers for async web server
void handleStatus(AsyncWebServerRequest *request);
void handleLogs(AsyncWebServerRequest *request);
void handleSave(AsyncWebServerRequest *request);
void handleResetStats(AsyncWebServerRequest *request);
void handleCrashLogJson(AsyncWebServerRequest *request);
void handleClearCrashLog(AsyncWebServerRequest *request);

#endif // WEB_API_H