// BLE GATT service definitions for Nordic UART Service (NUS)
// This file MUST be compiled as C (not C++) due to NimBLE's use of
// compound literals which have different lifetime semantics in C vs C++.
// See: https://github.com/espressif/esp-idf/issues/7895

#include "bluetooth_ble_gatt.h"

#if defined(BLE_ENABLED)

#include "host/ble_uuid.h"
#include "os/os_mbuf.h"
#include <string.h>

// Nordic UART Service UUIDs (128-bit, little-endian)
// Service: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
// RX Char: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E (Write - client sends TO ESP)
// TX Char: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E (Notify - ESP sends TO client)

static const ble_uuid128_t NUS_SERVICE_UUID =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);

static const ble_uuid128_t NUS_CHAR_RX_UUID =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);

static const ble_uuid128_t NUS_CHAR_TX_UUID =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

// TX characteristic attribute handle (set during GATT registration)
uint16_t g_nus_tx_attr_handle = 0;

// RX callback (set from C++ code)
static ble_rx_callback_t g_rx_callback = NULL;

void ble_gatt_set_rx_callback(ble_rx_callback_t callback) {
    g_rx_callback = callback;
}

// GATT characteristic access callback
int ble_gatt_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt* ctxt, void* arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // Data received from client (RX characteristic)
        if (g_rx_callback && ctxt->om) {
            uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
            uint8_t buf[256];
            if (len > sizeof(buf)) len = sizeof(buf);

            uint16_t copied = 0;
            ble_hs_mbuf_to_flat(ctxt->om, buf, len, &copied);

            if (copied > 0) {
                g_rx_callback(buf, copied);
            }
        }
    }
    return 0;
}

// GATT service definition - using C compound literals (valid in C, not C++)
const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        // Nordic UART Service
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &NUS_SERVICE_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // TX Characteristic (ESP -> Client, Notify)
                .uuid = &NUS_CHAR_TX_UUID.u,
                .access_cb = ble_gatt_chr_access,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &g_nus_tx_attr_handle,
            },
            {
                // RX Characteristic (Client -> ESP, Write)
                .uuid = &NUS_CHAR_RX_UUID.u,
                .access_cb = ble_gatt_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                0,  // End of characteristics
            },
        },
    },
    {
        0,  // End of services
    },
};

#endif // BLE_ENABLED
