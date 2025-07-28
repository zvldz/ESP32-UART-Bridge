// Standard ESP32 includes
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>

// Local includes
#include "web_interface.h"
#include "web_api.h"
#include "web_ota.h"
#include "logging.h"
#include "config.h"
#include "defines.h"
#include "uart_interface.h"
#include "usb_interface.h"
#include "scheduler_tasks.h"

// Include generated web content
#include "webui_gen/web_content.h"

#include "freertos/semphr.h"
#include "esp_wifi.h"

// External objects from main.cpp
extern Config config;
extern UartStats uartStats;
extern SystemState systemState;
extern BridgeMode bridgeMode;
extern Preferences preferences;
extern UsbInterface* usbInterface;

// Local objects - created on demand
AsyncWebServer* server = nullptr;
DNSServer* dnsServer = nullptr;

// Indicates whether the web server was successfully started
static bool webServerInitialized = false;

// Template processor no longer needed - config loaded via AJAX

// Initialize web server in NETWORK mode
void webserver_init(Config* config, UartStats* stats, SystemState* state) {
  log_msg("Starting Network Mode", LOG_INFO);

  // Temporarily pause USB operations if Device 2 is using USB
  // This helps prevent brownout during WiFi initialization power spike
  bool usbWasPaused = false;
  if (config->device2.role == D2_USB && usbInterface) {
    log_msg("Temporarily pausing USB for WiFi startup", LOG_DEBUG);
    Serial.flush();
    vTaskDelay(pdMS_TO_TICKS(50)); // Small delay to ensure flush completes
    usbWasPaused = true;
  }

  state->networkActive = true;
  state->networkStartTime = millis();
  state->isTemporaryNetwork = true;  // Setup AP is temporary

  // Start WiFi Access Point
  WiFi.mode(WIFI_AP);
  WiFi.setTxPower(WIFI_POWER_5dBm);     // For Arduino API compatibility  
  esp_wifi_set_max_tx_power(20);        // Ensure immediate effect

  // Set static IP for AP mode to avoid DHCP race conditions
  IPAddress apIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.softAPConfig(apIP, gateway, subnet);
  WiFi.softAP(config->ssid.c_str(), config->password.c_str());

  // Create DNS server for Captive Portal
  dnsServer = new DNSServer();
  dnsServer->start(53, "*", WiFi.softAPIP());

  log_msg("WiFi AP started: " + config->ssid, LOG_INFO);
  log_msg("IP address: " + WiFi.softAPIP().toString(), LOG_INFO);
  log_msg("Captive Portal DNS server started", LOG_INFO);

  // Resume USB operations if they were paused
  if (usbWasPaused) {
    vTaskDelay(pdMS_TO_TICKS(100)); // Let WiFi stabilize
    log_msg("WiFi startup complete, USB operations continue normally", LOG_DEBUG);
  }

  // Create async web server
  server = new AsyncWebServer(80);

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
  server->on("/config/export", HTTP_GET, handleExportConfig);
  server->on("/config/import", HTTP_POST, 
    [](AsyncWebServerRequest *request) {
      // Handle upload completion
      handleImportConfig(request);
    },
    [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
      // Handle file upload data
      static String uploadedData = "";
      
      if (index == 0) {
        uploadedData = "";  // Reset for new upload
        uploadedData.reserve(4096);  // Reserve space to avoid frequent reallocations
      }
      
      // Only append printable characters and whitespace, skip null bytes
      for (size_t i = 0; i < len; i++) {
        char c = (char)data[i];
        if (c >= 32 || c == '\n' || c == '\r' || c == '\t') {
          uploadedData += c;
        }
      }
      
      if (final) {
        // Store complete data for processing
        request->_tempObject = new String(uploadedData);
      }
    }
  );
  server->on("/client-ip", HTTP_GET, handleClientIP);
  
  // Serve static files with gzip compression
  server->on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", CSS_STYLE_GZ, CSS_STYLE_GZ_LEN);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });
  server->on("/main.js", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "application/javascript", JS_MAIN_GZ, JS_MAIN_GZ_LEN);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });
  server->on("/crash-log.js", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "application/javascript", JS_CRASH_LOG_GZ, JS_CRASH_LOG_GZ_LEN);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });
  server->on("/utils.js", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "application/javascript", JS_UTILS_GZ, JS_UTILS_GZ_LEN);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });
  server->on("/device-config.js", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "application/javascript", JS_DEVICE_CONFIG_GZ, JS_DEVICE_CONFIG_GZ_LEN);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });
  server->on("/form-utils.js", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "application/javascript", JS_FORM_UTILS_GZ, JS_FORM_UTILS_GZ_LEN);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });
  server->on("/status-updates.js", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "application/javascript", JS_STATUS_UPDATES_GZ, JS_STATUS_UPDATES_GZ_LEN);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });
  
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
  log_msg("Async web server started on port 80", LOG_INFO);
  webServerInitialized = true;
}

// Cleanup resources
void webserver_cleanup() {
  if (server != nullptr) {
    server->end();
    delete server;
    server = nullptr;
  }
  if (dnsServer != nullptr) {
    dnsServer->stop();
    delete dnsServer;
    dnsServer = nullptr;
  }
}

// Check WiFi timeout
bool checkWiFiTimeout() {
  if (systemState.networkActive && 
      systemState.isTemporaryNetwork &&
      (millis() - systemState.networkStartTime > WIFI_TIMEOUT)) {
    return true;
  }
  return false;
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
  log_msg("Device reboot requested via web interface", LOG_INFO);
  request->send(200, "text/html", "<h1>Rebooting...</h1>");
  vTaskDelay(pdMS_TO_TICKS(1000));
  ESP.restart();
}

// Make server available to other modules
AsyncWebServer* getWebServer() {
  return server;
}