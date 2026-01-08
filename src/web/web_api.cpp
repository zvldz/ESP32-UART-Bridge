#include "protocols/protocol_stats.h"
#include "web_api.h"
#include "web_interface.h"
#include "esp_heap_caps.h"
#include "logging.h"
#include "config.h"
#include "../device_stats.h"
#include "../uart/uartbridge.h"
#include "defines.h"
#include "crashlog.h"
#include "diagnostics.h"
#include "scheduler_tasks.h"
#include "../wifi/wifi_manager.h"
#include "protocols/protocol_pipeline.h"
#include "protocols/sbus_router.h"
#include "protocols/sbus_fast_parser.h"
#if defined(BOARD_MINIKIT_ESP32)
#include "../bluetooth/bluetooth_spp.h"
#endif
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "esp_system.h"           // for esp_get_idf_version()
#include "esp_arduino_version.h"  // for ESP_ARDUINO_VERSION_STR
#include "esp_heap_caps.h"        // for PSRAM allocation


// External objects from main.cpp
extern Config config;
extern SystemState systemState;
extern BridgeMode bridgeMode;
extern SemaphoreHandle_t logMutex;
extern UartInterface* uartBridgeSerial;

// Validate SBUS configuration
bool validateSbusConfig(const Config& cfg) {
    // Check if there are SBUS_IN without any SBUS_OUT
    bool hasSbusIn = (cfg.device1.role == D1_SBUS_IN ||
                      cfg.device2.role == D2_SBUS_IN ||
                      cfg.device3.role == D3_SBUS_IN ||
                      cfg.device4.role == D4_SBUS_UDP_RX);
    bool hasSbusOut = (cfg.device2.role == D2_SBUS_OUT ||
                       cfg.device3.role == D3_SBUS_OUT ||
                       cfg.device4.role == D4_SBUS_UDP_TX
#if defined(BOARD_MINIKIT_ESP32)
                       || cfg.device5_config.role == D5_BT_SBUS_TEXT
#endif
                       );

    if (hasSbusIn && !hasSbusOut) {
        log_msg(LOG_ERROR, "SBUS_IN devices require at least one SBUS_OUT device");
        return false;
    }

    return true;
}

// Generate complete configuration JSON for initial page load
// Helper function to populate JsonDocument with config data
static void populateConfigJson(JsonDocument& doc) {
    // System info
    doc["deviceName"] = config.device_name;
    doc["version"] = config.device_version;
    doc["arduinoVersion"] = ESP_ARDUINO_VERSION_STR;  // NEW - Arduino Core version
    doc["idfVersion"] = esp_get_idf_version();        // NEW - ESP-IDF version
    doc["freeRam"] = ESP.getFreeHeap();

    // Board identification with compile-time verification
#if defined(BOARD_ESP32_S3_SUPER_MINI)
    doc["boardType"] = "s3supermini";
    doc["usbHostSupported"] = false;
#elif defined(BOARD_XIAO_ESP32_S3)
    doc["boardType"] = "xiao";
    doc["usbHostSupported"] = true;
#elif defined(BOARD_MINIKIT_ESP32)
    doc["boardType"] = "minikit";
    doc["usbHostSupported"] = false;
    doc["uart2Available"] = false;  // D2_UART2 not available (GPIO 8/9 conflict)
#elif defined(BOARD_ESP32_S3_ZERO)
    doc["boardType"] = "s3zero";
    doc["usbHostSupported"] = true;
#else
    // Default fallback to S3-Zero if no board type specified
    doc["boardType"] = "s3zero";
    doc["usbHostSupported"] = true;
#endif

    // Compile-time verification
#if defined(BOARD_ESP32_S3_SUPER_MINI) && defined(BOARD_ESP32_S3_ZERO)
    #error "Both BOARD_ESP32_S3_SUPER_MINI and BOARD_ESP32_S3_ZERO are defined - this should not happen"
#endif

    // Feature flags for Web UI
#ifdef SBUS_MAVLINK_SUPPORT
    doc["sbusMavlinkEnabled"] = true;
#else
    doc["sbusMavlinkEnabled"] = false;
#endif

    // Calculate uptime
    doc["uptime"] = (millis() - g_deviceStats.systemStartTime.load(std::memory_order_relaxed)) / MS_TO_SECONDS;

    // Current configuration
    doc["baudrate"] = config.baudrate;
    doc["databits"] = atoi(word_length_to_string(config.databits));
    doc["parity"] = parity_to_string(config.parity);
    doc["stopbits"] = atoi(stop_bits_to_string(config.stopbits));
    doc["flowcontrol"] = config.flowcontrol;

    // WiFi
    doc["ssid"] = config.ssid;
    doc["password"] = config.password;
    doc["permanentWifi"] = config.permanent_network_mode;

    // WiFi mode and client settings
    doc["wifiMode"] = config.wifi_mode;
    doc["wifiTxPower"] = config.wifi_tx_power;
    doc["wifiApChannel"] = config.wifi_ap_channel;
    doc["mdnsHostname"] = config.mdns_hostname;

    // WiFi networks array for Client mode
    JsonArray networks = doc["wifiNetworks"].to<JsonArray>();
    for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
        JsonObject net = networks.add<JsonObject>();
        net["ssid"] = config.wifi_networks[i].ssid;
        net["password"] = config.wifi_networks[i].password;
    }

    // Client mode status
    if (config.wifi_mode == BRIDGE_WIFI_MODE_CLIENT) {
        doc["wifiClientConnected"] = systemState.wifiClientConnected;
        if (systemState.wifiClientConnected) {
            doc["connectedSSID"] = wifiGetConnectedSSID();
            doc["ipAddress"] = wifiGetIP();
            doc["rssiPercent"] = rssiToPercent(wifiGetRSSI());
        }
    }

    // USB mode
    doc["usbMode"] = config.usb_mode == USB_MODE_HOST ? "host" : "device";

    // Device roles
    doc["device1Role"] = String(config.device1.role);
    doc["device2Role"] = String(config.device2.role);
    doc["device3Role"] = String(config.device3.role);
    doc["device4Role"] = String(config.device4.role);

    // Device role names
    doc["device1RoleName"] = getDevice1RoleName(config.device1.role);
    doc["device2RoleName"] = getDevice2RoleName(config.device2.role);
    doc["device3RoleName"] = getDevice3RoleName(config.device3.role);
    doc["device4RoleName"] = getDevice4RoleName(config.device4.role);

