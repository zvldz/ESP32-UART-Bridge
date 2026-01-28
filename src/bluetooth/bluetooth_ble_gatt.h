#pragma once

#if defined(BLE_ENABLED)

#ifdef __cplusplus
extern "C" {
#endif

#include "host/ble_hs.h"

// GATT service table (defined in bluetooth_ble_gatt.c)
extern const struct ble_gatt_svc_def gatt_svr_svcs[];

// TX characteristic attribute handle (for notifications)
extern uint16_t g_nus_tx_attr_handle;

// RX data callback type
typedef void (*ble_rx_callback_t)(const uint8_t* data, size_t len);

// Set RX callback (called from C++ code)
void ble_gatt_set_rx_callback(ble_rx_callback_t callback);

// GATT characteristic access callback (called by NimBLE)
int ble_gatt_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt* ctxt, void* arg);

#ifdef __cplusplus
}
#endif

#endif // BLE_ENABLED
