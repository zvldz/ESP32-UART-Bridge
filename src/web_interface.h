#ifndef ESP32_BRIDGE_WEBSERVER_H
#define ESP32_BRIDGE_WEBSERVER_H

#include "types.h"
#include <WebServer.h>

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

// Get server instance for other modules
WebServer* getWebServer();

#endif // ESP32_BRIDGE_WEBSERVER_H