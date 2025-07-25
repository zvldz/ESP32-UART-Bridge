#include "web_api.h"
#include "web_interface.h"
#include "logging.h"
#include "config.h"
#include "uartbridge.h"
#include "flow_control.h"
#include "defines.h"
#include "crashlog.h"
#include "diagnostics.h"
#include "scheduler_tasks.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// External objects from main.cpp
extern Config config;
extern UartStats uartStats;
extern SystemState systemState;
extern BridgeMode bridgeMode;
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

// Generate complete configuration JSON for initial page load
String getConfigJson() {
    JsonDocument doc;
    
    // System info
    doc["deviceName"] = config.device_name;
    doc["version"] = config.device_version;
    doc["freeRam"] = ESP.getFreeHeap();
    
    // Calculate uptime
    unsigned long startTime = getProtectedULongValue(&uartStats.deviceStartTime);
    doc["uptime"] = (millis() - startTime) / 1000;
    
    // Current configuration - convert ESP-IDF enums to web-compatible values
    doc["baudrate"] = config.baudrate;
    doc["databits"] = atoi(word_length_to_string(config.databits));  // Convert to number
    doc["parity"] = parity_to_string(config.parity);                 // Convert to string
    doc["stopbits"] = atoi(stop_bits_to_string(config.stopbits));    // Convert to number
    doc["flowcontrol"] = config.flowcontrol;
    
    // WiFi
    doc["ssid"] = config.ssid;
    doc["password"] = config.password;
    doc["permanentWifi"] = config.permanent_network_mode;
    
    // USB mode
    doc["usbMode"] = config.usb_mode == USB_MODE_HOST ? "host" : "device";
    
    // Device roles
    doc["device2Role"] = String(config.device2.role);
    doc["device3Role"] = String(config.device3.role);
    doc["device4Role"] = String(config.device4.role);
    
    // Device role names
    doc["device2RoleName"] = getDevice2RoleName(config.device2.role);
    doc["device3RoleName"] = getDevice3RoleName(config.device3.role);
    
    // Log levels
    doc["logLevelWeb"] = (int)config.log_level_web;
    doc["logLevelUart"] = (int)config.log_level_uart;
    doc["logLevelNetwork"] = (int)config.log_level_network;
    
    // UART configuration string - build from ESP-IDF enums
    String uartConfig = String(config.baudrate) + " baud, " +
                       word_length_to_string(config.databits) + 
                       parity_to_string(config.parity)[0] +  // First char only (N/E/O)
                       stop_bits_to_string(config.stopbits);
    doc["uartConfig"] = uartConfig;
    
    // Flow control status
    doc["flowControl"] = getFlowControlStatus();
    
    // Statistics
    doc["device1Rx"] = getProtectedULongValue(&uartStats.device1RxBytes);
    doc["device1Tx"] = getProtectedULongValue(&uartStats.device1TxBytes);
    doc["device2Rx"] = getProtectedULongValue(&uartStats.device2RxBytes);
    doc["device2Tx"] = getProtectedULongValue(&uartStats.device2TxBytes);
    doc["device3Rx"] = getProtectedULongValue(&uartStats.device3RxBytes);
    doc["device3Tx"] = getProtectedULongValue(&uartStats.device3TxBytes);
    
    // Total traffic
    unsigned long totalTraffic = 0;
    enterStatsCritical();
    totalTraffic = uartStats.device1RxBytes + uartStats.device1TxBytes +
                   uartStats.device2RxBytes + uartStats.device2TxBytes +
                   uartStats.device3RxBytes + uartStats.device3TxBytes;
    exitStatsCritical();
    doc["totalTraffic"] = totalTraffic;
    
    // Last activity
    unsigned long lastActivity = getProtectedULongValue(&uartStats.lastActivityTime);
    if (lastActivity == 0) {
        doc["lastActivity"] = "Never";
    } else {
        doc["lastActivity"] = String((millis() - lastActivity) / 1000) + " seconds ago";
    }
    
    // Log display count
    doc["logDisplayCount"] = LOG_DISPLAY_COUNT;
    
    String json;
    serializeJson(doc, json);
    return json;
}