#if defined(BOARD_MINIKIT_ESP32)
    // Device 5 (Bluetooth SPP)
    // Note: BT name uses mdnsHostname, SSP "Just Works" pairing
    doc["device5Role"] = String(config.device5_config.role);
    doc["device5RoleName"] = getDevice5RoleName(config.device5_config.role);
    doc["btSendRate"] = config.device5_config.btSendRate;
    // BT runtime status
    doc["btInitialized"] = (bluetoothSPP != nullptr);
    doc["btConnected"] = (bluetoothSPP != nullptr && bluetoothSPP->isConnected());
#endif

    // Device 4 configuration
    doc["device4TargetIp"] = config.device4_config.target_ip;
    doc["device4Port"] = config.device4_config.port;
    doc["device4AutoBroadcast"] = config.device4_config.auto_broadcast;
    doc["device4UdpTimeout"] = config.device4_config.udpSourceTimeout;
    doc["device4SendRate"] = config.device4_config.udpSendRate;

    // SBUS output format options (for SBUS_OUT roles)
    doc["device2SbusFormat"] = config.device2.sbusOutputFormat;
    doc["device3SbusFormat"] = config.device3.sbusOutputFormat;
    doc["device4SbusFormat"] = config.device4_config.sbusOutputFormat;

    // Log levels
    doc["logLevelWeb"] = (int)config.log_level_web;
    doc["logLevelUart"] = (int)config.log_level_uart;
    doc["logLevelNetwork"] = (int)config.log_level_network;

    // UART configuration string - use char buffer to avoid String concatenation
    char uartConfigBuf[32];
    snprintf(uartConfigBuf, sizeof(uartConfigBuf), "%d baud, %s%c%s",
             config.baudrate,
             word_length_to_string(config.databits),
             parity_to_string(config.parity)[0],  // First char only (N/E/O)
             stop_bits_to_string(config.stopbits));
    doc["uartConfig"] = uartConfigBuf;

    // Flow control status
    if (uartBridgeSerial) {
        doc["flowControl"] = uartBridgeSerial->getFlowControlStatus();
    } else {
        doc["flowControl"] = "Not initialized";
    }

    // Statistics
    doc["device1Rx"] = g_deviceStats.device1.rxBytes.load(std::memory_order_relaxed);
    doc["device1Tx"] = g_deviceStats.device1.txBytes.load(std::memory_order_relaxed);
    doc["device2Rx"] = g_deviceStats.device2.rxBytes.load(std::memory_order_relaxed);
    doc["device2Tx"] = g_deviceStats.device2.txBytes.load(std::memory_order_relaxed);
    doc["device3Rx"] = g_deviceStats.device3.rxBytes.load(std::memory_order_relaxed);
    doc["device3Tx"] = g_deviceStats.device3.txBytes.load(std::memory_order_relaxed);

    // Device4 statistics - KEEP EXACT FIELD NAMES for backward compatibility
    if (config.device4.role != D4_NONE && systemState.networkActive) {
        doc["device4TxBytes"] = g_deviceStats.device4.txBytes.load(std::memory_order_relaxed);
        doc["device4TxPackets"] = g_deviceStats.device4.txPackets.load(std::memory_order_relaxed);
        doc["device4RxBytes"] = g_deviceStats.device4.rxBytes.load(std::memory_order_relaxed);
        doc["device4RxPackets"] = g_deviceStats.device4.rxPackets.load(std::memory_order_relaxed);
    }

    // Calculate total traffic (local UART/USB only, not network)
    unsigned long totalTraffic =
        g_deviceStats.device1.rxBytes.load(std::memory_order_relaxed) +
        g_deviceStats.device1.txBytes.load(std::memory_order_relaxed) +
        g_deviceStats.device2.rxBytes.load(std::memory_order_relaxed) +
        g_deviceStats.device2.txBytes.load(std::memory_order_relaxed) +
        g_deviceStats.device3.rxBytes.load(std::memory_order_relaxed) +
        g_deviceStats.device3.txBytes.load(std::memory_order_relaxed);
    doc["totalTraffic"] = totalTraffic;

    // Last activity - use char buffer to avoid String concatenation
    unsigned long lastActivity = g_deviceStats.lastGlobalActivity.load(std::memory_order_relaxed);
    if (lastActivity == 0) {
        doc["lastActivity"] = "Never";
    } else {
        char activityBuf[32];
        snprintf(activityBuf, sizeof(activityBuf), "%lu seconds ago",
                 (millis() - lastActivity) / MS_TO_SECONDS);
        doc["lastActivity"] = activityBuf;
    }

    // Protocol optimization configuration
    doc["protocolOptimization"] = config.protocolOptimization;
    doc["udpBatchingEnabled"] = config.udpBatchingEnabled;
    doc["mavlinkRouting"] = config.mavlinkRouting;

    // Protocol statistics via Pipeline
    BridgeContext* ctx = getBridgeContext();
    if (ctx && ctx->protocolPipeline) {
        ctx->protocolPipeline->appendStatsToJson(doc);
    }

    // Log display count
    doc["logDisplayCount"] = LOG_DISPLAY_COUNT;
}

