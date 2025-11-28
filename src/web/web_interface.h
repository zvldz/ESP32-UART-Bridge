#ifndef ESP32_BRIDGE_WEBSERVER_H
#define ESP32_BRIDGE_WEBSERVER_H

#include "types.h"
#include <ESPAsyncWebServer.h>
#include <functional>

// Web interface constants
#define HTTP_PORT                   80
#define UPLOAD_BUFFER_RESERVE       4096
#define ASCII_PRINTABLE_THRESHOLD   32
#define MAX_IMPORT                  (32 * 1024)  // 32 KiB limit for config import

// Structure for passing PSRAM import data
struct ImportData {
    char* ptr;
    size_t len;
};

// Web server interface
void webserver_init(Config* config, SystemState* state);

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

#endif // ESP32_BRIDGE_WEBSERVER_H