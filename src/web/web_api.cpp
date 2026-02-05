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
#if defined(MINIKIT_BT_ENABLED)
#include "../bluetooth/bluetooth_spp.h"
#endif
#if defined(BLE_ENABLED)
#include "../bluetooth/bluetooth_ble.h"
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

// =========================================================================
// JSON Response Helpers
// =========================================================================

static void sendJsonError(AsyncWebServerRequest* req, int code, const char* msg) {
    char buf[160];
    snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"message\":\"%s\"}", msg);
    req->send(code, "application/json", buf);
}

static void sendJsonOk(AsyncWebServerRequest* req, const char* msg = nullptr) {
    if (msg) {
        char buf[160];
        snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"message\":\"%s\"}", msg);
        req->send(200, "application/json", buf);
    } else {
        req->send(200, "application/json", "{\"status\":\"ok\"}");
    }
}

// =========================================================================

// Validate SBUS configuration
bool validateSbusConfig(const Config& cfg) {
    // Check if there are SBUS_IN without any SBUS_OUT
    bool hasSbusIn = (cfg.device1.role == D1_SBUS_IN ||
                      cfg.device2.role == D2_SBUS_IN ||
                      cfg.device3.role == D3_SBUS_IN ||
                      cfg.device4.role == D4_SBUS_UDP_RX);
    bool hasSbusOut = (cfg.device2.role == D2_SBUS_OUT ||
                       cfg.device2.role == D2_USB_SBUS_TEXT ||
                       cfg.device3.role == D3_SBUS_OUT ||
                       cfg.device4.role == D4_SBUS_UDP_TX
#if defined(MINIKIT_BT_ENABLED) || defined(BLE_ENABLED)
                       || cfg.device5_config.role == D5_BT_SBUS_TEXT
#endif
                       );

    if (hasSbusIn && !hasSbusOut) {
        log_msg(LOG_ERROR, "SBUS_IN devices require at least one SBUS_OUT device");
        return false;
    }

    return true;
}

// Populate JSON with static configuration (for /api/config)
static void populateApiConfig(JsonDocument& doc) {
    // System info (static)
    doc["deviceName"] = config.device_name;
    doc["version"] = config.device_version;
    doc["arduinoVersion"] = ESP_ARDUINO_VERSION_STR;
    doc["idfVersion"] = esp_get_idf_version();

    // Board identification
#if defined(BOARD_ESP32_S3_SUPER_MINI)
    doc["boardType"] = "s3supermini";
    doc["usbHostSupported"] = false;
#elif defined(BOARD_XIAO_ESP32_S3)
    doc["boardType"] = "xiao";
    doc["usbHostSupported"] = true;
#elif defined(BOARD_MINIKIT_ESP32)
    #if defined(MINIKIT_BT_ENABLED)
        doc["boardType"] = "minikit_bt";
        doc["btSupported"] = true;
    #elif defined(BLE_ENABLED)
        doc["boardType"] = "minikit_ble";
        doc["bleSupported"] = true;
    #else
        doc["boardType"] = "minikit";
        doc["btSupported"] = false;
    #endif
    doc["usbHostSupported"] = false;
    doc["uart2Available"] = false;
#elif defined(BOARD_ESP32_S3_ZERO)
    #if defined(BLE_ENABLED)
        doc["boardType"] = "s3zero_ble";
        doc["bleSupported"] = true;
    #else
        doc["boardType"] = "s3zero";
        doc["bleSupported"] = false;
    #endif
    doc["usbHostSupported"] = true;
#else
    doc["boardType"] = "s3zero";
    doc["usbHostSupported"] = true;
#endif

    // Feature flags
#ifdef SBUS_MAVLINK_SUPPORT
    doc["sbusMavlinkEnabled"] = true;
#else
    doc["sbusMavlinkEnabled"] = false;
#endif

    // UART configuration
    doc["baudrate"] = config.baudrate;
    doc["databits"] = atoi(word_length_to_string(config.databits));
    doc["parity"] = parity_to_string(config.parity);
    doc["stopbits"] = atoi(stop_bits_to_string(config.stopbits));
    doc["flowcontrol"] = config.flowcontrol;

    // WiFi configuration
    doc["ssid"] = config.ssid;
    doc["password"] = config.password;
    doc["wifiApMode"] = (int)config.wifi_ap_mode;  // 0=Disabled, 1=Temporary, 2=Always On
    doc["wifiMode"] = config.wifi_mode;
    doc["wifiTxPower"] = config.wifi_tx_power;
    doc["wifiApChannel"] = config.wifi_ap_channel;
    doc["mdnsHostname"] = config.mdns_hostname;

    // WiFi networks array
    JsonArray networks = doc["wifiNetworks"].to<JsonArray>();
    for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
        JsonObject net = networks.add<JsonObject>();
        net["ssid"] = config.wifi_networks[i].ssid;
        net["password"] = config.wifi_networks[i].password;
    }

    // USB mode
    doc["usbMode"] = config.usb_mode == USB_MODE_HOST ? "host" : "device";

    // Device roles
    doc["device1Role"] = String(config.device1.role);
    doc["device2Role"] = String(config.device2.role);
    doc["device3Role"] = String(config.device3.role);
    doc["device4Role"] = String(config.device4.role);

    // Device role names (for display)
    doc["device1RoleName"] = getDevice1RoleName(config.device1.role);
    doc["device2RoleName"] = getDevice2RoleName(config.device2.role);
    doc["device3RoleName"] = getDevice3RoleName(config.device3.role);
    doc["device4RoleName"] = getDevice4RoleName(config.device4.role);

