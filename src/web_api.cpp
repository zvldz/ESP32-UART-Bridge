#include "protocols/protocol_stats.h"
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
#include "wifi_manager.h"
#include "protocols/protocol_pipeline.h"
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
    
    // WiFi mode and client settings
    doc["wifiMode"] = config.wifi_mode;
    doc["wifiClientSsid"] = config.wifi_client_ssid;
    doc["wifiClientPassword"] = config.wifi_client_password;
    
    // Client mode status
    if (config.wifi_mode == BRIDGE_WIFI_MODE_CLIENT) {
        doc["wifiClientConnected"] = systemState.wifiClientConnected;
        if (systemState.wifiClientConnected) {
            doc["ipAddress"] = wifi_manager_get_ip();
            doc["rssiPercent"] = rssi_to_percent(wifi_manager_get_rssi());
        }
    }
    
    // USB mode
    doc["usbMode"] = config.usb_mode == USB_MODE_HOST ? "host" : "device";
    
    // Device roles
    doc["device2Role"] = String(config.device2.role);
    doc["device3Role"] = String(config.device3.role);
    doc["device4Role"] = String(config.device4.role);
    
    // Device role names
    doc["device2RoleName"] = getDevice2RoleName(config.device2.role);
    doc["device3RoleName"] = getDevice3RoleName(config.device3.role);
    doc["device4RoleName"] = getDevice4RoleName(config.device4.role);
    
    // Device 4 configuration
    doc["device4TargetIp"] = config.device4_config.target_ip;
    doc["device4Port"] = config.device4_config.port;
    
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
    
    // Add Device 4 statistics if active
    if (config.device4.role != D4_NONE && systemState.networkActive) {
        doc["device4TxBytes"] = getProtectedULongValue(&uartStats.device4TxBytes);
        doc["device4TxPackets"] = getProtectedULongValue(&uartStats.device4TxPackets);
        doc["device4RxBytes"] = getProtectedULongValue(&uartStats.device4RxBytes);
        doc["device4RxPackets"] = getProtectedULongValue(&uartStats.device4RxPackets);
    }
    
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
    
    // Protocol optimization configuration
    doc["protocolOptimization"] = config.protocolOptimization;
    
    // Protocol statistics via bridge context accessor
    BridgeContext* ctx = getBridgeContext();
    if (ctx && ctx->protocol.stats) {
        auto stats = ctx->protocol.stats;
        doc["protocolStats"]["packetsDetected"] = stats->packetsDetected;
        doc["protocolStats"]["packetsTransmitted"] = stats->packetsTransmitted;
        doc["protocolStats"]["detectionErrors"] = stats->detectionErrors;
        doc["protocolStats"]["resyncEvents"] = stats->resyncEvents;
        doc["protocolStats"]["totalBytes"] = stats->totalBytes;
        doc["protocolStats"]["minPacketSize"] = (stats->minPacketSize == UINT32_MAX) ? 0 : stats->minPacketSize;
        doc["protocolStats"]["maxPacketSize"] = stats->maxPacketSize;
        doc["protocolStats"]["avgPacketSize"] = stats->avgPacketSize;
        doc["protocolStats"]["packetsPerSecond"] = stats->packetsPerSecond;
        doc["protocolStats"]["consecutiveErrors"] = stats->consecutiveErrors;
        doc["protocolStats"]["maxConsecutiveErrors"] = stats->maxConsecutiveErrors;
        
        // Format last packet time with overflow protection
        if (stats->lastPacketTime == 0) {
            doc["protocolStats"]["lastPacketTime"] = "Never";
        } else {
            unsigned long currentMillis = millis();
            unsigned long timeDiff;
            
            // Handle millis() overflow properly (every ~49 days)
            if (currentMillis >= stats->lastPacketTime) {
                timeDiff = currentMillis - stats->lastPacketTime;
            } else {
                // Only consider it overflow if difference is huge (> 40 days)
                if ((stats->lastPacketTime - currentMillis) > 3456000000UL) {  // ~40 days in ms
                    stats->lastPacketTime = currentMillis;
                    timeDiff = 0;
                } else {
                    // Small negative difference - just old packet, use simple calculation
                    timeDiff = currentMillis + (UINT32_MAX - stats->lastPacketTime);
                }
            }
            
            // Reset very old time values (likely old data from previous session)
            if (timeDiff > 86400000) {  // More than 24 hours in milliseconds
                stats->lastPacketTime = currentMillis;
                timeDiff = 0;
            }
            
            if (timeDiff == 0) {
                doc["protocolStats"]["lastPacketTime"] = "Just now";
            } else {
                doc["protocolStats"]["lastPacketTime"] = String(timeDiff / 1000) + " seconds ago";
            }
        }
    }
    
    // Log display count
    doc["logDisplayCount"] = LOG_DISPLAY_COUNT;
    
    String json;
    serializeJson(doc, json);
    return json;
}

