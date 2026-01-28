#include "bluetooth_ble.h"

#if defined(BLE_ENABLED)

#include "../logging.h"
#include "../diagnostics.h"
#include "../circular_buffer.h"
#include "nvs_flash.h"

// ESP32 (WROOM) needs explicit BT controller init
// Arduino framework releases BT memory at startup if btInUse() returns false
#if defined(CONFIG_IDF_TARGET_ESP32)
#include "esp_bt.h"

// Override Arduino's weak btInUse() to prevent BT memory release in initArduino()
// This tells Arduino that BT is in use, so it won't call esp_bt_controller_mem_release()
extern "C" bool btInUse() { return true; }
#endif

// ESP-IDF NimBLE includes
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

// GATT service definitions (in C file for NimBLE compatibility)
#include "bluetooth_ble_gatt.h"

// Global instance
BluetoothBLE* bluetoothBLE = nullptr;

// Forward declarations for NimBLE callbacks
static int ble_gap_event_handler(struct ble_gap_event* event, void* arg);
static void ble_on_sync(void);
static void ble_on_reset(int reason);
static void ble_host_task(void* param);

// C callback wrapper for RX data
static void ble_rx_callback_wrapper(const uint8_t* data, size_t len) {
    if (bluetoothBLE) {
        bluetoothBLE->onRxData(data, len);
    }
}

// GAP event handler
static int ble_gap_event_handler(struct ble_gap_event* event, void* arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                // Connected
                if (bluetoothBLE) {
                    bluetoothBLE->onConnect(event->connect.conn_handle);
                }
            } else {
                // Connection failed, restart advertising
                ble_on_sync();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            if (bluetoothBLE) {
                bluetoothBLE->onDisconnect(event->disconnect.conn.conn_handle,
                                           event->disconnect.reason);
            }
            // Restart advertising
            ble_on_sync();
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            // Advertising finished, restart if not connected
            if (bluetoothBLE && !bluetoothBLE->isConnected()) {
                ble_on_sync();
            }
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            // log_msg in NimBLE callbacks disabled for WiFi+BLE stability test
            // log_msg(LOG_DEBUG, "BLE: Subscribe event, cur_notify=%d", event->subscribe.cur_notify);
            break;

        case BLE_GAP_EVENT_MTU:
            // log_msg(LOG_DEBUG, "BLE: MTU updated to %d", event->mtu.value);
            break;

        default:
            break;
    }
    return 0;
}

// Start advertising
static void ble_advertise(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    // forceSerialLog("BLE: ble_advertise() called");

    memset(&fields, 0, sizeof(fields));

    // Flags: general discoverable, BR/EDR not supported
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // Include TX power level
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    // Include device name
    const char* name = ble_svc_gap_device_name();
    fields.name = (uint8_t*)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    // forceSerialLog("BLE: adv_set_fields rc=%d", rc);
    if (rc != 0) {
        return;
    }

    // Advertising parameters
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;  // Undirected connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;  // General discoverable
    adv_params.itvl_min = 0x20;  // 20ms
    adv_params.itvl_max = 0x40;  // 40ms

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_gap_event_handler, NULL);
    // forceSerialLog("BLE: adv_start rc=%d", rc);
}

// Called when BLE host syncs with controller
static void ble_on_sync(void) {
    // forceSerialLog("BLE: ble_on_sync() called");

    // Infer best address type to use
    uint8_t own_addr_type;
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    // forceSerialLog("BLE: infer_auto rc=%d", rc);
    if (rc != 0) {
        return;
    }

    // Start advertising
    ble_advertise();
}

// Called on BLE host reset
static void ble_on_reset(int reason) {
    // forceSerialLog("BLE: on_reset reason=%d", reason);
}

// NimBLE host task
static void ble_host_task(void* param) {
    // forceSerialLog("BLE: host_task started");
    (void)param;
    nimble_port_run();  // This returns only when nimble_port_stop() is called
    nimble_port_freertos_deinit();
}

// ============================================================================
// BluetoothBLE class implementation
// ============================================================================

BluetoothBLE::BluetoothBLE() {
    memset(deviceName, 0, sizeof(deviceName));
}

BluetoothBLE::~BluetoothBLE() {
    deinit();
}

bool BluetoothBLE::init(const char* name) {
    if (initialized) {
        log_msg(LOG_WARNING, "BLE: Already initialized");
        return true;
    }

    strncpy(deviceName, name, sizeof(deviceName) - 1);
    log_msg(LOG_INFO, "BLE: Initializing, name=%s", deviceName);

    // Ensure NVS is initialized (required for NimBLE bonding storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        log_msg(LOG_ERROR, "BLE: NVS init failed, err=%d", ret);
        return false;
    }
    // forceSerialLog("BLE: NVS ok, target=%s", CONFIG_IDF_TARGET);

    // ESP32 (WROOM): btInUse() override above prevents Arduino from releasing BT memory
    // nimble_port_init() will handle controller init internally

    // Initialize NimBLE (handles controller init internally)
    ret = nimble_port_init();
    // forceSerialLog("BLE: nimble_port_init ret=%d", ret);
    if (ret != ESP_OK) {
        log_msg(LOG_ERROR, "BLE: nimble_port_init failed, err=%d", ret);
        return false;
    }

    // Configure host callbacks
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;

    // Initialize GAP and GATT services
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Set device name
    ble_svc_gap_device_name_set(deviceName);

    // Set RX callback before registering services
    ble_gatt_set_rx_callback(ble_rx_callback_wrapper);

    // Register custom GATT services (defined in bluetooth_ble_gatt.c)
    int rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        log_msg(LOG_ERROR, "BLE: ble_gatts_count_cfg failed, rc=%d", rc);
        return false;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        log_msg(LOG_ERROR, "BLE: ble_gatts_add_svcs failed, rc=%d", rc);
        return false;
    }

    // Start NimBLE host task
    nimble_port_freertos_init(ble_host_task);
    // forceSerialLog("BLE: nimble_port_freertos_init done");

    // Store TX handle for notifications (from C file)
    txAttrHandle = g_nus_tx_attr_handle;

    initialized = true;
    // forceSerialLog("BLE: init() completed");

    return true;
}