String getConfigJson() {
    JsonDocument doc;
    populateConfigJson(doc);

    String json;
    serializeJson(doc, json);
    return json;
}

void getConfigJson(Print& output) {
    JsonDocument doc;
    populateConfigJson(doc);
    serializeJson(doc, output);
}

// Helper function to populate JsonDocument with logs data
static void populateLogsJson(JsonDocument& doc) {
    JsonArray logs = doc["logs"].to<JsonArray>();

    String logBuffer[LOG_DISPLAY_COUNT];
    int actualCount = 0;
    logging_get_recent_logs(logBuffer, LOG_DISPLAY_COUNT, &actualCount);

    for (int i = 0; i < actualCount; i++) {
        logs.add(logBuffer[i]);
    }
}

void writeLogsJson(Print& output) {
    JsonDocument doc;
    populateLogsJson(doc);
    serializeJson(doc, output);
}

// Handle status request - return full config and stats (used by AJAX)
void handleStatus(AsyncWebServerRequest *request) {
    // Stream JSON response to avoid large String allocation
    auto* res = request->beginResponseStream("application/json");
    res->addHeader("Connection", "close");

    getConfigJson(*res);
    request->send(res);
}

// Handle logs request
void handleLogs(AsyncWebServerRequest *request) {
    // Stream JSON response to avoid large String allocation
    auto* res = request->beginResponseStream("application/json");
    res->addHeader("Connection", "close");

    writeLogsJson(*res);
    request->send(res);
}

