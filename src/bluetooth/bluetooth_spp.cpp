// Bluetooth SPP implementation for MiniKit using ESP-IDF API
// Memory is allocated only when init() is called
#if defined(MINIKIT_BT_ENABLED)

#include "bluetooth_spp.h"
#include "../logging.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"

#include <cstring>
#include "../circular_buffer.h"

// Prevent Arduino from releasing BT memory at startup
// Arduino's initArduino() checks btInUse() - if false, calls esp_bt_controller_mem_release()
// which makes esp_bt_controller_init() fail with ESP_ERR_INVALID_STATE
// See: https://github.com/espressif/arduino-esp32/issues/8176
//
// Returns true ONLY if BT will actually be used (D5 role != NONE and not quick reset)
// This runs BEFORE our code, so we read config from NVS directly
#include <Preferences.h>
extern "C" bool btInUse() {
    Preferences prefs;

    // Check quick reset state
    prefs.begin("quickreset", true);  // read-only
    unsigned long lastUptime = prefs.getULong("uptime", 0);
    int count = prefs.getInt("count", 0);
    prefs.end();

    // Quick reset = temp AP mode = no BT needed
    bool quickResetWillTrigger = (lastUptime > 0 && lastUptime < 3000 && count >= 1);
    if (quickResetWillTrigger) {
        return false;  // Let Arduino release BT memory for WiFi
    }

    // Check D5 role from config (duplicated to Preferences in config_save)
    prefs.begin("btconfig", true);  // read-only
    uint8_t d5role = prefs.getUChar("d5_role", 0);  // D5_NONE = 0
    prefs.end();

    // Only keep BT memory if D5 role is active
    return (d5role != 0);
}

// Global instance
BluetoothSPP* bluetoothSPP = nullptr;

// GAP callback for pairing
static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT:
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                log_msg(LOG_INFO, "BT auth success: %s", param->auth_cmpl.device_name);
            } else {
                log_msg(LOG_WARNING, "BT auth failed: %d", param->auth_cmpl.stat);
            }
            break;

        case ESP_BT_GAP_PIN_REQ_EVT:
            // Legacy PIN request - respond with our fixed PIN
            log_msg(LOG_DEBUG, "BT PIN request");
            if (bluetoothSPP) {
                esp_bt_pin_code_t pin;
                memcpy(pin, bluetoothSPP->pinCode, strlen(bluetoothSPP->pinCode));
                esp_bt_gap_pin_reply(param->pin_req.bda, true, strlen(bluetoothSPP->pinCode), pin);
            }
            break;

        case ESP_BT_GAP_CFM_REQ_EVT:
            // SSP numeric comparison - auto-confirm
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
            break;

        case ESP_BT_GAP_KEY_NOTIF_EVT:
            // SSP passkey notification
            log_msg(LOG_INFO, "BT passkey: %u", param->key_notif.passkey);
            break;

        default:
            break;
    }
}

// SPP callback
static void spp_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    if (!bluetoothSPP) return;

    switch (event) {
        case ESP_SPP_INIT_EVT:
            if (param->init.status == ESP_SPP_SUCCESS) {
                // Set scan mode before starting server (as Arduino BluetoothSerial does)
                esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
                // Start SPP server - SEC_NONE for SSP "Just Works" mode
                esp_spp_start_srv(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_SLAVE, 0, "ESP32_SPP");
            } else {
                log_msg(LOG_ERROR, "SPP init failed: %d", param->init.status);
            }
            break;

        case ESP_SPP_START_EVT:
            if (param->start.status == ESP_SPP_SUCCESS) {
                log_msg(LOG_INFO, "SPP server started");
            } else {
                log_msg(LOG_ERROR, "SPP start failed: %d", param->start.status);
            }
            break;

        case ESP_SPP_SRV_OPEN_EVT:
            log_msg(LOG_INFO, "SPP client connected");
            bluetoothSPP->onSppConnect(param->srv_open.handle);
            break;

        case ESP_SPP_CLOSE_EVT:
            log_msg(LOG_INFO, "SPP connection closed");
            bluetoothSPP->onSppDisconnect();
            break;

        case ESP_SPP_DATA_IND_EVT:
            if (param->data_ind.len > 0 && param->data_ind.data) {
                bluetoothSPP->onSppData(param->data_ind.data, param->data_ind.len);
            }
            break;

        default:
            break;
    }
}

BluetoothSPP::~BluetoothSPP() {
    end();
}