void BluetoothBLE::deinit() {
    if (!initialized) return;

    log_msg(LOG_INFO, "BLE: Shutting down");

    // Stop advertising
    ble_gap_adv_stop();

    // Stop NimBLE host
    nimble_port_stop();

    // Deinit NimBLE
    nimble_port_deinit();

    initialized = false;
    connected = false;
    connHandle = 0;
}

void BluetoothBLE::onConnect(uint16_t handle) {
    connHandle = handle;
    connected = true;
    // log_msg in NimBLE callbacks disabled for WiFi+BLE stability test
    // log_msg(LOG_INFO, "BLE: Client connected (handle=%d)", handle);
}

void BluetoothBLE::onDisconnect(uint16_t handle, int reason) {
    (void)handle;
    (void)reason;
    connected = false;
    connHandle = 0;
    // log_msg in NimBLE callbacks disabled for WiFi+BLE stability test
    // log_msg(LOG_INFO, "BLE: Client disconnected (reason=%d)", reason);
}

void BluetoothBLE::onRxData(const uint8_t* data, size_t len) {
    // TODO: remove after full BLE bridge implementation
    // // Ping/pong test
    // if (len == 4 && memcmp(data, "ping", 4) == 0) {
    //     write((const uint8_t*)"pong", 4);
    //     return;
    // }

    rxBytes += len;

    // Use external buffer if set (pipeline integration)
    if (externalInputBuffer) {
        size_t free = externalInputBuffer->freeSpace();
        if (free < len) {
            // FIFO eviction - discard oldest data to make room
            externalInputBuffer->consume(len - free);
        }
        externalInputBuffer->write(data, len);
        return;
    }

    // Fallback: internal circular buffer
    for (size_t i = 0; i < len; i++) {
        size_t nextHead = (rxHead + 1) % BLE_RX_BUFFER_SIZE;
        if (nextHead != rxTail) {  // Not full
            rxBuffer[rxHead] = data[i];
            rxHead = nextHead;
        } else {
            // log_msg in NimBLE callbacks disabled for WiFi+BLE stability test
            // log_msg(LOG_WARNING, "BLE: RX buffer full, dropped %zu bytes", len - i);
            break;
        }
    }
}

size_t BluetoothBLE::write(const uint8_t* data, size_t len) {
    if (!initialized || !connected || txAttrHandle == 0) return 0;

    size_t totalSent = 0;

    // Send in chunks respecting MTU
    while (totalSent < len) {
        size_t chunkSize = min(len - totalSent, (size_t)BLE_TX_MTU_SIZE);

        struct os_mbuf* om = ble_hs_mbuf_from_flat(data + totalSent, chunkSize);
        if (om == NULL) {
            // log_msg in NimBLE callbacks disabled for WiFi+BLE stability test
            // log_msg(LOG_ERROR, "BLE: Failed to allocate mbuf");
            break;
        }

        int rc = ble_gatts_notify_custom(connHandle, txAttrHandle, om);
        if (rc != 0) {
            // log_msg(LOG_ERROR, "BLE: Notify failed, rc=%d", rc);
            break;
        }

        totalSent += chunkSize;
        txBytes += chunkSize;
    }

    return totalSent;
}

size_t BluetoothBLE::write(const char* str) {
    return write((const uint8_t*)str, strlen(str));
}

void BluetoothBLE::flush() {
    // Notifications are sent immediately, nothing to flush
}

size_t BluetoothBLE::available() {
    if (rxHead >= rxTail) {
        return rxHead - rxTail;
    } else {
        return BLE_RX_BUFFER_SIZE - rxTail + rxHead;
    }
}

size_t BluetoothBLE::read(uint8_t* buffer, size_t maxLen) {
    size_t count = 0;
    while (count < maxLen && rxTail != rxHead) {
        buffer[count++] = rxBuffer[rxTail];
        rxTail = (rxTail + 1) % BLE_RX_BUFFER_SIZE;
    }
    return count;
}

int BluetoothBLE::read() {
    if (rxTail == rxHead) return -1;  // Empty

    uint8_t byte = rxBuffer[rxTail];
    rxTail = (rxTail + 1) % BLE_RX_BUFFER_SIZE;
    return byte;
}

#endif // BLE_ENABLED
