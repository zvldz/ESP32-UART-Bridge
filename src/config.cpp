#include "config.h"
#include "logging.h"
#include "defines.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "esp_heap_caps.h"

// Configuration constants
#define DEFAULT_BAUDRATE        115200
#define DEFAULT_UDP_PORT        14560
#define JSON_PREVIEW_SIZE       200
#define IP_BUFFER_SIZE          95  // Multiple IPs comma-separated

// Helper function to create optimized JsonDocument for config operations
static JsonDocument createConfigJsonDocument() {
    // For large config JSON, try to use PSRAM if available
    size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (psramFree > 2048) {
        log_msg(LOG_DEBUG, "Config JSON: PSRAM available (%zu KB free)", psramFree/1024);
    } else {
        log_msg(LOG_DEBUG, "Config JSON: Using internal RAM (PSRAM: %zu bytes free)", psramFree);
    }

    return JsonDocument();
}

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

// Helper function to set device role defaults
static void setDeviceDefaults(Config* config) {
    config->device1.role = D1_UART1;
    config->device1.sbusOutputFormat = SBUS_FMT_BINARY;
    config->device2.role = D2_USB;
    config->device2.sbusOutputFormat = SBUS_FMT_BINARY;
    config->device3.role = D3_NONE;
    config->device3.sbusOutputFormat = SBUS_FMT_BINARY;
    config->device4.role = D4_NONE;
}

// Initialize configuration with defaults
void config_init(Config* config) {
    // Set configuration version
    config->config_version = CURRENT_CONFIG_VERSION;

    // Set default values with ESP-IDF types
    config->baudrate = DEFAULT_BAUDRATE;
    config->databits = UART_DATA_8_BITS;
    config->parity = UART_PARITY_DISABLE;
    config->stopbits = UART_STOP_BITS_1;
    config->flowcontrol = false;
    config->ssid = "";  // Empty = auto-generate unique SSID on first AP start
    config->password = DEFAULT_AP_PASSWORD;
#ifdef DEFAULT_PERMANENT_AP
    config->permanent_network_mode = true;  // AP always available on fresh firmware
#else
    config->permanent_network_mode = false;
#endif

    // WiFi Client mode defaults
    config->wifi_mode = BRIDGE_WIFI_MODE_AP;  // Default to AP mode
    for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
        config->wifi_networks[i].ssid = "";
        config->wifi_networks[i].password = "";
    }
    config->wifi_tx_power = DEFAULT_WIFI_TX_POWER;
    config->wifi_ap_channel = 1;  // Default AP channel
    config->mdns_hostname = "";  // Empty = auto-generate on startup
    config->device_version = DEVICE_VERSION;
    config->device_name = DEVICE_NAME;
    config->usb_mode = USB_MODE_DEVICE;  // Default to Device mode for compatibility

    // Device roles defaults
    setDeviceDefaults(config);

    // Device 4 defaults
    strcpy(config->device4_config.target_ip, "");
    config->device4_config.port = DEFAULT_UDP_PORT;
    config->device4_config.role = D4_NONE;
    config->device4_config.auto_broadcast = false;
    config->device4_config.sbusOutputFormat = SBUS_FMT_BINARY;
    config->device4_config.udpSourceTimeout = 1000;  // Default 1 second
    config->device4_config.udpSendRate = 50;  // Default 50 Hz

    // Log levels defaults
    config->log_level_web = LOG_WARNING;
    config->log_level_uart = LOG_WARNING;
    config->log_level_network = LOG_OFF;

    // Protocol optimization default
    config->protocolOptimization = PROTOCOL_NONE;

    // UDP batching default
    config->udpBatchingEnabled = true;

    // MAVLink routing default
    config->mavlinkRouting = false;

    // SBUS settings defaults
    config->sbusTimingKeeper = false;  // Disabled by default

#if defined(BOARD_MINIKIT_ESP32)
    // Device 5 (Bluetooth SPP) defaults
    // Note: BT name uses mdns_hostname, SSP "Just Works" pairing
    config->device5_config.role = D5_NONE;
    config->device5_config.btSendRate = 50;  // 50 Hz default for SBUS Text
#endif
}

// Reset WiFi settings to defaults (AP mode, empty credentials)
// On next boot, unique SSID and hostname will be auto-generated
void config_reset_wifi(Config* config) {
    config->wifi_mode = BRIDGE_WIFI_MODE_AP;
    config->ssid = "";  // Empty = auto-generate unique SSID on startup
    config->password = DEFAULT_AP_PASSWORD;
    for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
        config->wifi_networks[i].ssid = "";
        config->wifi_networks[i].password = "";
    }
    config->mdns_hostname = "";  // Empty = auto-generate on startup
    config->wifi_tx_power = DEFAULT_WIFI_TX_POWER;
}

