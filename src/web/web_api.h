#ifndef WEB_API_H
#define WEB_API_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// Web API constants
#define MS_TO_SECONDS           1000
#define IP_ADDRESS_BUFFER_SIZE  15

// Generate configuration JSON
String getConfigJson();
void getConfigJson(Print& output);

// Generate logs JSON
void writeLogsJson(Print& output);

// API handlers for async web server
void handleStatus(AsyncWebServerRequest *request);
void handleLogs(AsyncWebServerRequest *request);
void handleSave(AsyncWebServerRequest *request);
void handleResetStats(AsyncWebServerRequest *request);
void handleCrashLogJson(AsyncWebServerRequest *request);
void handleClearCrashLog(AsyncWebServerRequest *request);
void handleExportConfig(AsyncWebServerRequest *request);
void handleImportConfig(AsyncWebServerRequest *request);
void handleClientIP(AsyncWebServerRequest *request);
void handleSbusSetSource(AsyncWebServerRequest *request);
void handleSbusSetMode(AsyncWebServerRequest *request);
void handleSbusStatus(AsyncWebServerRequest *request);


#endif // WEB_API_H