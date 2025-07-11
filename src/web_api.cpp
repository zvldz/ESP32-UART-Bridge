#include "web_api.h"
#include "web_interface.h"
#include "logging.h"
#include "config.h"
#include "uartbridge.h"
#include "defines.h"
#include "html_common.h"
#include "crashlog.h"
#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// External objects from main.cpp
extern Config config;
extern UartStats uartStats;
extern SystemState systemState;
extern DeviceMode currentMode;
extern const int DEBUG_MODE;
extern SemaphoreHandle_t logMutex;

// Helper function to safely read protected unsigned long value using critical sections
unsigned long getProtectedULongValue(unsigned long* valuePtr) {
    if (!valuePtr) return 0;
    unsigned long result;
    enterStatsCritical();
    result = *valuePtr;
    exitStatsCritical();
    return result;
}

// Template processor for main page
String mainPageProcessor(const String& var) {
  if (var == "DEVICE_NAME") return config.device_name;
  if (var == "VERSION") return config.version;
  if (var == "FREE_RAM") return String(ESP.getFreeHeap());

  // Protected access to uartStats.deviceStartTime
  if (var == "UPTIME") {
    unsigned long startTime = getProtectedULongValue(&uartStats.deviceStartTime);
    return String((millis() - startTime) / 1000);
  }

  if (var == "WIFI_SSID") return config.ssid;
  if (var == "WIFI_PASSWORD") return config.password;
  if (var == "UART_CONFIG") {
    return String(config.baudrate) + " baud, " +
           String(config.databits) + config.parity.substring(0,1) + String(config.stopbits);
  }
  if (var == "FLOW_CONTROL") return getFlowControlStatus();

  // Protected access to uartStats.bytesUartToUsb
  if (var == "UART_TO_USB") {
    return String(getProtectedULongValue(&uartStats.bytesUartToUsb));
  }

  // Protected access to uartStats.bytesUsbToUart
  if (var == "USB_TO_UART") {
    return String(getProtectedULongValue(&uartStats.bytesUsbToUart));
  }

  // Protected access to total traffic
  if (var == "TOTAL_TRAFFIC") {
    unsigned long total = 0;
    enterStatsCritical();
    total = uartStats.bytesUartToUsb + uartStats.bytesUsbToUart;
    exitStatsCritical();
    return String(total);
  }

  // FIXED: Protected access to lastActivityTime with proper "Never" handling
  if (var == "LAST_ACTIVITY") {
    unsigned long lastActivity;
    enterStatsCritical();
    lastActivity = uartStats.lastActivityTime;
    exitStatsCritical();

    if (lastActivity == 0) {
      return String("Never");
    } else {
      unsigned long secondsAgo = (millis() - lastActivity) / 1000;
      return String(secondsAgo);
    }
  }

  if (var == "DEBUG_MODE") {
    return String(DEBUG_MODE == 0 ? "Production (Bridge Active)" : "Debug (Bridge Disabled)");
  }
  if (var == "LOG_DISPLAY_COUNT") return String(LOG_DISPLAY_COUNT);
  if (var == "BAUDRATE") return String(config.baudrate);
  if (var == "DATABITS") return String(config.databits);
  if (var == "PARITY") return config.parity;
  if (var == "STOPBITS") return String(config.stopbits);
  if (var == "FLOWCONTROL") return config.flowcontrol ? "true" : "false";
  if (var == "UART_FULL_CONFIG") {
    return String(config.baudrate) + " baud, " +
           String(config.databits) + config.parity.substring(0,1) + String(config.stopbits);
  }
  if (var == "FLOW_CONTROL_TEXT") {
    return String(config.flowcontrol ? "Enabled" : "Disabled");
  }

  return String(); // Return empty string if variable not found
}

// Handle status request with critical sections
void handleStatus() {
  WebServer* server = getWebServer();
  if (!server) return;

  JsonDocument doc;

  // Copy all statistics in one critical section
  UartStats localStats;
  enterStatsCritical();
  localStats = uartStats;
  exitStatsCritical();

  // Work with local copy
  doc["freeRam"] = ESP.getFreeHeap();
  doc["uptime"] = (millis() - localStats.deviceStartTime) / 1000;
  doc["uartToUsb"] = localStats.bytesUartToUsb;
  doc["usbToUart"] = localStats.bytesUsbToUart;
  doc["totalTraffic"] = localStats.bytesUartToUsb + localStats.bytesUsbToUart;

  if (localStats.lastActivityTime == 0) {
    doc["lastActivity"] = "Never";
  } else {
    doc["lastActivity"] = (millis() - localStats.lastActivityTime) / 1000;
  }

  String json;
  serializeJson(doc, json);
  server->send(200, "application/json", json);
}

