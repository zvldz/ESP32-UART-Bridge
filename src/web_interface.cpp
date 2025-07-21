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
#include "uart_interface.h"
#include "usb_interface.h"

// Include generated web content
#include "generated/web_content.h"

#include "freertos/semphr.h"
#include "esp_wifi.h"

SemaphoreHandle_t webServerReadySemaphore = nullptr;

// External objects from main.cpp
extern Config config;
extern UartStats uartStats;
extern SystemState systemState;
extern DeviceMode currentMode;
extern Preferences preferences;
extern UsbInterface* usbInterface;

// Local objects - created on demand
WebServer* server = nullptr;
DNSServer* dnsServer = nullptr;

// Indicates whether the web server was successfully started
static bool webServerInitialized = false;

// New template processor for {{}} placeholders
String processTemplate(const String& html, std::function<String(const String&)> processor) {
    String result = html;
    int start = 0;
    
    while ((start = result.indexOf("{{", start)) != -1) {
        int end = result.indexOf("}}", start + 2);
        if (end == -1) break;
        
        String var = result.substring(start + 2, end);
        String value = processor(var);
        
        if (value.length() > 0) {
            result = result.substring(0, start) + value + result.substring(end + 2);
            start += value.length();
        } else {
            start = end + 2;
        }
    }
    
    return result;
}

// Initialize web server in CONFIG mode
void webserver_init(Config* config, UartStats* stats, SystemState* state) {
  log_msg("Starting WiFi Configuration Mode", LOG_INFO);

  // Temporarily pause USB operations if Device 2 is using USB
  // This helps prevent brownout during WiFi initialization power spike
  bool usbWasPaused = false;
  if (config->device2.role == D2_USB && usbInterface) {
    log_msg("Temporarily pausing USB for WiFi startup", LOG_DEBUG);
    Serial.flush();
    vTaskDelay(pdMS_TO_TICKS(50)); // Small delay to ensure flush completes
    usbWasPaused = true;
  }

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

  // Resume USB operations if they were paused
  if (usbWasPaused) {
    vTaskDelay(pdMS_TO_TICKS(100)); // Let WiFi stabilize
    log_msg("WiFi startup complete, USB operations continue normally", LOG_DEBUG);
  }

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
  
  // Serve static files
  server->on("/style.css", handleCSS);
  server->on("/main.js", handleMainJS);
  server->on("/crash-log.js", handleCrashJS);
  server->on("/utils.js", handleUtilsJS);
  server->on("/device-config.js", handleDeviceConfigJS);
  server->on("/form-utils.js", handleFormUtilsJS);
  server->on("/status-updates.js", handleStatusUpdatesJS);
  
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
      // Process DNS only every 50ms instead of every 5ms
      static unsigned long lastDnsProcess = 0;
      if (millis() - lastDnsProcess > 50) {
        dnsServer->processNextRequest();
        lastDnsProcess = millis();
      }
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
  String html = String(FPSTR(HTML_INDEX));
  
  // Process template with configuration JSON
  html = processTemplate(html, [](const String& var) {
    if (var == "CONFIG_JSON") {
      return getConfigJson();
    }
    return String();
  });
  
  server->send(200, "text/html", html);
}

// Handle help page
void handleHelp() {
  server->send(200, "text/html", FPSTR(HTML_HELP));
}

// Handle CSS request
void handleCSS() {
  server->send(200, "text/css", FPSTR(CSS_STYLE));
}

// Handle main.js request
void handleMainJS() {
  server->send(200, "application/javascript", FPSTR(JS_MAIN));
}

// Handle crash-log.js request
void handleCrashJS() {
  server->send(200, "application/javascript", FPSTR(JS_CRASH_LOG));
}

// Handle utils.js request
void handleUtilsJS() {
  server->send(200, "application/javascript", FPSTR(JS_UTILS));
}

// Handle device-config.js request
void handleDeviceConfigJS() {
  server->send(200, "application/javascript", FPSTR(JS_DEVICE_CONFIG));
}

// Handle form-utils.js request
void handleFormUtilsJS() {
  server->send(200, "application/javascript", FPSTR(JS_FORM_UTILS));
}

// Handle status-updates.js request
void handleStatusUpdatesJS() {
  server->send(200, "application/javascript", FPSTR(JS_STATUS_UPDATES));
}

// Handle success page for captive portal
void handleSuccess() {
  const char successPage[] = R"(
<!DOCTYPE html><html><head><title>Connected</title></head>
<body><h1>Successfully Connected!</h1>
<p>You can now configure your UART Bridge.</p>
<script>setTimeout(function(){window.location='/';}, 2000);</script>
</body></html>
)";
  server->send(200, "text/html", successPage);
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