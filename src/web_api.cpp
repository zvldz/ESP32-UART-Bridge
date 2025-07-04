#include "web_api.h"
#include "web_interface.h"
#include "logging.h"
#include "config.h"
#include "uartbridge.h"
#include "defines.h"
#include "html_common.h"
#include <Arduino.h>
#include <WebServer.h>

// External objects from main.cpp
extern Config config;
extern UartStats uartStats;
extern SystemState systemState;
extern DeviceMode currentMode;
extern const int DEBUG_MODE;
extern SemaphoreHandle_t statsMutex;
extern SemaphoreHandle_t logMutex;

// Template processor for main page
String mainPageProcessor(const String& var) {
  if (var == "DEVICE_NAME") return config.device_name;
  if (var == "VERSION") return config.version;
  if (var == "FREE_RAM") return String(ESP.getFreeHeap());
  
  // Protected access to uartStats.deviceStartTime
  if (var == "UPTIME") {
    unsigned long value = 0;
    if (xSemaphoreTake(statsMutex, 100) == pdTRUE) {
      value = (millis() - uartStats.deviceStartTime) / 1000;
      xSemaphoreGive(statsMutex);
    }
    return String(value);
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
    unsigned long value = 0;
    if (xSemaphoreTake(statsMutex, 100) == pdTRUE) {
      value = uartStats.bytesUartToUsb;
      xSemaphoreGive(statsMutex);
    }
    return String(value);
  }
  
  // Protected access to uartStats.bytesUsbToUart
  if (var == "USB_TO_UART") {
    unsigned long value = 0;
    if (xSemaphoreTake(statsMutex, 100) == pdTRUE) {
      value = uartStats.bytesUsbToUart;
      xSemaphoreGive(statsMutex);
    }
    return String(value);
  }
  
  // Protected access to total traffic
  if (var == "TOTAL_TRAFFIC") {
    unsigned long value = 0;
    if (xSemaphoreTake(statsMutex, 100) == pdTRUE) {
      value = uartStats.bytesUartToUsb + uartStats.bytesUsbToUart;
      xSemaphoreGive(statsMutex);
    }
    return String(value);
  }
  
  // FIXED: Protected access to lastActivityTime with proper "Never" handling
  if (var == "LAST_ACTIVITY") {
    if (xSemaphoreTake(statsMutex, 100) == pdTRUE) {
      if (uartStats.lastActivityTime == 0) {
        xSemaphoreGive(statsMutex);
        return String("Never");
      } else {
        unsigned long secondsAgo = (millis() - uartStats.lastActivityTime) / 1000;
        xSemaphoreGive(statsMutex);
        return String(secondsAgo);
      }
    }
    return String("Never");  // Fallback if mutex timeout
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

// Handle status request
void handleStatus() {
  WebServer* server = getWebServer();
  if (!server) return;
  
  // Return JSON with current statistics (thread-safe)
  String json = "{";
  
  // Get statistics with mutex protection
  if (xSemaphoreTake(statsMutex, 100) == pdTRUE) {
    json += "\"freeRam\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"uptime\":" + String((millis() - uartStats.deviceStartTime) / 1000) + ",";
    json += "\"uartToUsb\":" + String(uartStats.bytesUartToUsb) + ",";
    json += "\"usbToUart\":" + String(uartStats.bytesUsbToUart) + ",";
    json += "\"totalTraffic\":" + String(uartStats.bytesUartToUsb + uartStats.bytesUsbToUart) + ",";
    
    // FIXED: Handle "Never" case in JSON response
    if (uartStats.lastActivityTime == 0) {
      json += "\"lastActivity\":\"Never\"";
    } else {
      json += "\"lastActivity\":" + String((millis() - uartStats.lastActivityTime) / 1000);
    }
    
    xSemaphoreGive(statsMutex);
  } else {
    // Fallback if mutex timeout
    json += "\"freeRam\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"uptime\":" + String((millis() / 1000)) + ",";
    json += "\"uartToUsb\":0,";
    json += "\"usbToUart\":0,";
    json += "\"totalTraffic\":0,";
    json += "\"lastActivity\":\"Never\"";
  }
  
  json += "}";
  
  server->send(200, "application/json", json);
}

// Handle logs request
void handleLogs() {
  WebServer* server = getWebServer();
  if (!server) return;
  
  // Return JSON with recent log entries
  String json = "{\"logs\":[";
  
  String logBuffer[LOG_DISPLAY_COUNT];
  int actualCount = 0;
  logging_get_recent_logs(logBuffer, LOG_DISPLAY_COUNT, &actualCount);
  
  for (int i = 0; i < actualCount; i++) {
    if (i > 0) json += ",";
    json += "\"" + logBuffer[i] + "\"";
  }
  
  json += "]}";
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
    delay(1000);
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