// Handle logs request
void handleLogs() {
  WebServer* server = getWebServer();
  if (!server) return;

  JsonDocument doc;
  JsonArray logs = doc["logs"].to<JsonArray>();

  String logBuffer[LOG_DISPLAY_COUNT];
  int actualCount = 0;
  logging_get_recent_logs(logBuffer, LOG_DISPLAY_COUNT, &actualCount);

  for (int i = 0; i < actualCount; i++) {
    logs.add(logBuffer[i]);
  }

  String json;
  serializeJson(doc, json);
  server->send(200, "application/json", json);
}

// Handle save configuration
void handleSave() {
  WebServer* server = getWebServer();
  if (!server) return;

  log_msg("Saving new configuration...");

  // Parse form data
  bool configChanged = false;

  // UART settings
  if (server->hasArg("baudrate")) {
    uint32_t newBaudrate = server->arg("baudrate").toInt();
    if (newBaudrate != config.baudrate) {
      config.baudrate = newBaudrate;
      configChanged = true;
      log_msg("UART baudrate changed to " + String(newBaudrate));
    }
  }

  if (server->hasArg("databits")) {
    uint8_t newDatabits = server->arg("databits").toInt();
    if (newDatabits != config.databits) {
      config.databits = newDatabits;
      configChanged = true;
      log_msg("UART data bits changed to " + String(newDatabits));
    }
  }

  if (server->hasArg("parity")) {
    String newParity = server->arg("parity");
    if (newParity != config.parity) {
      config.parity = newParity;
      configChanged = true;
      log_msg("UART parity changed to " + newParity);
    }
  }

  if (server->hasArg("stopbits")) {
    uint8_t newStopbits = server->arg("stopbits").toInt();
    if (newStopbits != config.stopbits) {
      config.stopbits = newStopbits;
      configChanged = true;
      log_msg("UART stop bits changed to " + String(newStopbits));
    }
  }

  bool newFlowcontrol = server->hasArg("flowcontrol");
  if (newFlowcontrol != config.flowcontrol) {
    config.flowcontrol = newFlowcontrol;
    configChanged = true;
    log_msg("Flow control " + String(newFlowcontrol ? "enabled" : "disabled"));
  }

  // WiFi settings
  if (server->hasArg("ssid")) {
    String newSSID = server->arg("ssid");
    if (newSSID.length() > 0 && newSSID != config.ssid) {
      config.ssid = newSSID;
      configChanged = true;
      log_msg("WiFi SSID changed to " + newSSID);
    }
  }

  if (server->hasArg("password")) {
    String newPassword = server->arg("password");
    if (newPassword.length() >= 8 && newPassword != config.password) {
      config.password = newPassword;
      configChanged = true;
      log_msg("WiFi password updated");
    }
  }

  if (configChanged) {
    config_save(&config);
    server->send(200, "text/html", "<html><head><title>Configuration Saved</title></head><body><h1>Configuration Saved</h1><p>Settings updated successfully!</p><p>Device will restart in 3 seconds...</p><script>setTimeout(function(){window.location='/';}, 3000);</script></body></html>");
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP.restart();
  } else {
    server->send(200, "text/html", "<h1>No Changes</h1><p>Configuration was not modified.</p><a href='/'>Back</a>");
  }
}

// Handle reset statistics
void handleResetStats() {
  WebServer* server = getWebServer();
  if (!server) return;

  log_msg("Resetting statistics and logs...");
  resetStatistics(&uartStats);
  logging_clear();

  server->send(200, "text/html", loadTemplate(HTML_RESET_SUCCESS));
}

// Handle crash log JSON request
void handleCrashLogJson() {
  WebServer* server = getWebServer();
  if (!server) return;

  String json = crashlog_get_json();
  server->send(200, "application/json", json);
}

// Handle clear crash log request
void handleClearCrashLog() {
  WebServer* server = getWebServer();
  if (!server) return;

  crashlog_clear();
  server->send(200, "application/json", "{\"status\":\"ok\"}");
}