#include "config.h"
#include "logging.h"
#include "defines.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// Initialize configuration with defaults
void config_init(Config* config) {
  // Set default values
  config->baudrate = 115200;
  config->databits = 8;
  config->parity = "none";
  config->stopbits = 1;
  config->flowcontrol = false;
  config->ssid = DEFAULT_AP_SSID;
  config->password = DEFAULT_AP_PASSWORD;
  config->version = DEVICE_VERSION;
  config->device_name = DEVICE_NAME;
}

// Load configuration from LittleFS
void config_load(Config* config) {
  if (!LittleFS.exists("/config.json")) {
    config_save(config);  // Create default config file
    return;
  }

  File file = LittleFS.open("/config.json", "r");
  if (!file) {
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    return;
  }

  // Load UART settings
  if (doc["uart"].is<JsonObject>()) {
    config->baudrate = doc["uart"]["baudrate"] | 115200;
    config->databits = doc["uart"]["databits"] | 8;
    config->parity = doc["uart"]["parity"] | "none";
    config->stopbits = doc["uart"]["stopbits"] | 1;
    config->flowcontrol = doc["uart"]["flowcontrol"] | false;
  }

  // Load WiFi settings
  if (doc["wifi"].is<JsonObject>()) {
    config->ssid = doc["wifi"]["ssid"] | DEFAULT_AP_SSID;
    config->password = doc["wifi"]["password"] | DEFAULT_AP_PASSWORD;
  }

  // System settings like version and device_name are NOT loaded from file
  // They always use compiled-in values from defines.h
}

// Save configuration to LittleFS
void config_save(Config* config) {
  log_msg("Saving configuration to LittleFS...");

  // Create backup of current config
  if (LittleFS.exists("/config.json")) {
    if (LittleFS.exists("/backup.json")) {
      LittleFS.remove("/backup.json");
    }
    LittleFS.rename("/config.json", "/backup.json");
  }

  JsonDocument doc;

  // UART settings
  doc["uart"]["baudrate"] = config->baudrate;
  doc["uart"]["databits"] = config->databits;
  doc["uart"]["parity"] = config->parity;
  doc["uart"]["stopbits"] = config->stopbits;
  doc["uart"]["flowcontrol"] = config->flowcontrol;

  // WiFi settings
  doc["wifi"]["ssid"] = config->ssid;
  doc["wifi"]["password"] = config->password;

  // Note: version and device_name are NOT saved - always use compiled values

  File file = LittleFS.open("/config.json", "w");
  if (!file) {
    log_msg("ERROR: Failed to create config file");
    return;
  }

  if (serializeJson(doc, file) == 0) {
    log_msg("ERROR: Failed to write config file");
  } else {
    log_msg("Configuration saved successfully");
  }

  file.close();
}