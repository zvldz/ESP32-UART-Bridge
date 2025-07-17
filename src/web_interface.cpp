// Standard ESP32 includes
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WebServer.h>

// Local includes
#include "web_interface.h"
#include "web_api.h"
#include "web_ota.h"
#include "logging.h"
#include "config.h"
#include "defines.h"

// HTML template includes
#include "html_common.h"
#include "html_main_page.h"
#include "html_help_page.h"

#include "freertos/semphr.h"
#include "esp_wifi.h"

SemaphoreHandle_t webServerReadySemaphore = nullptr;

// External objects from main.cpp
extern Config config;
extern UartStats uartStats;
extern SystemState systemState;
extern DeviceMode currentMode;
extern Preferences preferences;

// Local objects - created on demand
WebServer* server = nullptr;
DNSServer* dnsServer = nullptr;

// Indicates whether the web server was successfully started
static bool webServerInitialized = false;

// Initialize web server in CONFIG mode
void webserver_init(Config* config, UartStats* stats, SystemState* state) {
  log_msg("Starting WiFi Configuration Mode", LOG_INFO);

  // Always disable USB CDC before Wi-Fi initialization to avoid conflicts
  Serial.flush();
  Serial.end();
  vTaskDelay(pdMS_TO_TICKS(100));

  state->wifiAPActive = true;
  state->wifiStartTime = millis();

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

  // Re-enable USB CDC after Wi-Fi is active at configured bridge rate
  Serial.begin(config->baudrate);
  vTaskDelay(pdMS_TO_TICKS(100));
  log_msg("Serial USB reinitialized in config mode", LOG_DEBUG);

  // Create web server
  server = new WebServer(80);

  // Setup web server routes
  server->on("/", handleRoot);
  server->on("/save", HTTP_POST, handleSave);
  server->on("/status", handleStatus);
  server->on("/logs", handleLogs);
  server->on("/reboot", handleReboot);
  server->on("/reset_stats", handleResetStats);
  server->on("/help", handleHelp);
  server->on("/success", handleSuccess);
  server->on("/crashlog_json", handleCrashLogJson);
  server->on("/clear_crashlog", handleClearCrashLog);
  server->onNotFound(handleNotFound);
  server->on("/update", HTTP_POST, handleUpdateEnd, handleOTA);

  server->begin();
  log_msg("Web server started on port 80", LOG_INFO);
  webServerInitialized = true;

  // Signal that the web server is initialized
  if (!webServerReadySemaphore) {
    webServerReadySemaphore = xSemaphoreCreateBinary();
  }
  xSemaphoreGive(webServerReadySemaphore);
}

// Web server task for FreeRTOS
void webServerTask(void* parameter) {
  // Wait for Web Server initialization to complete
  log_msg("Waiting for web server initialization...", LOG_DEBUG);
  if (webServerReadySemaphore) {
    xSemaphoreTake(webServerReadySemaphore, portMAX_DELAY);
  }

  log_msg("Web task started on core " + String(xPortGetCoreID()), LOG_DEBUG);
  vTaskDelay(pdMS_TO_TICKS(1000));

  // Stack diagnostics timer
  static unsigned long lastStackCheck = 0;

  while (1) {
    // Stack diagnostics every 5 seconds
    if (millis() - lastStackCheck > 5000) {
      UBaseType_t stackFree = uxTaskGetStackHighWaterMark(NULL);
      log_msg("Web task: Stack free=" + String(stackFree * 4) +
              " bytes, Heap free=" + String(ESP.getFreeHeap()) +
              " bytes, Largest block=" + String(ESP.getMaxAllocHeap()) + " bytes", LOG_DEBUG);
      lastStackCheck = millis();
    }

    if (server && !webServerInitialized) {
      log_msg("Server not started", LOG_WARNING);
    } else if (server) {
      server->handleClient();
    }
    vTaskDelay(pdMS_TO_TICKS(5)); // Give other tasks a chance

    if (dnsServer) {
      dnsServer->processNextRequest();
    }

    // Check for WiFi timeout
    if (checkWiFiTimeout()) {
      log_msg("WiFi timeout - switching to normal mode", LOG_INFO);
      ESP.restart();
    }
  }
}

