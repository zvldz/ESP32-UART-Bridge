#include "config.h"
#include "logging.h"
#include "defines.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// Helper functions for string conversion
const char* parity_to_string(uart_parity_t parity) {
  switch(parity) {
    case UART_PARITY_DISABLE: return "none";
    case UART_PARITY_EVEN: return "even";
    case UART_PARITY_ODD: return "odd";
    default: return "none";
  }
}

uart_parity_t string_to_parity(const char* str) {
  if (strcmp(str, "even") == 0) return UART_PARITY_EVEN;
  if (strcmp(str, "odd") == 0) return UART_PARITY_ODD;
  return UART_PARITY_DISABLE;
}

const char* word_length_to_string(uart_word_length_t length) {
  switch(length) {
    case UART_DATA_7_BITS: return "7";
    case UART_DATA_8_BITS: return "8";
    default: return "8";
  }
}

uart_word_length_t string_to_word_length(uint8_t bits) {
  switch(bits) {
    case 7: return UART_DATA_7_BITS;
    case 8: 
    default: return UART_DATA_8_BITS;
  }
}

const char* stop_bits_to_string(uart_stop_bits_t bits) {
  switch(bits) {
    case UART_STOP_BITS_1: return "1";
    case UART_STOP_BITS_1_5: return "1.5";
    case UART_STOP_BITS_2: return "2";
    default: return "1";
  }
}

uart_stop_bits_t string_to_stop_bits(uint8_t bits) {
  switch(bits) {
    case 2: return UART_STOP_BITS_2;
    case 1:
    default: return UART_STOP_BITS_1;
  }
}

// Initialize configuration with defaults
void config_init(Config* config) {
  // Set configuration version
  config->config_version = CURRENT_CONFIG_VERSION;
  
  // Set default values with ESP-IDF types
  config->baudrate = 115200;
  config->databits = UART_DATA_8_BITS;
  config->parity = UART_PARITY_DISABLE;
  config->stopbits = UART_STOP_BITS_1;
  config->flowcontrol = false;
  config->ssid = DEFAULT_AP_SSID;
  config->password = DEFAULT_AP_PASSWORD;
  config->permanent_network_mode = false;
  
  // WiFi Client mode defaults
  config->wifi_mode = BRIDGE_WIFI_MODE_AP;  // Default to AP mode
  config->wifi_client_ssid = "";
  config->wifi_client_password = "";
  config->device_version = DEVICE_VERSION;
  config->device_name = DEVICE_NAME;
  config->usb_mode = USB_MODE_DEVICE;  // Default to Device mode for compatibility
  
  // Device roles defaults
  config->device1.role = D1_UART1;    // Main bridge always
  config->device2.role = D2_USB;      // USB by default
  config->device3.role = D3_NONE;     // Disabled
  config->device4.role = D4_NONE;     // Disabled
  
  // Device 4 defaults
  strcpy(config->device4_config.target_ip, "");
  config->device4_config.port = 14560;
  config->device4_config.role = D4_NONE;
  
  // Log levels defaults
  config->log_level_web = LOG_WARNING;
  config->log_level_uart = LOG_WARNING;
  config->log_level_network = LOG_OFF;
  
  // Protocol optimization default
  config->protocolOptimization = PROTOCOL_NONE;
}