// Handle save configuration
void handleSave(AsyncWebServerRequest *request) {
    log_msg(LOG_INFO, "Saving new configuration...");

    // Parse form data
    bool configChanged = false;

    // UART settings
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

        #if defined(BOARD_ESP32_S3_SUPER_MINI)
        // Block USB Host mode on Super Mini
        if (mode == "host") {
            log_msg(LOG_WARNING, "USB Host mode not supported on Super Mini, using Device mode");
            newMode = USB_MODE_DEVICE;
        }
        #elif defined(BOARD_MINIKIT_ESP32)
        // Block USB Host mode on MiniKit (no native USB peripheral)
        if (mode == "host") {
            log_msg(LOG_WARNING, "USB Host mode not supported on MiniKit, using Device mode");
            newMode = USB_MODE_DEVICE;
        }
        #elif defined(BOARD_ESP32_S3_ZERO)
        if (mode == "host") {
            newMode = USB_MODE_HOST;
        }
        #else
        // Default S3-Zero behavior - USB Host supported
        if (mode == "host") {
            newMode = USB_MODE_HOST;
        }
        #endif

        if (newMode != config.usb_mode) {
            config.usb_mode = newMode;
            configChanged = true;
            log_msg(LOG_INFO, "USB mode changed to %s", newMode == USB_MODE_HOST ? "host" : "device");
        }
    }

    // Device 1 role (NEW!)
    if (request->hasParam("device1_role", true)) {
        const AsyncWebParameter* p = request->getParam("device1_role", true);
        int role = p->value().toInt();
        if (role >= D1_UART1 && role <= D1_SBUS_IN) {
            if (role != config.device1.role) {
                config.device1.role = role;
                configChanged = true;
                const char* roleName = (role == D1_SBUS_IN) ? "SBUS_IN" : "UART Bridge";
                log_msg(LOG_INFO, "Device 1 role changed to %s", roleName);
            }
        }
    }

    // Device 2 role
    if (request->hasParam("device2_role", true)) {
        const AsyncWebParameter* p = request->getParam("device2_role", true);
        int role = p->value().toInt();
        if (role >= D2_NONE && role <= D2_SBUS_OUT) {
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
        if (role >= D3_NONE && role <= D3_SBUS_OUT) {
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
        if (role >= D4_NONE && role <= D4_SBUS_UDP_RX) {
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
        strncpy(config.device4_config.target_ip, ip.c_str(), IP_ADDRESS_BUFFER_SIZE);
        config.device4_config.target_ip[IP_ADDRESS_BUFFER_SIZE] = '\0';
        configChanged = true;
        log_msg(LOG_INFO, "Device 4 target IP changed to %s", ip.c_str());
    }
    if (request->hasParam("device4_port", true)) {
        String portStr = request->getParam("device4_port", true)->value();
        config.device4_config.port = portStr.toInt();
        configChanged = true;
        log_msg(LOG_INFO, "Device 4 port changed to %s", portStr.c_str());
    }

    // Device 4 auto broadcast (checkbox)
    bool newAutoBroadcast = request->hasParam("device4_auto_broadcast", true);
    if (newAutoBroadcast != config.device4_config.auto_broadcast) {
        config.device4_config.auto_broadcast = newAutoBroadcast;
        configChanged = true;
        log_msg(LOG_INFO, "Device 4 auto broadcast %s", newAutoBroadcast ? "enabled" : "disabled");
    }

    // Device 4 UDP source timeout
    if (request->hasParam("device4_udp_timeout", true)) {
        uint16_t newTimeout = request->getParam("device4_udp_timeout", true)->value().toInt();
        if (newTimeout >= 100 && newTimeout <= 5000 && newTimeout != config.device4_config.udpSourceTimeout) {
            config.device4_config.udpSourceTimeout = newTimeout;
            configChanged = true;
            log_msg(LOG_INFO, "Device 4 UDP timeout: %u ms", newTimeout);
        }
    }

    // Device 4 UDP send rate
    if (request->hasParam("device4_send_rate", true)) {
        uint8_t newRate = request->getParam("device4_send_rate", true)->value().toInt();
        if (newRate >= 10 && newRate <= 70 && newRate != config.device4_config.udpSendRate) {
            config.device4_config.udpSendRate = newRate;
            configChanged = true;
            log_msg(LOG_INFO, "Device 4 send rate: %u Hz", newRate);
        }
    }

    // Copy role to configuration
    config.device4_config.role = config.device4.role;

    // SBUS output format options (select dropdowns)
    static const char* fmtNames[] = {"binary", "text", "mavlink"};
    if (request->hasParam("device2_sbus_format", true)) {
        uint8_t newFmt = request->getParam("device2_sbus_format", true)->value().toInt();
        if (newFmt != config.device2.sbusOutputFormat && newFmt <= 2) {
            config.device2.sbusOutputFormat = newFmt;
            configChanged = true;
            log_msg(LOG_INFO, "Device 2 SBUS output format: %s", fmtNames[newFmt]);
        }
    }
    if (request->hasParam("device3_sbus_format", true)) {
        uint8_t newFmt = request->getParam("device3_sbus_format", true)->value().toInt();
        if (newFmt != config.device3.sbusOutputFormat && newFmt <= 2) {
            config.device3.sbusOutputFormat = newFmt;
            configChanged = true;
            log_msg(LOG_INFO, "Device 3 SBUS output format: %s", fmtNames[newFmt]);
        }
    }
    if (request->hasParam("device4_sbus_format", true)) {
        uint8_t newFmt = request->getParam("device4_sbus_format", true)->value().toInt();
        if (newFmt != config.device4_config.sbusOutputFormat && newFmt <= 2) {
            config.device4_config.sbusOutputFormat = newFmt;
            configChanged = true;
            log_msg(LOG_INFO, "Device 4 SBUS output format: %s", fmtNames[newFmt]);
        }
    }

#if defined(BOARD_MINIKIT_ESP32)
    // Device 5 role (Bluetooth SPP)
    if (request->hasParam("device5_role", true)) {
        const AsyncWebParameter* p = request->getParam("device5_role", true);
        int role = p->value().toInt();
        if (role >= D5_NONE && role <= D5_BT_SBUS_TEXT) {
            if (role != config.device5_config.role) {
                config.device5_config.role = role;
                configChanged = true;
                log_msg(LOG_INFO, "Device 5 role changed to %d", role);
            }
        }
    }

    // Device 5 BT send rate (for SBUS Text mode)
    if (request->hasParam("bt_send_rate", true)) {
        const AsyncWebParameter* p = request->getParam("bt_send_rate", true);
        int rate = p->value().toInt();
        if (rate >= 10 && rate <= 70) {
            if (rate != config.device5_config.btSendRate) {
                config.device5_config.btSendRate = rate;
                configChanged = true;
                log_msg(LOG_INFO, "BT send rate changed to %d Hz", rate);
            }
        }
    }

    // Note: BT name uses mdns_hostname, SSP "Just Works" pairing (no PIN)
#endif

    // Validate SBUS configuration before saving
    if (!validateSbusConfig(config)) {
        request->send(400, "application/json",
            "{\"status\":\"error\",\"message\":\"Invalid SBUS configuration. Check device roles.\"}");
        return;
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
                // Reinitialize Pipeline with new configuration
                if (ctx->protocolPipeline) {
                    delete ctx->protocolPipeline;
                    ctx->protocolPipeline = new ProtocolPipeline(ctx);
                    ctx->protocolPipeline->init(&config);
                }
                log_msg(LOG_DEBUG, "Protocol pipeline reinitialized");
            } else {
                log_msg(LOG_WARNING, "Warning: BridgeContext not available for protocol reinit");
            }
        }
    }

    // UDP batching control
    if (request->hasParam("udp_batching", true)) {
        // Checkbox present = true, absent = false
        bool newBatching = true;
        if (config.udpBatchingEnabled != newBatching) {
            config.udpBatchingEnabled = newBatching;
            configChanged = true;
            log_msg(LOG_INFO, "UDP batching enabled");
        }
    } else {
        // Checkbox not present = false
        bool newBatching = false;
        if (config.udpBatchingEnabled != newBatching) {
            config.udpBatchingEnabled = newBatching;
            configChanged = true;
            log_msg(LOG_INFO, "UDP batching disabled");
        }
    }

    // MAVLink routing control
    if (request->hasParam("mavlink_routing", true)) {
        // Checkbox present = true, absent = false
        bool newRouting = true;
        if (config.mavlinkRouting != newRouting) {
            config.mavlinkRouting = newRouting;
            configChanged = true;
            log_msg(LOG_INFO, "MAVLink routing enabled");
        }
    } else {
        // Checkbox not present = false
        bool newRouting = false;
        if (config.mavlinkRouting != newRouting) {
            config.mavlinkRouting = newRouting;
            configChanged = true;
            log_msg(LOG_INFO, "MAVLink routing disabled");
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

    // WiFi networks array for Client mode
    bool wifiNetworksChanged = false;
    for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
        char ssidParam[32], passParam[32];
        snprintf(ssidParam, sizeof(ssidParam), "wifi_network_ssid_%d", i);
        snprintf(passParam, sizeof(passParam), "wifi_network_pass_%d", i);

        if (request->hasParam(ssidParam, true)) {
            String newSSID = request->getParam(ssidParam, true)->value();
            newSSID.trim();

            // Validate primary network (index 0) when in Client mode
            if (i == 0 && config.wifi_mode == BRIDGE_WIFI_MODE_CLIENT && newSSID.isEmpty()) {
                log_msg(LOG_ERROR, "Primary network SSID cannot be empty in Client mode");
                request->send(400, "application/json",
                    "{\"status\":\"error\",\"message\":\"Primary network SSID cannot be empty\"}");
                return;
            }

            if (newSSID != config.wifi_networks[i].ssid) {
                config.wifi_networks[i].ssid = newSSID;
                wifiNetworksChanged = true;
                if (!newSSID.isEmpty()) {
                    log_msg(LOG_INFO, "WiFi network %d SSID changed to %s", i + 1, newSSID.c_str());
                }
            }
        }

        if (request->hasParam(passParam, true)) {
            String newPassword = request->getParam(passParam, true)->value();

            // Validate password (empty allowed for open networks, otherwise min 8 chars)
            if (newPassword.length() > 0 && newPassword.length() < 8) {
                log_msg(LOG_ERROR, "Network %d password must be at least 8 characters or empty", i + 1);
                request->send(400, "application/json",
                    "{\"status\":\"error\",\"message\":\"WiFi password must be at least 8 characters or empty\"}");
                return;
            }

            if (newPassword != config.wifi_networks[i].password) {
                config.wifi_networks[i].password = newPassword;
                wifiNetworksChanged = true;
                log_msg(LOG_INFO, "WiFi network %d password updated", i + 1);
            }
        }
    }

    if (wifiNetworksChanged) {
        configChanged = true;
        // Reset auth failure flags when networks are changed
        wifiResetAuthFlags();
    }

    // WiFi TX Power
    if (request->hasParam("wifi_tx_power", true)) {
        const AsyncWebParameter* p = request->getParam("wifi_tx_power", true);
        uint8_t newTxPower = p->value().toInt();

        // Validate range (8-80 for ESP-IDF)
        if (newTxPower < 8 || newTxPower > 80) {
            log_msg(LOG_ERROR, "WiFi TX Power must be between 8 and 80");
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"WiFi TX Power must be between 8 and 80\"}");
            return;
        }

        if (newTxPower != config.wifi_tx_power) {
            config.wifi_tx_power = newTxPower;
            configChanged = true;
            log_msg(LOG_INFO, "WiFi TX Power updated to %d (%.1fdBm)", newTxPower, newTxPower * 0.25);
        }
    }

    // WiFi AP Channel
    if (request->hasParam("wifi_ap_channel", true)) {
        const AsyncWebParameter* p = request->getParam("wifi_ap_channel", true);
        uint8_t newChannel = p->value().toInt();

        // Validate range (1-13)
        if (newChannel < 1 || newChannel > 13) {
            log_msg(LOG_ERROR, "WiFi AP Channel must be between 1 and 13");
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"WiFi AP Channel must be between 1 and 13\"}");
            return;
        }

        if (newChannel != config.wifi_ap_channel) {
            config.wifi_ap_channel = newChannel;
            configChanged = true;
            log_msg(LOG_INFO, "WiFi AP Channel updated to %d", newChannel);
        }
    }

    // mDNS Hostname
    if (request->hasParam("mdns_hostname", true)) {
        const AsyncWebParameter* p = request->getParam("mdns_hostname", true);
        String newHostname = p->value();
        newHostname.toLowerCase();
        newHostname.trim();

        // Validate hostname (DNS rules: a-z, 0-9, -, max 63 chars, no start/end with -)
        bool valid = true;
        if (newHostname.length() > 63) {
            valid = false;
        } else if (newHostname.length() > 0) {
            // Check first and last characters
            if (newHostname.charAt(0) == '-' || newHostname.charAt(newHostname.length() - 1) == '-') {
                valid = false;
            }
            // Check all characters
            for (size_t i = 0; i < newHostname.length() && valid; i++) {
                char c = newHostname.charAt(i);
                if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
                    valid = false;
                }
            }
        }

        if (!valid) {
            log_msg(LOG_ERROR, "Invalid mDNS hostname: must be lowercase, a-z/0-9/-, max 63 chars");
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid hostname: use a-z, 0-9, - only, max 63 chars\"}");
            return;
        }

        if (newHostname != config.mdns_hostname) {
            config.mdns_hostname = newHostname;
            configChanged = true;
            log_msg(LOG_INFO, "mDNS hostname updated to: %s", newHostname.c_str());
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
    // Use helper function with current time for g_deviceStats reset
    resetDeviceStatistics(g_deviceStats, millis());

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

// Handle test crash request - triggers intentional crash for coredump testing
void handleTestCrash(AsyncWebServerRequest *request) {
    log_msg(LOG_WARNING, "Test crash requested via web interface");
    request->send(200, "text/plain", "Triggering test crash in 2 seconds...\nDevice will reboot and crash log will be available after restart.");

    // Give time for response to be sent
    delay(2000);

    // Trigger null pointer dereference (LoadProhibited exception)
    // cppcheck-suppress nullPointer
    volatile int* null_ptr = nullptr;
    *null_ptr = 42;  // Intentional crash for coredump testing
}

// Export configuration as downloadable JSON file
void handleExportConfig(AsyncWebServerRequest *request) {
    log_msg(LOG_INFO, "Configuration export requested");

    // Use mDNS hostname for filename (identifies device)
    char filename[80];
    snprintf(filename, sizeof(filename), "%s-config.json", config.mdns_hostname.c_str());

    // Stream JSON response to avoid large String allocation
    auto* res = request->beginResponseStream("application/json");

    // Build Content-Disposition header with filename
    char contentDisposition[128];
    snprintf(contentDisposition, sizeof(contentDisposition),
             "attachment; filename=\"%s\"", filename);
    res->addHeader("Content-Disposition", contentDisposition);
    res->addHeader("Connection", "close");

    config_to_json_stream(*res, &config);
    request->send(res);
}

// Factory reset - restore all settings to defaults
void handleFactoryReset(AsyncWebServerRequest *request) {
    log_msg(LOG_WARNING, "Factory reset requested via web interface");

    // Reset config to defaults
    config_init(&config);
    config_save(&config);

    log_msg(LOG_INFO, "Configuration reset to factory defaults");

    // Send response before reboot
    request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Factory reset complete\"}");

    // Schedule reboot
    delay(500);
    ESP.restart();
}

