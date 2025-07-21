#ifndef ESP32_BRIDGE_WEBSERVER_H
#define ESP32_BRIDGE_WEBSERVER_H

#include "types.h"
#include <WebServer.h>
#include <functional>

// Web server interface
void webserver_init(Config* config, UartStats* stats, SystemState* state);
void webServerTask(void* parameter);  // FreeRTOS task function
void webserver_cleanup();  // Resource cleanup
bool checkWiFiTimeout();

// Page handlers
void handleRoot();
void handleHelp();
void handleSuccess();
void handleNotFound();
void handleReboot();

// Static file handlers
void handleCSS();
void handleMainJS();
void handleCrashJS();
void handleUtilsJS();
void handleDeviceConfigJS();
void handleFormUtilsJS();
void handleStatusUpdatesJS();

// Template processing
String processTemplate(const String& html, std::function<String(const String&)> processor);

// Get server instance for other modules
WebServer* getWebServer();

#endif // ESP32_BRIDGE_WEBSERVER_H