// Migrate configuration from old versions
void config_migrate(Config* config) {
  if (config->config_version < 2) {
    log_msg(LOG_INFO, "Migrating config from version %d to 2", config->config_version);
    
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
  
  if (config->config_version < 3) {
    log_msg(LOG_INFO, "Migrating config from version %d to 3", config->config_version);
    
    // Version 2 had string/uint8_t types, convert to ESP-IDF enums
    // These will be set from loaded values in config_load
    // Just update version here
    config->config_version = 3;
  }
  
  if (config->config_version < 4) {
    log_msg(LOG_INFO, "Migrating config from version %d to 4", config->config_version);
    config->permanent_network_mode = false;
    config->config_version = 4;
  }
  
  if (config->config_version < 5) {
    log_msg(LOG_INFO, "Migrating config from version %d to 5", config->config_version);
    
    // Set Device 4 defaults
    config->device4.role = D4_NONE;
    strcpy(config->device4_config.target_ip, "");
    config->device4_config.port = 14560;
    config->device4_config.role = D4_NONE;
    
    config->config_version = 5;
  }
  
  if (config->config_version < 6) {
    log_msg(LOG_INFO, "Migrating config from version 5 to 6");
    
    // Add WiFi Client mode fields
    config->wifi_mode = BRIDGE_WIFI_MODE_AP;  // Default to AP mode
    config->wifi_client_ssid = "";      // Empty by default
    config->wifi_client_password = "";  // Empty by default
    
    config->config_version = 6;
  }
  
  if (config->config_version < 7) {
    log_msg(LOG_INFO, "Migrating config from version 6 to 7");
    
    // Add Protocol Optimization field
    config->protocolOptimization = 0;  // PROTOCOL_NONE by default
    
    config->config_version = 7;
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

  String jsonString = file.readString();
  file.close();

  config_load_from_json(config, jsonString);
}

// Load configuration from JSON string
bool config_load_from_json(Config* config, const String& jsonString) {
  log_msg(LOG_DEBUG, "Parsing JSON config, length: %zu", jsonString.length());
  
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonString);

  if (error) {
    log_msg(LOG_ERROR, "Failed to parse configuration JSON: %s", error.c_str());
    String jsonPreview = jsonString.substring(0, 200);
    log_msg(LOG_ERROR, "JSON content: %s", jsonPreview.c_str());  // First 200 chars
    return false;
  }
  
  log_msg(LOG_DEBUG, "JSON parsed successfully");

  // Check if this is old format (no config_version field)
  if (!doc["config_version"].is<int>()) {
    config->config_version = 1;  // Old format
  } else {
    config->config_version = doc["config_version"] | CURRENT_CONFIG_VERSION;
  }

  // Load UART settings
  if (doc["uart"].is<JsonObject>()) {
    config->baudrate = doc["uart"]["baudrate"] | 115200;
    
    // Load string values (current format)
    String databits = doc["uart"]["databits"] | "8";
    config->databits = string_to_word_length(databits.toInt());
    
    String parity = doc["uart"]["parity"] | "none";
    config->parity = string_to_parity(parity.c_str());
    
    String stopbits = doc["uart"]["stopbits"] | "1";
    config->stopbits = string_to_stop_bits(stopbits.toInt());
    
    config->flowcontrol = doc["uart"]["flowcontrol"] | false;
  }

  // Load WiFi settings
  if (doc["wifi"].is<JsonObject>()) {
    config->ssid = doc["wifi"]["ssid"] | DEFAULT_AP_SSID;
    config->password = doc["wifi"]["password"] | DEFAULT_AP_PASSWORD;
    config->permanent_network_mode = doc["wifi"]["permanent"] | false;
    
    // Load WiFi mode and client settings
    config->wifi_mode = (BridgeWiFiMode)(doc["wifi"]["mode"] | BRIDGE_WIFI_MODE_AP);
    config->wifi_client_ssid = doc["wifi"]["client_ssid"] | "";
    config->wifi_client_password = doc["wifi"]["client_password"] | "";
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

  // Load Device 4 configuration (new in v5)
  if (doc["device4"].is<JsonObject>()) {
    const char* ip = doc["device4"]["target_ip"] | "";
    strncpy(config->device4_config.target_ip, ip, 15);
    config->device4_config.target_ip[15] = '\0';
    config->device4_config.port = doc["device4"]["port"] | 14560;
    config->device4_config.role = doc["device4"]["role"] | D4_NONE;
  }

  // Load log levels (new in v2)
  if (doc["logging"].is<JsonObject>()) {
    config->log_level_web = (LogLevel)(doc["logging"]["web"] | LOG_WARNING);
    config->log_level_uart = (LogLevel)(doc["logging"]["uart"] | LOG_WARNING);
    config->log_level_network = (LogLevel)(doc["logging"]["network"] | LOG_OFF);
  }

  // Load protocol optimization (new in v7)
  if (doc["protocol"].is<JsonObject>()) {
    config->protocolOptimization = doc["protocol"]["optimization"] | 0;  // PROTOCOL_NONE by default
  }

  // System settings like device_version and device_name are NOT loaded from file
  // They always use compiled-in values from defines.h
  config->device_version = DEVICE_VERSION;
  config->device_name = DEVICE_NAME;

  // Migrate if needed
  config_migrate(config);
  
  return true;
}

// Convert configuration to JSON string
String config_to_json(Config* config) {
  JsonDocument doc;

  // Configuration version
  doc["config_version"] = CURRENT_CONFIG_VERSION;

  // UART settings
  doc["uart"]["baudrate"] = config->baudrate;
  doc["uart"]["databits"] = word_length_to_string(config->databits);
  doc["uart"]["parity"] = parity_to_string(config->parity);
  doc["uart"]["stopbits"] = stop_bits_to_string(config->stopbits);
  doc["uart"]["flowcontrol"] = config->flowcontrol;

  // WiFi settings
  doc["wifi"]["ssid"] = config->ssid;
  doc["wifi"]["password"] = config->password;
  doc["wifi"]["permanent"] = config->permanent_network_mode;
  
  // WiFi mode and client settings
  doc["wifi"]["mode"] = config->wifi_mode;
  doc["wifi"]["client_ssid"] = config->wifi_client_ssid;
  doc["wifi"]["client_password"] = config->wifi_client_password;

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

  // Save Device 4 configuration
  doc["device4"]["target_ip"] = config->device4_config.target_ip;
  doc["device4"]["port"] = config->device4_config.port;
  doc["device4"]["role"] = config->device4_config.role;

  // Log levels
  doc["logging"]["web"] = config->log_level_web;
  doc["logging"]["uart"] = config->log_level_uart;
  doc["logging"]["network"] = config->log_level_network;

  // Protocol optimization
  doc["protocol"]["optimization"] = config->protocolOptimization;

  // Note: device_version and device_name are NOT saved - always use compiled values

  String result;
  serializeJson(doc, result);
  return result;
}

// Save configuration to LittleFS
void config_save(Config* config) {
  log_msg(LOG_INFO, "Saving configuration to LittleFS...");

  // Create backup of current config
  if (LittleFS.exists("/config.json")) {
    if (LittleFS.exists("/backup.json")) {
      LittleFS.remove("/backup.json");
    }
    LittleFS.rename("/config.json", "/backup.json");
  }

  // Get configuration JSON
  String jsonString = config_to_json(config);

  File file = LittleFS.open("/config.json", "w");
  if (!file) {
    log_msg(LOG_ERROR, "Failed to create config file");
    return;
  }

  if (file.print(jsonString) == 0) {
    log_msg(LOG_ERROR, "Failed to write config file");
  } else {
    log_msg(LOG_INFO, "Configuration saved successfully");
  }

  file.close();
}