// Import configuration from uploaded JSON file
void handleImportConfig(AsyncWebServerRequest *request) {
    // Check if file data was uploaded
    if (!request->_tempObject) {
        log_msg(LOG_ERROR, "Import failed: No file uploaded");
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"No file uploaded\"}");
        return;
    }

    ImportData* importData = static_cast<ImportData*>(request->_tempObject);
    log_msg(LOG_INFO, "Configuration import requested, content length: %zu", importData->len);

    // Log first 100 characters for debugging (safe for display)
    size_t previewLen = importData->len > 100 ? 100 : importData->len;
    char preview[101];
    strncpy(preview, importData->ptr, previewLen);
    preview[previewLen] = '\0';
    log_msg(LOG_DEBUG, "JSON preview: %s", preview);

    // Parse configuration from PSRAM buffer
    Config tempConfig;
    config_init(&tempConfig);

    // Create String from buffer for compatibility with existing config_load_from_json
    String jsonString(importData->ptr);

    if (!config_load_from_json(&tempConfig, jsonString)) {
        log_msg(LOG_ERROR, "Import failed: JSON parsing error");
        heap_caps_free(importData->ptr);
        delete importData;
        request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid configuration file\"}");
        return;
    }

    // Clean up PSRAM buffer
    heap_caps_free(importData->ptr);
    delete importData;
    request->_tempObject = nullptr;

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

