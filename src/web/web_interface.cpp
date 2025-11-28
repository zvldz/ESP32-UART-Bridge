// Standard ESP32 includes
#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include "esp_heap_caps.h"

// Local includes
#include "web_interface.h"
#include "web_api.h"
#include "web_ota.h"
#include "logging.h"
#include "config.h"
#include "defines.h"
#include "../uart/uart_interface.h"
#include "../usb/usb_interface.h"
#include "scheduler_tasks.h"

// Include generated web content
#include "webui_gen/web_content.h"

#include "freertos/semphr.h"
#include "esp_wifi.h"

// External objects from main.cpp
extern Config config;
extern SystemState systemState;
extern BridgeMode bridgeMode;
extern Preferences preferences;
extern UsbInterface* usbInterface;

// Local objects - created on demand
AsyncWebServer* server = nullptr;

// Indicates whether the web server was successfully started
static bool webServerInitialized = false;

// Helper function for gzipped responses
static void sendGzippedResponse(AsyncWebServerRequest* request, const char* contentType,
                               const uint8_t* data, size_t len) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, contentType, data, len);
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "public, max-age=31536000, immutable");
    request->send(response);
}


// Initialize web server in NETWORK mode
void webserver_init(Config* config, SystemState* state) {
    log_msg(LOG_INFO, "Starting Network Mode");

    state->networkActive = true;
    state->networkStartTime = millis();
    
    // Create async web server
    server = new AsyncWebServer(HTTP_PORT);

    // Setup main page with gzip compression
    server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", HTML_INDEX_GZ, HTML_INDEX_GZ_LEN);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    });

    // Setup API routes
    server->on("/save", HTTP_POST, handleSave);
    server->on("/status", HTTP_GET, handleStatus);
    server->on("/logs", HTTP_GET, handleLogs);
    server->on("/reboot", HTTP_GET, handleReboot);
    server->on("/reset_stats", HTTP_GET, handleResetStats);
    server->on("/help", HTTP_GET, handleHelp);
    server->on("/success", HTTP_GET, handleSuccess);
    server->on("/crashlog_json", HTTP_GET, handleCrashLogJson);
    server->on("/clear_crashlog", HTTP_GET, handleClearCrashLog);
    server->on("/test_crash", HTTP_GET, handleTestCrash);
    server->on("/config/export", HTTP_GET, handleExportConfig);


    server->on("/config/import", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            // Handle upload completion
            handleImportConfig(request);
        },
        [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
            // Handle file upload data with PSRAM buffer
            static char* g_importBuf = nullptr;
            static size_t g_importLen = 0;

            if (index == 0) {
                // Clean up any previous buffer
                if (g_importBuf) {
                    heap_caps_free(g_importBuf);
                    g_importBuf = nullptr;
                }

                // Allocate PSRAM buffer
                g_importBuf = static_cast<char*>(heap_caps_malloc(MAX_IMPORT + 1, MALLOC_CAP_SPIRAM));
                if (!g_importBuf) {
                    request->send(507, "application/json", "{\"status\":\"error\",\"message\":\"PSRAM allocation failed\"}");
                    return;
                }
                g_importLen = 0;
            }

            if (!g_importBuf) {
                request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"import buffer not initialized\"}");
                return;
            }

            // ASCII filter and bounds check
            for (size_t i = 0; i < len && g_importLen < MAX_IMPORT; i++) {
                char c = (char)data[i];
                if (c >= ASCII_PRINTABLE_THRESHOLD || c == '\n' || c == '\r' || c == '\t') {
                    g_importBuf[g_importLen++] = c;
                }
            }

            // Hard limit check
            if (g_importLen >= MAX_IMPORT) {
                heap_caps_free(g_importBuf);
                g_importBuf = nullptr;
                request->send(413, "application/json", "{\"status\":\"error\",\"message\":\"config too large\"}");
                return;
            }

            if (final) {
                // Null-terminate and pass to handler
                g_importBuf[g_importLen] = '\0';

                ImportData* importData = new ImportData;
                importData->ptr = g_importBuf;
                importData->len = g_importLen;
                request->_tempObject = importData;

                // Reset static variables (buffer ownership transferred)
                g_importBuf = nullptr;
                g_importLen = 0;
            }
        }
    );
    server->on("/client-ip", HTTP_GET, handleClientIP);
    server->on("/sbus/set_source", HTTP_GET, handleSbusSetSource);
    server->on("/sbus/set_mode", HTTP_GET, handleSbusSetMode);
    server->on("/sbus/status", HTTP_GET, handleSbusStatus);
  
    // Serve static files with gzip compression
    server->on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        sendGzippedResponse(request, "text/css", CSS_STYLE_GZ, CSS_STYLE_GZ_LEN);
    });
    server->on("/main.js", HTTP_GET, [](AsyncWebServerRequest *request){
        sendGzippedResponse(request, "application/javascript", JS_MAIN_GZ, JS_MAIN_GZ_LEN);
    });
    server->on("/crash-log.js", HTTP_GET, [](AsyncWebServerRequest *request){
        sendGzippedResponse(request, "application/javascript", JS_CRASH_LOG_GZ, JS_CRASH_LOG_GZ_LEN);
    });
    server->on("/utils.js", HTTP_GET, [](AsyncWebServerRequest *request){
        sendGzippedResponse(request, "application/javascript", JS_UTILS_GZ, JS_UTILS_GZ_LEN);
    });
    server->on("/device-config.js", HTTP_GET, [](AsyncWebServerRequest *request){
        sendGzippedResponse(request, "application/javascript", JS_DEVICE_CONFIG_GZ, JS_DEVICE_CONFIG_GZ_LEN);
    });
    server->on("/form-utils.js", HTTP_GET, [](AsyncWebServerRequest *request){
        sendGzippedResponse(request, "application/javascript", JS_FORM_UTILS_GZ, JS_FORM_UTILS_GZ_LEN);
    });
    server->on("/status-updates.js", HTTP_GET, [](AsyncWebServerRequest *request){
        sendGzippedResponse(request, "application/javascript", JS_STATUS_UPDATES_GZ, JS_STATUS_UPDATES_GZ_LEN);
    });
    // sbus-source.js removed - functionality integrated into device-config.js

    // Setup OTA update with async handlers
    server->on("/update", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            handleUpdateEnd(request);
        },
        [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
            handleOTA(request, filename, index, data, len, final);
        }
    );

    // Setup not found handler for captive portal
    server->onNotFound(handleNotFound);

    // Start the server
    server->begin();
    log_msg(LOG_INFO, "Async web server started on port 80");
    webServerInitialized = true;
}

// Handle help page with gzip compression
void handleHelp(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", HTML_HELP_GZ, HTML_HELP_GZ_LEN);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

// Handle success page for captive portal
void handleSuccess(AsyncWebServerRequest *request) {
    const char successPage[] = R"(
<!DOCTYPE html><html><head><title>Connected</title></head>
<body><h1>Successfully Connected!</h1>
<p>You can now configure your UART Bridge.</p>
<script>setTimeout(function(){window.location='/';}, 2000);</script>
</body></html>
)";
    request->send(200, "text/html", successPage);
}

// Handle not found - redirect to main page (Captive Portal)
void handleNotFound(AsyncWebServerRequest *request) {
    request->redirect("/");
}

// Handle reboot request
void handleReboot(AsyncWebServerRequest *request) {
    log_msg(LOG_INFO, "Device reboot requested via web interface");
    request->send(200, "text/html", "<h1>Rebooting...</h1>");
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP.restart();
}