#if defined(MINIKIT_BT_ENABLED) || defined(BLE_ENABLED)
    doc["device5Role"] = String(config.device5_config.role);
    doc["device5RoleName"] = getDevice5RoleName(config.device5_config.role);
    doc["btSendRate"] = config.device5_config.btSendRate;
#endif

    // Device 4 configuration
    doc["device4TargetIp"] = config.device4_config.target_ip;
    doc["device4Port"] = config.device4_config.port;
    doc["device4AutoBroadcast"] = config.device4_config.auto_broadcast;
    doc["device4UdpTimeout"] = config.device4_config.udpSourceTimeout;
    doc["device4SendRate"] = config.device4_config.udpSendRate;

    // SBUS output format and rate options
    doc["device2SbusFormat"] = config.device2.sbusOutputFormat;
    doc["device2SbusRate"] = config.device2.sbusRate;
    doc["device3SbusFormat"] = config.device3.sbusOutputFormat;
    doc["device3SbusRate"] = config.device3.sbusRate;
    doc["device4SbusFormat"] = config.device4_config.sbusOutputFormat;

    // Log levels
    doc["logLevelWeb"] = (int)config.log_level_web;
    doc["logLevelUart"] = (int)config.log_level_uart;
    doc["logLevelNetwork"] = (int)config.log_level_network;

    // Protocol optimization
    doc["protocolOptimization"] = config.protocolOptimization;
    doc["udpBatchingEnabled"] = config.udpBatchingEnabled;
    doc["mavlinkRouting"] = config.mavlinkRouting;

    // Log display count
    doc["logDisplayCount"] = LOG_DISPLAY_COUNT;
}