// Handle SBUS source switching
void handleSbusSetSource(AsyncWebServerRequest *request) {
    if (!request->hasParam("source")) {
        request->send(400, "application/json",
            "{\"status\":\"error\",\"message\":\"Missing source parameter\"}");
        return;
    }

    String sourceStr = request->getParam("source")->value();
    uint8_t source = sourceStr.toInt();

    if (source > 2) {
        request->send(400, "application/json",
            "{\"status\":\"error\",\"message\":\"Invalid source\"}");
        return;
    }

    SbusRouter* router = SbusRouter::getInstance();

    // Switch to manual mode and set source
    router->setMode(SbusRouter::MODE_MANUAL);
    router->setManualSource(source);

    JsonDocument doc;
    doc["status"] = "ok";
    doc["source"] = source;
    doc["mode"] = "manual";

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);

    log_msg(LOG_INFO, "SBUS manual source set to %d", source);
}

void handleSbusSetMode(AsyncWebServerRequest *request) {
    if (!request->hasParam("mode")) {
        request->send(400, "application/json",
            "{\"status\":\"error\",\"message\":\"Missing mode parameter\"}");
        return;
    }

    String modeStr = request->getParam("mode")->value();
    int mode = modeStr.toInt();

    if (mode != 0 && mode != 1) {
        request->send(400, "application/json",
            "{\"status\":\"error\",\"message\":\"Invalid mode (0=AUTO, 1=MANUAL)\"}");
        return;
    }

    SbusRouter* router = SbusRouter::getInstance();
    router->setMode(mode == 0 ? SbusRouter::MODE_AUTO : SbusRouter::MODE_MANUAL);

    JsonDocument doc;
    doc["status"] = "ok";
    doc["mode"] = mode;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);

    log_msg(LOG_INFO, "SBUS mode changed to %s", mode == 0 ? "AUTO" : "MANUAL");
}