// Handle status request - return full config and stats (used by AJAX)
void handleStatus(AsyncWebServerRequest *request) {
  // Use existing getConfigJson() function which includes everything needed
  String json = getConfigJson();
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
  log_msg(LOG_INFO, "Saving new configuration...");

  // Parse form data
  bool configChanged = false;

  // UART settings - convert from web format to ESP-IDF enums
  if (request->hasParam("baudrate", true)) {
    const AsyncWebParameter* p = request->getParam("baudrate", true);
    uint32_t newBaudrate = p->value().toInt();
    if (newBaudrate != config.baudrate) {
      config.baudrate = newBaudrate;
      configChanged = true;
      log_msg(LOG_INFO, "UART baudrate changed to %u", newBaudrate);
    }
  }

  if (request->hasParam("databits", true)) {
    const AsyncWebParameter* p = request->getParam("databits", true);
    uint8_t newDatabits = p->value().toInt();
    uart_word_length_t newWordLength = string_to_word_length(newDatabits);
    if (newWordLength != config.databits) {
      config.databits = newWordLength;
      configChanged = true;
      log_msg(LOG_INFO, "UART data bits changed to %u", newDatabits);
    }
  }

  if (request->hasParam("parity", true)) {
    const AsyncWebParameter* p = request->getParam("parity", true);
    String newParity = p->value();
    uart_parity_t newParityEnum = string_to_parity(newParity.c_str());
    if (newParityEnum != config.parity) {
      config.parity = newParityEnum;
      configChanged = true;
      log_msg(LOG_INFO, "UART parity changed to %s", newParity.c_str());
    }
  }

  if (request->hasParam("stopbits", true)) {
    const AsyncWebParameter* p = request->getParam("stopbits", true);
    uint8_t newStopbits = p->value().toInt();
    uart_stop_bits_t newStopBitsEnum = string_to_stop_bits(newStopbits);
    if (newStopBitsEnum != config.stopbits) {
      config.stopbits = newStopBitsEnum;
      configChanged = true;
      log_msg(LOG_INFO, "UART stop bits changed to %u", newStopbits);
    }
  }

  bool newFlowcontrol = request->hasParam("flowcontrol", true);
  if (newFlowcontrol != config.flowcontrol) {
    config.flowcontrol = newFlowcontrol;
    configChanged = true;
    log_msg(LOG_INFO, "Flow control %s", newFlowcontrol ? "enabled" : "disabled");
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
      log_msg(LOG_INFO, "USB mode changed to %s", mode.c_str());
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
        log_msg(LOG_INFO, "Device 2 role changed to %d", role);
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
        log_msg(LOG_INFO, "Device 3 role changed to %d", role);
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
        log_msg(LOG_INFO, "Device 4 role changed to %d", role);
      }
    }
  }

  // Device 4 configuration
  if (request->hasParam("device4_target_ip", true)) {
    String ip = request->getParam("device4_target_ip", true)->value();
    strncpy(config.device4_config.target_ip, ip.c_str(), 15);
    config.device4_config.target_ip[15] = '\0';
    configChanged = true;
    log_msg(LOG_INFO, "Device 4 target IP changed to %s", ip.c_str());
  }
  if (request->hasParam("device4_port", true)) {
    String portStr = request->getParam("device4_port", true)->value();
    config.device4_config.port = portStr.toInt();
    configChanged = true;
    log_msg(LOG_INFO, "Device 4 port changed to %s", portStr.c_str());
  }
  // Copy role to configuration
  config.device4_config.role = config.device4.role;

  // Log levels
  if (request->hasParam("log_level_web", true)) {
    const AsyncWebParameter* p = request->getParam("log_level_web", true);
    int level = p->value().toInt();
    if (p->value() == "-1") level = LOG_OFF;
    
    if (level >= LOG_OFF && level <= LOG_DEBUG) {
      if (level != config.log_level_web) {
        config.log_level_web = (LogLevel)level;
        configChanged = true;
        log_msg(LOG_INFO, "Web log level changed to %s", getLogLevelName((LogLevel)level));
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
        log_msg(LOG_INFO, "UART log level changed to %s", getLogLevelName((LogLevel)level));
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
        log_msg(LOG_INFO, "Network log level changed to %s", getLogLevelName((LogLevel)level));
      }
    }
  }

  // Protocol optimization
  if (request->hasParam("protocol_optimization", true)) {
    const AsyncWebParameter* p = request->getParam("protocol_optimization", true);
    uint8_t newProtocol = p->value().toInt();
    if (newProtocol != config.protocolOptimization) {
      config.protocolOptimization = newProtocol;
      configChanged = true;
      const char* protocolName = (newProtocol == 0) ? "None" : 
                                 (newProtocol == 1) ? "MAVLink" : "Unknown";
      log_msg(LOG_INFO, "Protocol optimization changed to %s", protocolName);
      
      // Reinitialize protocol detection with new settings
      BridgeContext* ctx = getBridgeContext();
      if (ctx) {
        initProtocolDetection(ctx, &config);
        log_msg(LOG_DEBUG, "Protocol detection reinitialized");
      } else {
        log_msg(LOG_WARNING, "Warning: BridgeContext not available for protocol reinit");
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
      log_msg(LOG_INFO, "WiFi SSID changed to %s", newSSID.c_str());
    }
  }

  if (request->hasParam("password", true)) {
    const AsyncWebParameter* p = request->getParam("password", true);
    String newPassword = p->value();
    if (newPassword.length() >= 8 && newPassword != config.password) {
      config.password = newPassword;
      configChanged = true;
      log_msg(LOG_INFO, "WiFi password updated");
    }
  }

  // Permanent WiFi mode
  if (request->hasParam("permanent_wifi", true)) {
    const AsyncWebParameter* p = request->getParam("permanent_wifi", true);
    bool newPermanent = p->value() == "1";
    if (newPermanent != config.permanent_network_mode) {
      config.permanent_network_mode = newPermanent;
      configChanged = true;
      log_msg(LOG_INFO, "Permanent WiFi mode %s", newPermanent ? "enabled" : "disabled");
    }
  }
  
  // WiFi mode
  if (request->hasParam("wifi_mode", true)) {
    const AsyncWebParameter* p = request->getParam("wifi_mode", true);
    int mode = p->value().toInt();
    if (mode >= BRIDGE_WIFI_MODE_AP && mode <= BRIDGE_WIFI_MODE_CLIENT) {
      if (mode != config.wifi_mode) {
        config.wifi_mode = (BridgeWiFiMode)mode;
        configChanged = true;
        log_msg(LOG_INFO, "WiFi mode changed to %s", mode == BRIDGE_WIFI_MODE_AP ? "AP" : "Client");
      }
    }
  }

  // Client SSID
  if (request->hasParam("wifi_client_ssid", true)) {
    const AsyncWebParameter* p = request->getParam("wifi_client_ssid", true);
    String newSSID = p->value();
    
    // Validate SSID
    newSSID.trim();  // Remove whitespace
    if (config.wifi_mode == BRIDGE_WIFI_MODE_CLIENT && newSSID.isEmpty()) {
      log_msg(LOG_ERROR, "Client SSID cannot be empty");
      request->send(400, "application/json", 
        "{\"status\":\"error\",\"message\":\"Client SSID cannot be empty\"}");
      return;
    }
    
    if (newSSID != config.wifi_client_ssid) {
      config.wifi_client_ssid = newSSID;
      configChanged = true;
      log_msg(LOG_INFO, "WiFi Client SSID changed to %s", newSSID.c_str());
    }
  }

  // Client Password
  if (request->hasParam("wifi_client_password", true)) {
    const AsyncWebParameter* p = request->getParam("wifi_client_password", true);
    String newPassword = p->value();
    
    // Validate password (empty allowed for open networks, otherwise min 8 chars)
    if (newPassword.length() > 0 && newPassword.length() < 8) {
      log_msg(LOG_ERROR, "Client password must be at least 8 characters or empty");
      request->send(400, "application/json", 
        "{\"status\":\"error\",\"message\":\"WiFi password must be at least 8 characters or empty for open network\"}");
      return;
    }
    
    if (newPassword != config.wifi_client_password) {
      config.wifi_client_password = newPassword;
      configChanged = true;
      log_msg(LOG_INFO, "WiFi Client password updated");
    }
  }

  if (configChanged) {
    // Cancel WiFi timeout when settings are saved successfully
    cancelWiFiTimeout();
    
    config_save(&config);
    
    request->send(200, "application/json", 
        "{\"status\":\"ok\",\"message\":\"Configuration saved successfully! Device restarting...\"}");
    
    scheduleReboot(3000);
  } else {
    request->send(200, "application/json", "{\"status\":\"unchanged\",\"message\":\"Configuration was not modified\"}");
  }
}