// Migrate configuration from old versions
// Note: Versions 1-8 were alpha/internal, first public release was v2.18.7 with config v9
void config_migrate(Config* config) {
    // Pre-release configs (v1-8): reset to defaults
    if (config->config_version < 9) {
        log_msg(LOG_WARNING, "Pre-release config v%d detected, resetting to defaults", config->config_version);
        config_init(config);
        return;
    }

    // v9 → v10: Multi-WiFi networks
    // Migration handled in config_load_from_json() via fallback:
    // old client_ssid/client_password → wifi_networks[0]
    if (config->config_version == 9) {
        log_msg(LOG_INFO, "Migrating config v9 → v10 (multi-WiFi networks)");
        config->config_version = CURRENT_CONFIG_VERSION;
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

    uint16_t oldVersion = config->config_version;
    config_load_from_json(config, jsonString);

    // Save config if migration occurred (version changed)
    if (config->config_version != oldVersion) {
        log_msg(LOG_INFO, "Config migrated from v%d to v%d, saving...", oldVersion, config->config_version);
        config_save(config);
    }
}

// Load configuration from JSON string
bool config_load_from_json(Config* config, const String& jsonString) {
    log_msg(LOG_DEBUG, "Parsing JSON config, length: %zu", jsonString.length());

    JsonDocument doc = createConfigJsonDocument();
    DeserializationError error = deserializeJson(doc, jsonString);

    if (error) {
        log_msg(LOG_ERROR, "Failed to parse configuration JSON: %s", error.c_str());
        String jsonPreview = jsonString.substring(0, JSON_PREVIEW_SIZE);
        log_msg(LOG_ERROR, "JSON content: %s", jsonPreview.c_str());  // First 200 chars
        return false;
    }

    log_msg(LOG_DEBUG, "JSON parsed successfully");

    // Missing config_version means version 1 (before versioning was added)
    if (!doc["config_version"].is<int>()) {
        config->config_version = 1;
    } else {
        config->config_version = doc["config_version"] | CURRENT_CONFIG_VERSION;
    }

    // Load UART settings
    if (doc["uart"].is<JsonObject>()) {
        config->baudrate = doc["uart"]["baudrate"] | DEFAULT_BAUDRATE;

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
        config->ssid = doc["wifi"]["ssid"] | "";  // Empty = auto-generate unique SSID
        config->password = doc["wifi"]["password"] | DEFAULT_AP_PASSWORD;
        config->permanent_network_mode = doc["wifi"]["permanent"] | false;

        // Load WiFi mode
        config->wifi_mode = (BridgeWiFiMode)(doc["wifi"]["mode"] | BRIDGE_WIFI_MODE_AP);

        // Load WiFi networks array (new format) or migrate from old format
        if (doc["wifi"]["networks"].is<JsonArray>()) {
            // New format: array of networks
            JsonArray networks = doc["wifi"]["networks"];
            int i = 0;
            for (JsonObject net : networks) {
                if (i >= MAX_WIFI_NETWORKS) break;
                config->wifi_networks[i].ssid = net["ssid"] | "";
                config->wifi_networks[i].password = net["password"] | "";
                i++;
            }
            // Clear remaining slots
            for (; i < MAX_WIFI_NETWORKS; i++) {
                config->wifi_networks[i].ssid = "";
                config->wifi_networks[i].password = "";
            }
        } else {
            // Migration from old format: client_ssid/client_password → networks[0]
            config->wifi_networks[0].ssid = doc["wifi"]["client_ssid"] | "";
            config->wifi_networks[0].password = doc["wifi"]["client_password"] | "";
            for (int i = 1; i < MAX_WIFI_NETWORKS; i++) {
                config->wifi_networks[i].ssid = "";
                config->wifi_networks[i].password = "";
            }
        }

        config->wifi_tx_power = doc["wifi"]["tx_power"] | DEFAULT_WIFI_TX_POWER;
        config->wifi_ap_channel = doc["wifi"]["ap_channel"] | 1;
        config->mdns_hostname = doc["wifi"]["mdns_hostname"] | "";

        // Safety check: AP SSID should not match any client network
        // (could happen due to config corruption or old migration bugs)
        for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
            if (!config->wifi_networks[i].ssid.isEmpty() &&
                config->ssid == config->wifi_networks[i].ssid) {
                log_msg(LOG_WARNING, "AP SSID matches client network #%d, clearing AP SSID", i + 1);
                config->ssid = "";  // Will be auto-generated on next AP start
                break;
            }
        }
    }

    // Load USB settings
    if (doc["usb"].is<JsonObject>()) {
        String mode = doc["usb"]["mode"] | "device";
#if defined(BOARD_MINIKIT_ESP32)
        // MiniKit has no USB Host support (no native USB peripheral)
        config->usb_mode = USB_MODE_DEVICE;
        if (mode == "host") {
            log_msg(LOG_WARNING, "USB Host mode not supported on MiniKit, using Device mode");
        }
#else
        if (mode == "host") {
            config->usb_mode = USB_MODE_HOST;
        } else {
            config->usb_mode = USB_MODE_DEVICE;  // Default to device mode
        }
#endif
    }

    // Load device roles (new in v2)
    if (doc["devices"].is<JsonObject>()) {
        config->device1.role = doc["devices"]["device1"] | D1_UART1;
        config->device2.role = doc["devices"]["device2"] | D2_USB;
        // Load new format, migrate from old bool format if exists
        if (doc["devices"]["device2_sbus_format"].is<int>()) {
            config->device2.sbusOutputFormat = doc["devices"]["device2_sbus_format"] | SBUS_FMT_BINARY;
        } else {
            // Migration: old bool format -> new enum (true -> TEXT, false -> BINARY)
            config->device2.sbusOutputFormat = doc["devices"]["device2_sbus_text"] | false ? SBUS_FMT_TEXT : SBUS_FMT_BINARY;
        }
        config->device3.role = doc["devices"]["device3"] | D3_NONE;
        if (doc["devices"]["device3_sbus_format"].is<int>()) {
            config->device3.sbusOutputFormat = doc["devices"]["device3_sbus_format"] | SBUS_FMT_BINARY;
        } else {
            config->device3.sbusOutputFormat = doc["devices"]["device3_sbus_text"] | false ? SBUS_FMT_TEXT : SBUS_FMT_BINARY;
        }
        config->device4.role = doc["devices"]["device4"] | D4_NONE;
    }

    // Load Device 4 configuration (new in v5)
    if (doc["device4"].is<JsonObject>()) {
        const char* ip = doc["device4"]["target_ip"] | "";
        strncpy(config->device4_config.target_ip, ip, IP_BUFFER_SIZE);
        config->device4_config.target_ip[IP_BUFFER_SIZE] = '\0';
        config->device4_config.port = doc["device4"]["port"] | DEFAULT_UDP_PORT;
        config->device4_config.role = doc["device4"]["role"] | D4_NONE;
        config->device4_config.auto_broadcast = doc["device4"]["auto_broadcast"] | false;
        // Load new format, migrate from old bool format if exists
        if (doc["device4"]["sbus_format"].is<int>()) {
            config->device4_config.sbusOutputFormat = doc["device4"]["sbus_format"] | SBUS_FMT_BINARY;
        } else {
            config->device4_config.sbusOutputFormat = doc["device4"]["sbus_text"] | false ? SBUS_FMT_TEXT : SBUS_FMT_BINARY;
        }
        config->device4_config.udpSourceTimeout = doc["device4"]["udp_timeout"] | 1000;
        config->device4_config.udpSendRate = doc["device4"]["send_rate"] | 50;
    }

#if defined(BOARD_MINIKIT_ESP32)
    // Load Device 5 (Bluetooth SPP) configuration
    // Note: BT name uses mdns_hostname, SSP "Just Works" pairing
    if (doc["device5"].is<JsonObject>()) {
        config->device5_config.role = doc["device5"]["role"] | D5_NONE;
        config->device5_config.btSendRate = doc["device5"]["btSendRate"] | 50;
    }
#endif

    // Load log levels (new in v2)
    if (doc["logging"].is<JsonObject>()) {
        config->log_level_web = (LogLevel)(doc["logging"]["web"] | LOG_WARNING);
        config->log_level_uart = (LogLevel)(doc["logging"]["uart"] | LOG_WARNING);
        config->log_level_network = (LogLevel)(doc["logging"]["network"] | LOG_OFF);
    }

    // Load protocol optimization (new in v7)
    if (doc["protocol"].is<JsonObject>()) {
        config->protocolOptimization = doc["protocol"]["optimization"] | PROTOCOL_NONE;
        config->udpBatchingEnabled = doc["protocol"]["udp_batching"] | true;
        config->mavlinkRouting = doc["protocol"]["mavlink_routing"] | false;
        config->sbusTimingKeeper = doc["protocol"]["sbus_timing_keeper"] | false;
    }

    // System settings like device_version and device_name are NOT loaded from file
    // They always use compiled-in values from defines.h
    config->device_version = DEVICE_VERSION;
    config->device_name = DEVICE_NAME;

    // Migrate if needed
    config_migrate(config);

    // Board-specific role validation
#ifdef DEVICE2_UART_NOT_AVAILABLE
    if (config->device2.role == D2_UART2) {
        log_msg(LOG_WARNING, "D2_UART2 not available on this board, switching to D2_USB");
        config->device2.role = D2_USB;
    }
#endif

    return true;
}