// Handle status request with critical sections
void handleStatus(AsyncWebServerRequest *request) {
  JsonDocument doc;

  // Copy all statistics in one critical section
  UartStats localStats;
  enterStatsCritical();
  localStats = uartStats;
  exitStatsCritical();

  // System info
  doc["freeRam"] = ESP.getFreeHeap();
  doc["uptime"] = (millis() - localStats.deviceStartTime) / 1000;

  // Device 1 stats
  doc["device1Rx"] = localStats.device1RxBytes;
  doc["device1Tx"] = localStats.device1TxBytes;

  // Device 2 stats (only if enabled)
  if (config.device2.role != D2_NONE) {
    doc["device2Rx"] = localStats.device2RxBytes;
    doc["device2Tx"] = localStats.device2TxBytes;
    doc["device2Role"] = getDevice2RoleName(config.device2.role);
  }

  // Device 3 stats (only if enabled)
  if (config.device3.role != D3_NONE) {
    doc["device3Rx"] = localStats.device3RxBytes;
    doc["device3Tx"] = localStats.device3TxBytes;
    doc["device3Role"] = getDevice3RoleName(config.device3.role);
  }

  // Total traffic
  unsigned long totalTraffic = localStats.device1RxBytes + localStats.device1TxBytes +
                               localStats.device2RxBytes + localStats.device2TxBytes +
                               localStats.device3RxBytes + localStats.device3TxBytes;
  doc["totalTraffic"] = totalTraffic;

  // Last activity
  if (localStats.lastActivityTime == 0) {
    doc["lastActivity"] = "Never";
  } else {
    doc["lastActivity"] = (millis() - localStats.lastActivityTime) / 1000;
  }

  String json;
  serializeJson(doc, json);
  request->send(200, "application/json", json);
}

// Handle logs request
void handleLogs(AsyncWebServerRequest *request) {
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
  request->send(200, "application/json", json);
}