// Cleanup resources
void webserver_cleanup() {
  if (server != nullptr) {
    server->stop();
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
  if (systemState.wifiAPActive && (millis() - systemState.wifiStartTime > WIFI_TIMEOUT)) {
    return true;
  }
  return false;
}

// Handle root page
void handleRoot() {
  String html;

  // Build page using new structure
  html += loadTemplate(HTML_DOCTYPE_META);    // Common DOCTYPE and meta
  html += loadTemplate(HTML_MAIN_TITLE);      // Page-specific title
  html += loadTemplate(HTML_STYLES);          // Common styles
  html += loadTemplate(HTML_COMMON_JS);       // Common JavaScript utilities
  html += loadTemplate(HTML_BODY_CONTAINER);  // Common container
  html += loadTemplate(HTML_MAIN_HEADING);    // Page heading
  html += loadTemplate(HTML_QUICK_GUIDE);

  // System Status section
  html += loadTemplate(HTML_SYSTEM_STATUS_START);

  // Group 1: System Information
  html += processTemplate(loadTemplate(HTML_SYSTEM_INFO_TABLE), mainPageProcessor);

  // Group 2: Traffic Volume
  html += processTemplate(loadTemplate(HTML_TRAFFIC_STATS), mainPageProcessor);

  // System Logs section
  html += processTemplate(loadTemplate(HTML_SYSTEM_LOGS), mainPageProcessor);

  // Crash History section
  html += loadTemplate(HTML_CRASH_HISTORY);

  // Combined Device Settings section
  html += processTemplate(loadTemplate(HTML_DEVICE_SETTINGS), mainPageProcessor);

  // Firmware Update section
  html += loadTemplate(HTML_FIRMWARE_UPDATE);

  // JavaScript to set current values and auto-refresh
  html += processTemplate(loadTemplate(HTML_JAVASCRIPT), mainPageProcessor);

  html += loadTemplate(HTML_FOOTER);

  server->send(200, "text/html", html);
}

// Handle help page
void handleHelp() {
  String html;

  // Build page using new structure
  html += loadTemplate(HTML_DOCTYPE_META);    // Common DOCTYPE and meta
  html += loadTemplate(HTML_HELP_TITLE);      // Page-specific title
  html += loadTemplate(HTML_STYLES);          // Common styles
  html += loadTemplate(HTML_BODY_CONTAINER);  // Common container
  html += loadTemplate(HTML_HELP_HEADING);    // Page heading

  // Add all help sections from templates
  html += loadTemplate(HTML_HELP_PROTOCOL_INFO);
  html += loadTemplate(HTML_HELP_PIN_TABLE);
  html += loadTemplate(HTML_HELP_LED_BEHAVIOR);
  html += loadTemplate(HTML_HELP_CONNECTION_GUIDE);
  html += loadTemplate(HTML_HELP_SETUP_INSTRUCTIONS);
  html += loadTemplate(HTML_HELP_TROUBLESHOOTING);

  html += loadTemplate(HTML_HELP_BUTTONS);
  html += loadTemplate(HTML_FOOTER);

  server->send(200, "text/html", html);
}

// Handle success page for captive portal
void handleSuccess() {
  server->send(200, "text/html", loadTemplate(HTML_SUCCESS_PAGE));
}

// Handle not found - redirect to main page (Captive Portal)
void handleNotFound() {
  server->sendHeader("Location", "/", true);
  server->send(302, "text/plain", "Redirecting to configuration page");
}

// Handle reboot request
void handleReboot() {
  log_msg("Device reboot requested via web interface", LOG_INFO);
  server->send(200, "text/html", "<h1>Rebooting...</h1>");
  vTaskDelay(pdMS_TO_TICKS(1000));
  ESP.restart();
}

// Make server available to other modules
WebServer* getWebServer() {
  return server;
}