bool BluetoothSPP::init(const char* name, const char* pin) {
    if (initialized) {
        log_msg(LOG_WARNING, "Bluetooth SPP already initialized");
        return true;
    }

    // Save device info
    strncpy(deviceName, name, sizeof(deviceName) - 1);
    deviceName[sizeof(deviceName) - 1] = '\0';
    strncpy(pinCode, pin, sizeof(pinCode) - 1);
    pinCode[sizeof(pinCode) - 1] = '\0';

    log_msg(LOG_INFO, "Initializing Bluetooth SPP: %s", deviceName);

    // Initialize BT controller - THIS IS WHERE MEMORY IS ALLOCATED
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        log_msg(LOG_ERROR, "BT controller init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        log_msg(LOG_ERROR, "BT controller enable failed: %s", esp_err_to_name(ret));
        esp_bt_controller_deinit();
        return false;
    }

    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        log_msg(LOG_ERROR, "Bluedroid init failed: %s", esp_err_to_name(ret));
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        return false;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        log_msg(LOG_ERROR, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        return false;
    }

    // Set device name
    esp_bt_dev_set_device_name(deviceName);

    // Register GAP callback FIRST (before setting security)
    esp_bt_gap_register_callback(gap_callback);

    // Set PIN for legacy pairing fallback (old devices without SSP)
    esp_bt_pin_code_t pinCodeBuf;
    size_t pinLen = strlen(pinCode);
    if (pinLen > ESP_BT_PIN_CODE_LEN) pinLen = ESP_BT_PIN_CODE_LEN;
    memcpy(pinCodeBuf, pinCode, pinLen);
    esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_FIXED, pinLen, pinCodeBuf);

    // SSP "Just Works" - no PIN prompt (modern standard for IoT devices)
    // To require PIN: set CONFIG_BT_SSP_ENABLED=n in sdkconfig.minikit_*.defaults
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;  // NoInputNoOutput = Just Works
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(esp_bt_io_cap_t));

    // NOTE: scan mode is set in SPP_INIT_EVT callback (as Arduino BluetoothSerial does)

    // Register SPP callback and initialize SPP
    ret = esp_spp_register_callback(spp_callback);
    if (ret != ESP_OK) {
        log_msg(LOG_ERROR, "SPP callback register failed: %s", esp_err_to_name(ret));
        end();
        return false;
    }

    // Use enhanced init with L2CAP ERTM enabled (as in official ESP-IDF example)
    esp_spp_cfg_t spp_cfg = {
        .mode = ESP_SPP_MODE_CB,
        .enable_l2cap_ertm = true,
        .tx_buffer_size = 0
    };
    ret = esp_spp_enhanced_init(&spp_cfg);
    if (ret != ESP_OK) {
        log_msg(LOG_ERROR, "SPP init failed: %s", esp_err_to_name(ret));
        end();
        return false;
    }

    initialized = true;
    log_msg(LOG_INFO, "Bluetooth SPP started: %s (PIN: %s)", deviceName, pinCode);
    return true;
}

void BluetoothSPP::end() {
    if (!initialized) return;

    log_msg(LOG_INFO, "Stopping Bluetooth SPP");

    esp_spp_deinit();
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    initialized = false;
    connected = false;
    sppHandle = 0;
    rxHead = 0;
    rxTail = 0;

    log_msg(LOG_INFO, "Bluetooth SPP stopped");
}

int BluetoothSPP::available() {
    if (!initialized) return 0;
    size_t head = rxHead;
    size_t tail = rxTail;
    if (head >= tail) {
        return head - tail;
    }
    return RX_BUFFER_SIZE - tail + head;
}

int BluetoothSPP::read() {
    if (!initialized || rxHead == rxTail) return -1;
    uint8_t byte = rxBuffer[rxTail];
    rxTail = (rxTail + 1) % RX_BUFFER_SIZE;
    return byte;
}

size_t BluetoothSPP::readBytes(uint8_t* buffer, size_t length) {
    if (!initialized || !buffer) return 0;
    size_t count = 0;
    while (count < length && rxHead != rxTail) {
        buffer[count++] = rxBuffer[rxTail];
        rxTail = (rxTail + 1) % RX_BUFFER_SIZE;
    }
    return count;
}

size_t BluetoothSPP::write(uint8_t byte) {
    return write(&byte, 1);
}

size_t BluetoothSPP::write(const uint8_t* buffer, size_t size) {
    if (!initialized || !connected || !buffer || size == 0) return 0;

    esp_err_t ret = esp_spp_write(sppHandle, size, (uint8_t*)buffer);
    if (ret != ESP_OK) {
        log_msg(LOG_WARNING, "SPP write failed: %s", esp_err_to_name(ret));
        return 0;
    }
    return size;
}

void BluetoothSPP::flush() {
    // SPP doesn't have explicit flush
}

bool BluetoothSPP::hasClient() {
    return initialized && connected;
}

bool BluetoothSPP::isConnected() {
    return initialized && connected;
}

void BluetoothSPP::onSppData(const uint8_t* data, uint16_t len) {
    // If external buffer is set, write there (for pipeline integration)
    if (externalInputBuffer) {
        size_t free = externalInputBuffer->freeSpace();
        if (free < len) {
            // FIFO eviction - drop oldest data to make room
            externalInputBuffer->consume(len - free);
        }
        externalInputBuffer->write(data, len);
        return;
    }

    // Fallback: internal ring buffer
    for (uint16_t i = 0; i < len; i++) {
        size_t nextHead = (rxHead + 1) % RX_BUFFER_SIZE;
        if (nextHead != rxTail) {
            rxBuffer[rxHead] = data[i];
            rxHead = nextHead;
        } else {
            // Buffer overflow - drop oldest data
            rxTail = (rxTail + 1) % RX_BUFFER_SIZE;
            rxBuffer[rxHead] = data[i];
            rxHead = nextHead;
        }
    }
}

void BluetoothSPP::onSppConnect(uint32_t handle) {
    sppHandle = handle;
    connected = true;
}

void BluetoothSPP::onSppDisconnect() {
    connected = false;
    sppHandle = 0;
}

#endif // MINIKIT_BT_ENABLED