// Handle save configuration
void handleSave(AsyncWebServerRequest *request) {
  log_msg("Saving new configuration...", LOG_INFO);

  // Parse form data
  bool configChanged = false;

  // UART settings - convert from web format to ESP-IDF enums
  if (request->hasParam("baudrate", true)) {
    const AsyncWebParameter* p = request->getParam("baudrate", true);
    uint32_t newBaudrate = p->value().toInt();
    if (newBaudrate != config.baudrate) {
      config.baudrate = newBaudrate;
      configChanged = true;
      log_msg("UART baudrate changed to " + String(newBaudrate), LOG_INFO);
    }
  }

  if (request->hasParam("databits", true)) {
    const AsyncWebParameter* p = request->getParam("databits", true);
    uint8_t newDatabits = p->value().toInt();
    uart_word_length_t newWordLength = string_to_word_length(newDatabits);
    if (newWordLength != config.databits) {
      config.databits = newWordLength;
      configChanged = true;
      log_msg("UART data bits changed to " + String(newDatabits), LOG_INFO);
    }
  }

  if (request->hasParam("parity", true)) {
    const AsyncWebParameter* p = request->getParam("parity", true);
    String newParity = p->value();
    uart_parity_t newParityEnum = string_to_parity(newParity.c_str());
    if (newParityEnum != config.parity) {
      config.parity = newParityEnum;
      configChanged = true;
      log_msg("UART parity changed to " + newParity, LOG_INFO);
    }
  }

  if (request->hasParam("stopbits", true)) {
    const AsyncWebParameter* p = request->getParam("stopbits", true);
    uint8_t newStopbits = p->value().toInt();
    uart_stop_bits_t newStopBitsEnum = string_to_stop_bits(newStopbits);
    if (newStopBitsEnum != config.stopbits) {
      config.stopbits = newStopBitsEnum;
      configChanged = true;
      log_msg("UART stop bits changed to " + String(newStopbits), LOG_INFO);
    }
  }

  bool newFlowcontrol = request->hasParam("flowcontrol", true);
  if (newFlowcontrol != config.flowcontrol) {
    config.flowcontrol = newFlowcontrol;
    configChanged = true;
    log_msg("Flow control " + String(newFlowcontrol ? "enabled" : "disabled"), LOG_INFO);
  }

  // USB mode settings
  if (request->hasParam("usbmode", true)) {
    const AsyncWebParameter* p = request->getParam("usbmode", true);
    String mode = p->value();
    UsbMode newMode = USB_MODE_DEVICE;
    if (mode == "host") {
      newMode = USB_MODE_HOST;
    } else if (mode == "auto") {
      newMode = USB_MODE_AUTO;
    }
    
    if (newMode != config.usb_mode) {
      config.usb_mode = newMode;
      configChanged = true;
      log_msg("USB mode changed to " + mode, LOG_INFO);
    }
  }

  // Device 2 role
  if (request->hasParam("device2_role", true)) {
    const AsyncWebParameter* p = request->getParam("device2_role", true);
    int role = p->value().toInt();
    if (role >= D2_NONE && role <= D2_USB) {
      if (role != config.device2.role) {
        config.device2.role = role;
        configChanged = true;
        log_msg("Device 2 role changed to " + String(role), LOG_INFO);
      }
    }
  }

  // Device 3 role
  if (request->hasParam("device3_role", true)) {
    const AsyncWebParameter* p = request->getParam("device3_role", true);
    int role = p->value().toInt();
    if (role >= D3_NONE && role <= D3_UART3_LOG) {
      if (role != config.device3.role) {
        config.device3.role = role;
        configChanged = true;
        log_msg("Device 3 role changed to " + String(role), LOG_INFO);
      }
    }
  }

  // Device 4 role
  if (request->hasParam("device4_role", true)) {
    const AsyncWebParameter* p = request->getParam("device4_role", true);
    int role = p->value().toInt();
    if (role >= D4_NONE && role <= D4_LOG_NETWORK) {
      if (role != config.device4.role) {
        config.device4.role = role;
        configChanged = true;
        log_msg("Device 4 role changed to " + String(role), LOG_INFO);
      }
    }
  }

  // Log levels
  if (request->hasParam("log_level_web", true)) {
    const AsyncWebParameter* p = request->getParam("log_level_web", true);
    int level = p->value().toInt();
    if (p->value() == "-1") level = LOG_OFF;
    
    if (level >= LOG_OFF && level <= LOG_DEBUG) {
      if (level != config.log_level_web) {
        config.log_level_web = (LogLevel)level;
        configChanged = true;
        log_msg("Web log level changed to " + String(getLogLevelName((LogLevel)level)), LOG_INFO);
      }
    }
  }

  if (request->hasParam("log_level_uart", true)) {
    const AsyncWebParameter* p = request->getParam("log_level_uart", true);
    int level = p->value().toInt();
    if (p->value() == "-1") level = LOG_OFF;
    
    if (level >= LOG_OFF && level <= LOG_DEBUG) {
      if (level != config.log_level_uart) {
        config.log_level_uart = (LogLevel)level;
        configChanged = true;
        log_msg("UART log level changed to " + String(getLogLevelName((LogLevel)level)), LOG_INFO);
      }
    }
  }

  if (request->hasParam("log_level_network", true)) {
    const AsyncWebParameter* p = request->getParam("log_level_network", true);
    int level = p->value().toInt();
    if (p->value() == "-1") level = LOG_OFF;
    
    if (level >= LOG_OFF && level <= LOG_DEBUG) {
      if (level != config.log_level_network) {
        config.log_level_network = (LogLevel)level;
        configChanged = true;
        log_msg("Network log level changed to " + String(getLogLevelName((LogLevel)level)), LOG_INFO);
      }
    }
  }

  // WiFi settings
  if (request->hasParam("ssid", true)) {
    const AsyncWebParameter* p = request->getParam("ssid", true);
    String newSSID = p->value();
    if (newSSID.length() > 0 && newSSID != config.ssid) {
      config.ssid = newSSID;
      configChanged = true;
      log_msg("WiFi SSID changed to " + newSSID, LOG_INFO);
    }
  }

  if (request->hasParam("password", true)) {
    const AsyncWebParameter* p = request->getParam("password", true);
    String newPassword = p->value();
    if (newPassword.length() >= 8 && newPassword != config.password) {
      config.password = newPassword;
      configChanged = true;
      log_msg("WiFi password updated", LOG_INFO);
    }
  }

  // Permanent WiFi mode
  if (request->hasParam("permanent_wifi", true)) {
    const AsyncWebParameter* p = request->getParam("permanent_wifi", true);
    bool newPermanent = p->value() == "1";
    if (newPermanent != config.permanent_network_mode) {
      config.permanent_network_mode = newPermanent;
      configChanged = true;
      log_msg("Permanent WiFi mode " + String(newPermanent ? "enabled" : "disabled"), LOG_INFO);
    }
  }

  if (configChanged) {
    // Cancel WiFi timeout when settings are saved successfully
    cancelWiFiTimeout();
    
    config_save(&config);
    request->send(200, "text/html", "<html><head><title>Configuration Saved</title></head><body><h1>Configuration Saved</h1><p>Settings updated successfully!</p><p>Device will restart in 3 seconds...</p><script>setTimeout(function(){window.location='/';}, 3000);</script></body></html>");
    vTaskDelay(pdMS_TO_TICKS(3000));  // Match the 3 seconds shown in HTML
    ESP.restart();
  } else {
    request->send(200, "text/html", "<h1>No Changes</h1><p>Configuration was not modified.</p><a href='/'>Back</a>");
  }
}

// Handle reset statistics
void handleResetStats(AsyncWebServerRequest *request) {
  log_msg("Resetting statistics and logs...", LOG_INFO);
  resetStatistics(&uartStats);
  logging_clear();

  // Return JSON response for AJAX request
  request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Statistics and logs cleared\"}");
}

// Handle crash log JSON request
void handleCrashLogJson(AsyncWebServerRequest *request) {
  String json = crashlog_get_json();
  request->send(200, "application/json", json);
}

// Handle clear crash log request
void handleClearCrashLog(AsyncWebServerRequest *request) {
  crashlog_clear();
  request->send(200, "application/json", "{\"status\":\"ok\"}");
}