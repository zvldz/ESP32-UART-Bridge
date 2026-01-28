#pragma once

#if defined(BLE_ENABLED)

#include <Arduino.h>

// Forward declaration for pipeline integration
class CircularBuffer;

// Nordic UART Service (NUS) UUIDs
// Note: TX/RX naming is from the perspective of the peripheral (ESP)
// Service: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
// RX Char: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E (Write - client sends TO ESP)
// TX Char: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E (Notify - ESP sends TO client)

// BLE Serial buffer sizes
#define BLE_RX_BUFFER_SIZE      512
#define BLE_TX_MTU_SIZE         244  // BLE 5.0 max payload (247 - 3 header)

class BluetoothBLE {
public:
    BluetoothBLE();
    ~BluetoothBLE();

    // Initialize BLE with device name
    bool init(const char* deviceName);

    // Shutdown BLE
    void deinit();

    // Connection status
    bool isConnected() const { return connected; }
    bool hasClient() const { return connected; }  // Alias for pipeline compatibility

    // Get device name
    const char* getName() const { return deviceName; }

    // Data transmission (ESP → client via notify)
    size_t write(const uint8_t* data, size_t len);
    size_t write(const char* str);

    // Data reception (client → ESP)
    size_t available();
    size_t read(uint8_t* buffer, size_t maxLen);
    int read();  // Single byte read

    // Flush TX (send any pending notifications)
    void flush();

    // Pipeline integration - external buffer for RX data
    void setInputBuffer(CircularBuffer* buffer) { externalInputBuffer = buffer; }

    // Statistics
    uint32_t getTxBytes() const { return txBytes; }
    uint32_t getRxBytes() const { return rxBytes; }
    void resetStats() { txBytes = 0; rxBytes = 0; }

    // Called from NimBLE callbacks (internal use)
    void onConnect(uint16_t connHandle);
    void onDisconnect(uint16_t connHandle, int reason);
    void onRxData(const uint8_t* data, size_t len);

private:
    // Internal state
    char deviceName[32];
    bool initialized = false;
    volatile bool connected = false;
    uint16_t connHandle = 0;
    uint16_t txAttrHandle = 0;  // TX characteristic attribute handle

    // RX buffer (circular) - used when no external buffer set
    uint8_t rxBuffer[BLE_RX_BUFFER_SIZE];
    volatile size_t rxHead = 0;
    volatile size_t rxTail = 0;

    // External input buffer (for pipeline integration)
    CircularBuffer* externalInputBuffer = nullptr;

    // Statistics
    uint32_t txBytes = 0;
    uint32_t rxBytes = 0;
};

// Global instance (created in device_init.cpp when BLE role enabled)
extern BluetoothBLE* bluetoothBLE;

#endif // BLE_ENABLED