// Populate JSON with runtime status (for /api/status)
static void populateApiStatus(JsonDocument& doc) {
    // Runtime system info
    doc["uptime"] = (millis() - g_deviceStats.systemStartTime.load(std::memory_order_relaxed)) / MS_TO_SECONDS;
    doc["freeRam"] = ESP.getFreeHeap();

    // WiFi client status
    if (config.wifi_mode == BRIDGE_WIFI_MODE_CLIENT) {
        doc["wifiClientConnected"] = systemState.wifiClientConnected;
        if (systemState.wifiClientConnected) {
            doc["connectedSSID"] = wifiGetConnectedSSID();
            doc["ipAddress"] = wifiGetIP();
            doc["rssiPercent"] = rssiToPercent(wifiGetRSSI());
        }
    }
    doc["tempNetworkMode"] = systemState.isTemporaryNetwork;

#if defined(MINIKIT_BT_ENABLED)
    // BT SPP runtime status
    doc["btInitialized"] = (bluetoothSPP != nullptr);
    doc["btConnected"] = (bluetoothSPP != nullptr && bluetoothSPP->isConnected());
#endif
#if defined(BLE_ENABLED)
    // BLE runtime status
    doc["btInitialized"] = (bluetoothBLE != nullptr);
    doc["btConnected"] = (bluetoothBLE != nullptr && bluetoothBLE->isConnected());
#endif

    // UART configuration string
    char uartConfigBuf[32];
    snprintf(uartConfigBuf, sizeof(uartConfigBuf), "%d baud, %s%c%s",
             config.baudrate,
             word_length_to_string(config.databits),
             parity_to_string(config.parity)[0],
             stop_bits_to_string(config.stopbits));
    doc["uartConfig"] = uartConfigBuf;

    // Flow control status
    if (uartBridgeSerial) {
        doc["flowControl"] = uartBridgeSerial->getFlowControlStatus();
    } else {
        doc["flowControl"] = "Not initialized";
    }

    // Device roles (for conditional display in UI)
    doc["device2Role"] = String(config.device2.role);
    doc["device3Role"] = String(config.device3.role);
    doc["device4Role"] = String(config.device4.role);
    doc["device5Role"] = String(config.device5_config.role);

    // Device role names (for labels in Device Statistics)
    doc["device1RoleName"] = getDevice1RoleName(config.device1.role);
    doc["device2RoleName"] = getDevice2RoleName(config.device2.role);
    doc["device3RoleName"] = getDevice3RoleName(config.device3.role);
    doc["device4RoleName"] = getDevice4RoleName(config.device4.role);
    doc["device5RoleName"] = getDevice5RoleName(config.device5_config.role);

    // Device statistics
    doc["device1Rx"] = g_deviceStats.device1.rxBytes.load(std::memory_order_relaxed);
    doc["device1Tx"] = g_deviceStats.device1.txBytes.load(std::memory_order_relaxed);
    doc["device2Rx"] = g_deviceStats.device2.rxBytes.load(std::memory_order_relaxed);
    doc["device2Tx"] = g_deviceStats.device2.txBytes.load(std::memory_order_relaxed);
    doc["device3Rx"] = g_deviceStats.device3.rxBytes.load(std::memory_order_relaxed);
    doc["device3Tx"] = g_deviceStats.device3.txBytes.load(std::memory_order_relaxed);

    // Device4 statistics
    if (config.device4.role != D4_NONE && systemState.networkActive) {
        doc["device4TxBytes"] = g_deviceStats.device4.txBytes.load(std::memory_order_relaxed);
        doc["device4TxPackets"] = g_deviceStats.device4.txPackets.load(std::memory_order_relaxed);
        doc["device4RxBytes"] = g_deviceStats.device4.rxBytes.load(std::memory_order_relaxed);
        doc["device4RxPackets"] = g_deviceStats.device4.rxPackets.load(std::memory_order_relaxed);
    }

    // Device5 statistics (Bluetooth)
    if (config.device5_config.role != D5_NONE) {
        doc["device5TxBytes"] = g_deviceStats.device5.txBytes.load(std::memory_order_relaxed);
        doc["device5RxBytes"] = g_deviceStats.device5.rxBytes.load(std::memory_order_relaxed);
    }

    // Total traffic
    unsigned long totalTraffic =
        g_deviceStats.device1.rxBytes.load(std::memory_order_relaxed) +
        g_deviceStats.device1.txBytes.load(std::memory_order_relaxed) +
        g_deviceStats.device2.rxBytes.load(std::memory_order_relaxed) +
        g_deviceStats.device2.txBytes.load(std::memory_order_relaxed) +
        g_deviceStats.device3.rxBytes.load(std::memory_order_relaxed) +
        g_deviceStats.device3.txBytes.load(std::memory_order_relaxed);
    doc["totalTraffic"] = totalTraffic;

    // Last activity
    unsigned long lastActivity = g_deviceStats.lastGlobalActivity.load(std::memory_order_relaxed);
    if (lastActivity == 0) {
        doc["lastActivity"] = "Never";
    } else {
        char activityBuf[32];
        snprintf(activityBuf, sizeof(activityBuf), "%lu seconds ago",
                 (millis() - lastActivity) / MS_TO_SECONDS);
        doc["lastActivity"] = activityBuf;
    }

    // UDP batching setting
    doc["udpBatchingEnabled"] = config.udpBatchingEnabled;

    // Protocol statistics
    BridgeContext* ctx = getBridgeContext();
    if (ctx && ctx->protocolPipeline) {
        ctx->protocolPipeline->appendStatsToJson(doc);
    }
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

// Handle /api/config - static configuration (loaded once on page load)
void handleApiConfig(AsyncWebServerRequest *request) {
    JsonDocument doc;
    populateApiConfig(doc);

    auto* res = request->beginResponseStream("application/json");
    res->addHeader("Connection", "close");
    serializeJson(doc, *res);
    request->send(res);
}

// Handle /api/status - runtime status (polled periodically)
void handleApiStatus(AsyncWebServerRequest *request) {
    // Reset WiFi auto-disable timer on web activity (Client mode)
    if (config.wifi_mode == BRIDGE_WIFI_MODE_CLIENT) {
        resetWiFiTimeout();
    }

    JsonDocument doc;
    populateApiStatus(doc);

    auto* res = request->beginResponseStream("application/json");
    res->addHeader("Connection", "close");
    serializeJson(doc, *res);
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

// Handle save configuration (JSON body)
void handleSaveJson(AsyncWebServerRequest *request) {
    log_msg(LOG_INFO, "Saving new configuration (JSON)...");

    // Get JSON body from _tempObject
    if (!request->_tempObject) {
        sendJsonError(request, 400, "No JSON body received");
        return;
    }

    const char* jsonBody = static_cast<const char*>(request->_tempObject);

    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonBody);

    // Free the buffer
    free(request->_tempObject);
    request->_tempObject = nullptr;

    if (error) {
        log_msg(LOG_ERROR, "JSON parse error: %s", error.c_str());
        sendJsonError(request, 400, "Invalid JSON");
        return;
    }

    bool configChanged = false;

    // UART settings
    if (doc.containsKey("baudrate") && doc["baudrate"] != config.baudrate) {
        config.baudrate = doc["baudrate"];
        configChanged = true;
        log_msg(LOG_INFO, "UART baudrate: %u", config.baudrate);
    }

    if (doc.containsKey("databits")) {
        uart_word_length_t newWordLength = string_to_word_length(doc["databits"]);
        if (newWordLength != config.databits) {
            config.databits = newWordLength;
            configChanged = true;
            log_msg(LOG_INFO, "UART data bits: %d", (int)doc["databits"]);
        }
    }

    if (doc.containsKey("parity")) {
        const char* parityStr = doc["parity"];
        uart_parity_t newParity = string_to_parity(parityStr);
        if (newParity != config.parity) {
            config.parity = newParity;
            configChanged = true;
            log_msg(LOG_INFO, "UART parity: %s", parityStr);
        }
    }

    if (doc.containsKey("stopbits")) {
        uart_stop_bits_t newStopBits = string_to_stop_bits(doc["stopbits"]);
        if (newStopBits != config.stopbits) {
            config.stopbits = newStopBits;
            configChanged = true;
            log_msg(LOG_INFO, "UART stop bits: %d", (int)doc["stopbits"]);
        }
    }

    if (doc.containsKey("flowcontrol")) {
        bool newFlowcontrol = doc["flowcontrol"];
        if (newFlowcontrol != config.flowcontrol) {
            config.flowcontrol = newFlowcontrol;
            configChanged = true;
            log_msg(LOG_INFO, "Flow control %s", newFlowcontrol ? "enabled" : "disabled");
        }
    }

    // USB mode
    if (doc.containsKey("usbmode")) {
        String mode = doc["usbmode"].as<String>();
        UsbMode newMode = USB_MODE_DEVICE;

        #if defined(BOARD_ESP32_S3_SUPER_MINI) || defined(BOARD_MINIKIT_ESP32)
        if (mode == "host") {
            log_msg(LOG_WARNING, "USB Host mode not supported on this board");
            newMode = USB_MODE_DEVICE;
        }
        #else
        if (mode == "host") {
            newMode = USB_MODE_HOST;
        }
        #endif

        if (newMode != config.usb_mode) {
            config.usb_mode = newMode;
            configChanged = true;
            log_msg(LOG_INFO, "USB mode: %s", newMode == USB_MODE_HOST ? "host" : "device");
        }
    }

    // Device roles
    if (doc.containsKey("device1_role")) {
        int role = doc["device1_role"];
        if (role >= D1_UART1 && role <= D1_SBUS_IN && role != config.device1.role) {
            config.device1.role = role;
            configChanged = true;
            log_msg(LOG_INFO, "Device 1 role: %d", role);
        }
    }

    if (doc.containsKey("device2_role")) {
        int role = doc["device2_role"];
        if (role >= D2_NONE && role <= D2_USB_SBUS_TEXT && role != config.device2.role) {
            config.device2.role = role;
            configChanged = true;
            log_msg(LOG_INFO, "Device 2 role: %d", role);
        }
    }

    if (doc.containsKey("device2_sbus_rate")) {
        int rate = doc["device2_sbus_rate"];
        if (rate >= 10 && rate <= 70 && rate != config.device2.sbusRate) {
            config.device2.sbusRate = rate;
            configChanged = true;
            log_msg(LOG_INFO, "Device 2 SBUS rate: %d Hz", rate);
        }
    }

    if (doc.containsKey("device2_sbus_format")) {
        uint8_t fmt = doc["device2_sbus_format"];
        if (fmt <= 2 && fmt != config.device2.sbusOutputFormat) {
            config.device2.sbusOutputFormat = fmt;
            configChanged = true;
            log_msg(LOG_INFO, "Device 2 SBUS format: %d", fmt);
        }
    }

    if (doc.containsKey("device3_role")) {
        int role = doc["device3_role"];
        if (role >= D3_NONE && role <= D3_SBUS_OUT && role != config.device3.role) {
            config.device3.role = role;
            configChanged = true;
            log_msg(LOG_INFO, "Device 3 role: %d", role);
        }
    }

    if (doc.containsKey("device3_sbus_format")) {
        uint8_t fmt = doc["device3_sbus_format"];
        if (fmt <= 2 && fmt != config.device3.sbusOutputFormat) {
            config.device3.sbusOutputFormat = fmt;
            configChanged = true;
            log_msg(LOG_INFO, "Device 3 SBUS format: %d", fmt);
        }
    }

    if (doc.containsKey("device3_sbus_rate")) {
        int rate = doc["device3_sbus_rate"];
        if (rate >= 10 && rate <= 70 && rate != config.device3.sbusRate) {
            config.device3.sbusRate = rate;
            configChanged = true;
            log_msg(LOG_INFO, "Device 3 SBUS rate: %d Hz", rate);
        }
    }

    if (doc.containsKey("device4_role")) {
        int role = doc["device4_role"];
        if (role >= D4_NONE && role <= D4_SBUS_UDP_RX && role != config.device4.role) {
            config.device4.role = role;
            configChanged = true;
            log_msg(LOG_INFO, "Device 4 role: %d", role);
        }
    }

    if (doc.containsKey("device4_sbus_format")) {
        uint8_t fmt = doc["device4_sbus_format"];
        if (fmt <= 2 && fmt != config.device4_config.sbusOutputFormat) {
            config.device4_config.sbusOutputFormat = fmt;
            configChanged = true;
            log_msg(LOG_INFO, "Device 4 SBUS format: %d", fmt);
        }
    }

    // Device 4 network config
    if (doc.containsKey("device4_target_ip")) {
        const char* ip = doc["device4_target_ip"];
        if (strcmp(ip, config.device4_config.target_ip) != 0) {
            strncpy(config.device4_config.target_ip, ip, IP_ADDRESS_BUFFER_SIZE);
            config.device4_config.target_ip[IP_ADDRESS_BUFFER_SIZE] = '\0';
            configChanged = true;
            log_msg(LOG_INFO, "Device 4 target IP: %s", ip);
        }
    }

    if (doc.containsKey("device4_port")) {
        uint16_t port = doc["device4_port"];
        if (port != config.device4_config.port) {
            config.device4_config.port = port;
            configChanged = true;
            log_msg(LOG_INFO, "Device 4 port: %u", port);
        }
    }

    if (doc.containsKey("device4_auto_broadcast")) {
        bool newVal = doc["device4_auto_broadcast"];
        if (newVal != config.device4_config.auto_broadcast) {
            config.device4_config.auto_broadcast = newVal;
            configChanged = true;
            log_msg(LOG_INFO, "Device 4 auto broadcast: %s", newVal ? "enabled" : "disabled");
        }
    }

    if (doc.containsKey("device4_udp_timeout")) {
        uint16_t timeout = doc["device4_udp_timeout"];
        if (timeout >= 100 && timeout <= 5000 && timeout != config.device4_config.udpSourceTimeout) {
            config.device4_config.udpSourceTimeout = timeout;
            configChanged = true;
            log_msg(LOG_INFO, "Device 4 UDP timeout: %u ms", timeout);
        }
    }

    if (doc.containsKey("device4_send_rate")) {
        uint8_t rate = doc["device4_send_rate"];
        if (rate >= 10 && rate <= 70 && rate != config.device4_config.udpSendRate) {
            config.device4_config.udpSendRate = rate;
            configChanged = true;
            log_msg(LOG_INFO, "Device 4 send rate: %u Hz", rate);
        }
    }

    // Sync device4 role
    config.device4_config.role = config.device4.role;

#if defined(MINIKIT_BT_ENABLED) || defined(BLE_ENABLED)
    if (doc.containsKey("device5_role")) {
        int role = doc["device5_role"];
        if (role >= D5_NONE && role <= D5_BT_SBUS_TEXT && role != config.device5_config.role) {
            config.device5_config.role = role;
            configChanged = true;
            log_msg(LOG_INFO, "Device 5 role: %d", role);
        }
    }

    if (doc.containsKey("bt_send_rate")) {
        int rate = doc["bt_send_rate"];
        if (rate >= 10 && rate <= 70 && rate != config.device5_config.btSendRate) {
            config.device5_config.btSendRate = rate;
            configChanged = true;
            log_msg(LOG_INFO, "BT send rate: %d Hz", rate);
        }
    }
#endif

    // Validate SBUS configuration
    if (!validateSbusConfig(config)) {
        sendJsonError(request, 400, "Invalid SBUS configuration. Check device roles.");
        return;
    }

    // Log levels
    if (doc.containsKey("log_level_web")) {
        int level = doc["log_level_web"];
        if (level >= LOG_OFF && level <= LOG_DEBUG && level != config.log_level_web) {
            config.log_level_web = (LogLevel)level;
            configChanged = true;
            log_msg(LOG_INFO, "Web log level: %s", getLogLevelName((LogLevel)level));
        }
    }

    if (doc.containsKey("log_level_uart")) {
        int level = doc["log_level_uart"];
        if (level >= LOG_OFF && level <= LOG_DEBUG && level != config.log_level_uart) {
            config.log_level_uart = (LogLevel)level;
            configChanged = true;
            log_msg(LOG_INFO, "UART log level: %s", getLogLevelName((LogLevel)level));
        }
    }

    if (doc.containsKey("log_level_network")) {
        int level = doc["log_level_network"];
        if (level >= LOG_OFF && level <= LOG_DEBUG && level != config.log_level_network) {
            config.log_level_network = (LogLevel)level;
            configChanged = true;
            log_msg(LOG_INFO, "Network log level: %s", getLogLevelName((LogLevel)level));
        }
    }

    // Protocol optimization
    if (doc.containsKey("protocol_optimization")) {
        uint8_t newProtocol = doc["protocol_optimization"];
        if (newProtocol != config.protocolOptimization) {
            config.protocolOptimization = newProtocol;
            configChanged = true;
            log_msg(LOG_INFO, "Protocol optimization: %d", newProtocol);

            BridgeContext* ctx = getBridgeContext();
            if (ctx && ctx->protocolPipeline) {
                delete ctx->protocolPipeline;
                ctx->protocolPipeline = new ProtocolPipeline(ctx);
                ctx->protocolPipeline->init(&config);
            }
        }
    }

    if (doc.containsKey("udp_batching")) {
        bool newVal = doc["udp_batching"];
        if (newVal != config.udpBatchingEnabled) {
            config.udpBatchingEnabled = newVal;
            configChanged = true;
            log_msg(LOG_INFO, "UDP batching: %s", newVal ? "enabled" : "disabled");
        }
    }

    if (doc.containsKey("mavlink_routing")) {
        bool newVal = doc["mavlink_routing"];
        if (newVal != config.mavlinkRouting) {
            config.mavlinkRouting = newVal;
            configChanged = true;
            log_msg(LOG_INFO, "MAVLink routing: %s", newVal ? "enabled" : "disabled");
        }
    }

    // WiFi settings
    if (doc.containsKey("ssid")) {
        String newSSID = doc["ssid"].as<String>();
        if (newSSID.length() > 0 && newSSID != config.ssid) {
            config.ssid = newSSID;
            configChanged = true;
            log_msg(LOG_INFO, "WiFi SSID: %s", newSSID.c_str());
        }
    }

    if (doc.containsKey("password")) {
        String newPassword = doc["password"].as<String>();
        if (newPassword.length() >= 8 && newPassword != config.password) {
            config.password = newPassword;
            configChanged = true;
            log_msg(LOG_INFO, "WiFi password updated");
        }
    }

    if (doc.containsKey("wifi_ap_mode")) {
        int newVal = doc["wifi_ap_mode"];
        if (newVal >= WIFI_AP_DISABLED && newVal <= WIFI_AP_ALWAYS_ON && newVal != config.wifi_ap_mode) {
            config.wifi_ap_mode = (WifiApMode)newVal;
            configChanged = true;
            const char* modeNames[] = {"Disabled", "Temporary", "Always On"};
            log_msg(LOG_INFO, "WiFi AP mode: %s", modeNames[newVal]);
        }
    }

    if (doc.containsKey("wifi_mode")) {
        int mode = doc["wifi_mode"];
        if (mode >= BRIDGE_WIFI_MODE_AP && mode <= BRIDGE_WIFI_MODE_CLIENT && mode != config.wifi_mode) {
            config.wifi_mode = (BridgeWiFiMode)mode;
            configChanged = true;
            log_msg(LOG_INFO, "WiFi mode: %s", mode == BRIDGE_WIFI_MODE_AP ? "AP" : "Client");
        }
    }

    // WiFi networks array
    if (doc.containsKey("wifi_networks")) {
        JsonArray networks = doc["wifi_networks"];
        bool wifiNetworksChanged = false;

        for (int i = 0; i < MAX_WIFI_NETWORKS && i < (int)networks.size(); i++) {
            JsonObject net = networks[i];

            if (net.containsKey("ssid")) {
                String newSSID = net["ssid"].as<String>();
                newSSID.trim();

                // Validate primary network in Client mode
                if (i == 0 && config.wifi_mode == BRIDGE_WIFI_MODE_CLIENT && newSSID.isEmpty()) {
                    sendJsonError(request, 400, "Primary network SSID cannot be empty");
                    return;
                }

                if (newSSID != config.wifi_networks[i].ssid) {
                    config.wifi_networks[i].ssid = newSSID;
                    wifiNetworksChanged = true;
                }
            }

            if (net.containsKey("password")) {
                String newPassword = net["password"].as<String>();
                if (newPassword.length() > 0 && newPassword.length() < 8) {
                    sendJsonError(request, 400, "WiFi password must be at least 8 characters or empty");
                    return;
                }
                if (newPassword != config.wifi_networks[i].password) {
                    config.wifi_networks[i].password = newPassword;
                    wifiNetworksChanged = true;
                }
            }
        }

        if (wifiNetworksChanged) {
            configChanged = true;
            wifiResetAuthFlags();
            log_msg(LOG_INFO, "WiFi networks updated");
        }
    }

    // WiFi TX Power
    if (doc.containsKey("wifi_tx_power")) {
        uint8_t newTxPower = doc["wifi_tx_power"];
        if (newTxPower < 8 || newTxPower > 80) {
            sendJsonError(request, 400, "WiFi TX Power must be between 8 and 80");
            return;
        }
        if (newTxPower != config.wifi_tx_power) {
            config.wifi_tx_power = newTxPower;
            configChanged = true;
            log_msg(LOG_INFO, "WiFi TX Power: %d", newTxPower);
        }
    }

    // WiFi AP Channel
    if (doc.containsKey("wifi_ap_channel")) {
        uint8_t newChannel = doc["wifi_ap_channel"];
        if (newChannel < 1 || newChannel > 13) {
            sendJsonError(request, 400, "WiFi AP Channel must be between 1 and 13");
            return;
        }
        if (newChannel != config.wifi_ap_channel) {
            config.wifi_ap_channel = newChannel;
            configChanged = true;
            log_msg(LOG_INFO, "WiFi AP Channel: %d", newChannel);
        }
    }

    // mDNS Hostname
    if (doc.containsKey("mdns_hostname")) {
        String newHostname = doc["mdns_hostname"].as<String>();
        newHostname.toLowerCase();
        newHostname.trim();

        // Validate hostname
        bool valid = newHostname.length() <= 63;
        if (valid && newHostname.length() > 0) {
            if (newHostname.charAt(0) == '-' || newHostname.charAt(newHostname.length() - 1) == '-') {
                valid = false;
            }
            for (size_t i = 0; i < newHostname.length() && valid; i++) {
                char c = newHostname.charAt(i);
                if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
                    valid = false;
                }
            }
        }

        if (!valid) {
            sendJsonError(request, 400, "Invalid hostname: use a-z, 0-9, - only, max 63 chars");
            return;
        }

        if (newHostname != config.mdns_hostname) {
            config.mdns_hostname = newHostname;
            configChanged = true;
            log_msg(LOG_INFO, "mDNS hostname: %s", newHostname.c_str());
        }
    }

    if (configChanged) {
        cancelWiFiTimeout();
        config_save(&config);

        sendJsonOk(request, "Configuration saved successfully! Device restarting...");
        scheduleReboot(3000);
    } else {
        request->send(200, "application/json",
            "{\"status\":\"unchanged\",\"message\":\"Configuration was not modified\"}");
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
    sendJsonOk(request, "Statistics and logs cleared");
}

// Handle crash log JSON request
void handleCrashLogJson(AsyncWebServerRequest *request) {
    String json = crashlog_get_json();
    request->send(200, "application/json", json);
}

// Handle clear crash log request
void handleClearCrashLog(AsyncWebServerRequest *request) {
    crashlog_clear();
    sendJsonOk(request);
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
    sendJsonOk(request, "Factory reset complete");

    // Schedule reboot
    delay(500);
    ESP.restart();
}

// Import configuration from uploaded JSON file
void handleImportConfig(AsyncWebServerRequest *request) {
    // Check if file data was uploaded
    if (!request->_tempObject) {
        log_msg(LOG_ERROR, "Import failed: No file uploaded");
        sendJsonError(request, 400, "No file uploaded");
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
        sendJsonError(request, 400, "Invalid configuration file");
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

    sendJsonOk(request, "Configuration imported successfully! Device restarting...");
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
        sendJsonError(request, 400, "Missing source parameter");
        return;
    }

    String sourceStr = request->getParam("source")->value();
    uint8_t source = sourceStr.toInt();

    if (source > 2) {
        sendJsonError(request, 400, "Invalid source");
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
        sendJsonError(request, 400, "Missing mode parameter");
        return;
    }

    String modeStr = request->getParam("mode")->value();
    int mode = modeStr.toInt();

    if (mode != 0 && mode != 1) {
        sendJsonError(request, 400, "Invalid mode (0=AUTO, 1=MANUAL)");
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


// Helper to add SBUS source info to JSON array
static void addSbusSourceToJson(JsonArray& sources, SbusRouter* router,
                                 uint8_t sourceId, const char* name) {
    JsonObject src = sources.add<JsonObject>();
    src["id"] = sourceId;
    src["name"] = name;
    src["configured"] = router->isSourceConfigured(sourceId);
    src["quality"] = router->getSourceQuality(sourceId);
    src["priority"] = router->getSourcePriority(sourceId);
    src["hasData"] = router->getSourceHasData(sourceId);
    src["valid"] = router->getSourceIsValid(sourceId);
    src["hasFailsafe"] = router->getSourceHasFailsafe(sourceId);
    src["framesReceived"] = router->getSourceFramesReceived(sourceId);
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

    if (config.device1.role == D1_SBUS_IN)
        addSbusSourceToJson(sources, router, SBUS_SOURCE_DEVICE1, "Device1 (GPIO4)");
    if (config.device2.role == D2_SBUS_IN)
        addSbusSourceToJson(sources, router, SBUS_SOURCE_DEVICE2, "Device2 (GPIO8)");
    if (config.device3.role == D3_SBUS_IN)
        addSbusSourceToJson(sources, router, SBUS_SOURCE_DEVICE3, "Device3 (GPIO6)");
    if (config.device4.role == D4_SBUS_UDP_RX)
        addSbusSourceToJson(sources, router, SBUS_SOURCE_UDP, "Device4 (UDP)");

    // Statistics
    doc["framesRouted"] = router->getFramesRouted();
    doc["repeatedFrames"] = router->getRepeatedFrames();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}