// Get current SBUS source status
void handleSbusStatus(AsyncWebServerRequest *request) {

    JsonDocument doc;
    doc["status"] = "ok";

    SbusRouter* router = SbusRouter::getInstance();

    // Router state
    doc["mode"] = router->getMode();  // 0=AUTO, 1=MANUAL
    doc["state"] = router->getState();  // 0=OK, 1=HOLD, 2=FAILSAFE
    doc["activeSource"] = router->getActiveSource();

    // Sources array
    JsonArray sources = doc["sources"].to<JsonArray>();

    // Device1
    if (config.device1.role == D1_SBUS_IN) {
        JsonObject src = sources.add<JsonObject>();
        src["id"] = SBUS_SOURCE_DEVICE1;
        src["name"] = "Device1 (GPIO4)";
        src["configured"] = router->isSourceConfigured(SBUS_SOURCE_DEVICE1);
        src["quality"] = router->getSourceQuality(SBUS_SOURCE_DEVICE1);
        src["priority"] = router->getSourcePriority(SBUS_SOURCE_DEVICE1);
        src["hasData"] = router->getSourceHasData(SBUS_SOURCE_DEVICE1);
        src["valid"] = router->getSourceIsValid(SBUS_SOURCE_DEVICE1);
        src["hasFailsafe"] = router->getSourceHasFailsafe(SBUS_SOURCE_DEVICE1);
    }

    // Device2
    if (config.device2.role == D2_SBUS_IN) {
        JsonObject src = sources.add<JsonObject>();
        src["id"] = SBUS_SOURCE_DEVICE2;
        src["name"] = "Device2 (GPIO8)";
        src["configured"] = router->isSourceConfigured(SBUS_SOURCE_DEVICE2);
        src["quality"] = router->getSourceQuality(SBUS_SOURCE_DEVICE2);
        src["priority"] = router->getSourcePriority(SBUS_SOURCE_DEVICE2);
        src["hasData"] = router->getSourceHasData(SBUS_SOURCE_DEVICE2);
        src["valid"] = router->getSourceIsValid(SBUS_SOURCE_DEVICE2);
        src["hasFailsafe"] = router->getSourceHasFailsafe(SBUS_SOURCE_DEVICE2);
    }

    // UDP
    if (config.device4.role == D4_SBUS_UDP_RX) {
        JsonObject src = sources.add<JsonObject>();
        src["id"] = SBUS_SOURCE_UDP;
        src["name"] = "Device4 (UDP)";
        src["configured"] = router->isSourceConfigured(SBUS_SOURCE_UDP);
        src["quality"] = router->getSourceQuality(SBUS_SOURCE_UDP);
        src["priority"] = router->getSourcePriority(SBUS_SOURCE_UDP);
        src["hasData"] = router->getSourceHasData(SBUS_SOURCE_UDP);
        src["valid"] = router->getSourceIsValid(SBUS_SOURCE_UDP);
        src["hasFailsafe"] = router->getSourceHasFailsafe(SBUS_SOURCE_UDP);
    }

    // Statistics
    doc["framesRouted"] = router->getFramesRouted();
    doc["repeatedFrames"] = router->getRepeatedFrames();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}




