#ifndef ESP32_BRIDGE_WEBSERVER_H
#define ESP32_BRIDGE_WEBSERVER_H

#include "types.h"
#include <ESPAsyncWebServer.h>
#include <functional>

// Web server interface
void webserver_init(Config* config, SystemState* state);
void webserver_cleanup();  // Resource cleanup
bool checkWiFiTimeout();

// Page handlers
void handleRoot(AsyncWebServerRequest *request);
void handleHelp(AsyncWebServerRequest *request);
void handleSuccess(AsyncWebServerRequest *request);
void handleNotFound(AsyncWebServerRequest *request);
void handleReboot(AsyncWebServerRequest *request);

// Static file handlers
void handleCSS(AsyncWebServerRequest *request);
void handleMainJS(AsyncWebServerRequest *request);
void handleCrashJS(AsyncWebServerRequest *request);
void handleUtilsJS(AsyncWebServerRequest *request);
void handleDeviceConfigJS(AsyncWebServerRequest *request);
void handleFormUtilsJS(AsyncWebServerRequest *request);
void handleStatusUpdatesJS(AsyncWebServerRequest *request);

// Template processing for built-in AsyncWebServer processor
String processor(const String& var);

// Get server instance for other modules
AsyncWebServer* getWebServer();

#endif // ESP32_BRIDGE_WEBSERVER_H