// Helper function to populate JsonDocument with config data
static void populateConfigExportJson(JsonDocument& doc, const Config* config) {
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

    // WiFi mode and client networks
    doc["wifi"]["mode"] = config->wifi_mode;
    JsonArray networks = doc["wifi"]["networks"].to<JsonArray>();
    for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
        if (!config->wifi_networks[i].ssid.isEmpty()) {
            JsonObject net = networks.add<JsonObject>();
            net["ssid"] = config->wifi_networks[i].ssid;
            net["password"] = config->wifi_networks[i].password;
        }
    }
    doc["wifi"]["tx_power"] = config->wifi_tx_power;
    doc["wifi"]["ap_channel"] = config->wifi_ap_channel;
    doc["wifi"]["mdns_hostname"] = config->mdns_hostname;

    // USB settings
    switch(config->usb_mode) {
        case USB_MODE_HOST:
            doc["usb"]["mode"] = "host";
            break;
        case USB_MODE_DEVICE:
        default:
            doc["usb"]["mode"] = "device";
            break;
    }

    // Device roles
    doc["devices"]["device1"] = config->device1.role;
    doc["devices"]["device2"] = config->device2.role;
    doc["devices"]["device2_sbus_format"] = config->device2.sbusOutputFormat;
    doc["devices"]["device3"] = config->device3.role;
    doc["devices"]["device3_sbus_format"] = config->device3.sbusOutputFormat;
    doc["devices"]["device4"] = config->device4.role;

    // Save Device 4 configuration
    doc["device4"]["target_ip"] = config->device4_config.target_ip;
    doc["device4"]["port"] = config->device4_config.port;
    doc["device4"]["role"] = config->device4_config.role;
    doc["device4"]["auto_broadcast"] = config->device4_config.auto_broadcast;
    doc["device4"]["sbus_format"] = config->device4_config.sbusOutputFormat;
    doc["device4"]["udp_timeout"] = config->device4_config.udpSourceTimeout;
    doc["device4"]["send_rate"] = config->device4_config.udpSendRate;

    // Log levels
    doc["logging"]["web"] = config->log_level_web;
    doc["logging"]["uart"] = config->log_level_uart;
    doc["logging"]["network"] = config->log_level_network;

    // Protocol optimization
    doc["protocol"]["optimization"] = config->protocolOptimization;
    doc["protocol"]["udp_batching"] = config->udpBatchingEnabled;
    doc["protocol"]["mavlink_routing"] = config->mavlinkRouting;
    doc["protocol"]["sbus_timing_keeper"] = config->sbusTimingKeeper;

