#include "config.h"
#include "logging.h"
#include "defines.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// Initialize configuration with defaults
void config_init(Config* config) {
  // Set configuration version
  config->config_version = CURRENT_CONFIG_VERSION;
  
  // Set default values
  config->baudrate = 115200;
  config->databits = 8;
  config->parity = "none";
  config->stopbits = 1;
  config->flowcontrol = false;
  config->ssid = DEFAULT_AP_SSID;
  config->password = DEFAULT_AP_PASSWORD;
  config->device_version = DEVICE_VERSION;
  config->device_name = DEVICE_NAME;
  config->usb_mode = USB_MODE_DEVICE;  // Default to Device mode for compatibility
  
  // Device roles defaults
  config->device1.role = D1_UART1;    // Main bridge always
  config->device2.role = D2_USB;      // USB by default
  config->device3.role = D3_NONE;     // Disabled
  config->device4.role = D4_NONE;     // Disabled
  
  // Log levels defaults
  config->log_level_web = LOG_WARNING;
  config->log_level_uart = LOG_WARNING;
  config->log_level_network = LOG_OFF;
}

// Migrate configuration from old versions
void config_migrate(Config* config) {
  if (config->config_version < 2) {
    log_msg("Migrating config from version " + String(config->config_version) + " to 2", LOG_INFO);
    
    // Set defaults for new fields
    config->device1.role = D1_UART1;
    config->device2.role = D2_USB;
    config->device3.role = D3_NONE;
    config->device4.role = D4_NONE;
    
    // Keep existing usb_mode or default
    if (config->usb_mode != USB_MODE_DEVICE && 
        config->usb_mode != USB_MODE_HOST) {
      config->usb_mode = USB_MODE_DEVICE;
    }
    
    // Default log levels
    config->log_level_web = LOG_WARNING;
    config->log_level_uart = LOG_WARNING;
    config->log_level_network = LOG_OFF;
    
    config->config_version = 2;
  }
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

  // Check if this is old format (no config_version field)
  if (!doc["config_version"].is<int>()) {
    config->config_version = 1;  // Old format
  } else {
    config->config_version = doc["config_version"] | CURRENT_CONFIG_VERSION;
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

  // Load USB settings
  if (doc["usb"].is<JsonObject>()) {
    String mode = doc["usb"]["mode"] | "device";
    if (mode == "host") {
      config->usb_mode = USB_MODE_HOST;
    } else if (mode == "auto") {
      config->usb_mode = USB_MODE_AUTO;
    } else {
      config->usb_mode = USB_MODE_DEVICE;  // Default to device mode
    }
  }

  // Load device roles (new in v2)
  if (doc["devices"].is<JsonObject>()) {
    config->device1.role = doc["devices"]["device1"] | D1_UART1;
    config->device2.role = doc["devices"]["device2"] | D2_USB;
    config->device3.role = doc["devices"]["device3"] | D3_NONE;
    config->device4.role = doc["devices"]["device4"] | D4_NONE;
  }

  // Load log levels (new in v2)
  if (doc["logging"].is<JsonObject>()) {
    config->log_level_web = (LogLevel)(doc["logging"]["web"] | LOG_WARNING);
    config->log_level_uart = (LogLevel)(doc["logging"]["uart"] | LOG_WARNING);
    config->log_level_network = (LogLevel)(doc["logging"]["network"] | LOG_OFF);
  }

  // System settings like device_version and device_name are NOT loaded from file
  // They always use compiled-in values from defines.h
  config->device_version = DEVICE_VERSION;
  config->device_name = DEVICE_NAME;

  // Migrate if needed
  config_migrate(config);
}

// Save configuration to LittleFS
void config_save(Config* config) {
  log_msg("Saving configuration to LittleFS...", LOG_INFO);

  // Create backup of current config
  if (LittleFS.exists("/config.json")) {
    if (LittleFS.exists("/backup.json")) {
      LittleFS.remove("/backup.json");
    }
    LittleFS.rename("/config.json", "/backup.json");
  }

  JsonDocument doc;

  // Configuration version
  doc["config_version"] = CURRENT_CONFIG_VERSION;

  // UART settings
  doc["uart"]["baudrate"] = config->baudrate;
  doc["uart"]["databits"] = config->databits;
  doc["uart"]["parity"] = config->parity;
  doc["uart"]["stopbits"] = config->stopbits;
  doc["uart"]["flowcontrol"] = config->flowcontrol;

  // WiFi settings
  doc["wifi"]["ssid"] = config->ssid;
  doc["wifi"]["password"] = config->password;

  // USB settings
  switch(config->usb_mode) {
    case USB_MODE_HOST:
      doc["usb"]["mode"] = "host";
      break;
    case USB_MODE_AUTO:
      doc["usb"]["mode"] = "auto";
      break;
    case USB_MODE_DEVICE:
    default:
      doc["usb"]["mode"] = "device";
      break;
  }

  // Device roles
  doc["devices"]["device1"] = config->device1.role;
  doc["devices"]["device2"] = config->device2.role;
  doc["devices"]["device3"] = config->device3.role;
  doc["devices"]["device4"] = config->device4.role;

  // Log levels
  doc["logging"]["web"] = config->log_level_web;
  doc["logging"]["uart"] = config->log_level_uart;
  doc["logging"]["network"] = config->log_level_network;

  // Note: device_version and device_name are NOT saved - always use compiled values

  File file = LittleFS.open("/config.json", "w");
  if (!file) {
    log_msg("Failed to create config file", LOG_ERROR);
    return;
  }

  if (serializeJson(doc, file) == 0) {
    log_msg("Failed to write config file", LOG_ERROR);
  } else {
    log_msg("Configuration saved successfully", LOG_INFO);
  }

  file.close();
}