// Handle reset statistics
void handleResetStats(AsyncWebServerRequest *request) {
  log_msg(LOG_INFO, "Resetting statistics and logs...");
  resetStatistics(&uartStats);
  
  // Reset protocol statistics via bridge context accessor
  BridgeContext* ctx = getBridgeContext();
  if (ctx && ctx->protocol.stats) {
    ctx->protocol.stats->reset();
    log_msg(LOG_INFO, "Protocol statistics reset");
  }
  
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

// Export configuration as downloadable JSON file
void handleExportConfig(AsyncWebServerRequest *request) {
  log_msg(LOG_INFO, "Configuration export requested");
  
  // Generate random ID for filename uniqueness
  String randomId = String(millis() & 0xFFFFFF, HEX);
  randomId.toUpperCase();
  String filename = "esp32-bridge-config-" + randomId + ".json";
  
  // Get configuration JSON (without runtime statistics)
  String configJson = config_to_json(&config);
  
  // Send as downloadable file
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", configJson);
  response->addHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
  request->send(response);
}

// Import configuration from uploaded JSON file
void handleImportConfig(AsyncWebServerRequest *request) {
  // Check if file data was uploaded
  if (!request->_tempObject) {
    log_msg(LOG_ERROR, "Import failed: No file uploaded");
    request->send(400, "text/plain", "No file uploaded");
    return;
  }

  String* fileContent = (String*)request->_tempObject;
  log_msg(LOG_INFO, "Configuration import requested, content length: %zu", fileContent->length());
  
  // Log first 100 characters for debugging (safe for display)
  String preview = fileContent->substring(0, 100);
  log_msg(LOG_DEBUG, "JSON preview: %s", preview.c_str());

  // Parse configuration from uploaded JSON
  Config tempConfig;
  config_init(&tempConfig);
  
  if (!config_load_from_json(&tempConfig, *fileContent)) {
    log_msg(LOG_ERROR, "Import failed: JSON parsing error");
    delete fileContent;  // Clean up
    request->send(400, "text/plain", "Invalid configuration file");
    return;
  }
  
  delete fileContent;  // Clean up

  // Apply imported configuration
  config = tempConfig;
  config_save(&config);
  
  log_msg(LOG_INFO, "Configuration imported successfully, restarting...");
  
  request->send(200, "application/json", 
    "{\"status\":\"ok\",\"message\":\"Configuration imported successfully! Device restarting...\"}");
  
  scheduleReboot(3000);
}

// Get client IP address
void handleClientIP(AsyncWebServerRequest *request) {
  String clientIP = request->client()->remoteIP().toString();
  log_msg(LOG_DEBUG, "Client IP requested: %s", clientIP.c_str());
  request->send(200, "text/plain", clientIP);
}