#if defined(BOARD_MINIKIT_ESP32)
    // Device 5 (Bluetooth SPP) configuration
    // Note: BT name uses mdns_hostname, SSP "Just Works" pairing
    doc["device5"]["role"] = config->device5_config.role;
    doc["device5"]["btSendRate"] = config->device5_config.btSendRate;
#endif

    // Note: device_version and device_name are NOT saved - always use compiled values
}

// Convert configuration to JSON string
String config_to_json(Config* config) {
    JsonDocument doc = createConfigJsonDocument();
    populateConfigExportJson(doc, config);

    String result;
    serializeJson(doc, result);
    return result;
}

// Stream configuration JSON directly to Print output
void config_to_json_stream(Print& output, const Config* config) {
    JsonDocument doc = createConfigJsonDocument();
    populateConfigExportJson(doc, config);
    serializeJson(doc, output);
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

#if defined(BOARD_MINIKIT_ESP32)
    // Duplicate D5 config to Preferences for btInUse() early access
    // btInUse() is called by Arduino before LittleFS is mounted
    Preferences prefs;
    prefs.begin("btconfig", false);
    prefs.putUChar("d5_role", config->device5_config.role);
    prefs.putUChar("d5_rate", config->device5_config.btSendRate);
    prefs.end();
#endif
}