#ifndef WEB_OTA_H
#define WEB_OTA_H

#include <ESPAsyncWebServer.h>

// OTA update handlers for async web server
void handleOTA(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);
void handleUpdateEnd(AsyncWebServerRequest *request);

#endif // WEB_OTA_H