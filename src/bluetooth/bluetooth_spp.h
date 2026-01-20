// Bluetooth SPP interface for MiniKit (ESP32 WROOM-32)
// Uses ESP-IDF SPP API directly - memory allocated only when init() is called
#if defined(MINIKIT_BT_ENABLED)

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "../defines.h"

// Forward declaration
class CircularBuffer;

class BluetoothSPP {
private:
    bool initialized = false;
    bool connected = false;
    uint32_t sppHandle = 0;

    // Ring buffer for RX data (used if no external buffer set)
    static constexpr size_t RX_BUFFER_SIZE = 1024;
    uint8_t rxBuffer[RX_BUFFER_SIZE];
    volatile size_t rxHead = 0;
    volatile size_t rxTail = 0;

    // External input buffer (for pipeline integration)
    CircularBuffer* externalInputBuffer = nullptr;

    // Device info
    char deviceName[32];

public:
    // PIN code - public for GAP callback access
    char pinCode[17];
    BluetoothSPP() = default;
    ~BluetoothSPP();

    // Initialize Bluetooth with name and PIN
    // This is where BT controller memory is allocated
    bool init(const char* name, const char* pin);
    void end();

    // Stream-like interface
    int available();
    int read();
    size_t readBytes(uint8_t* buffer, size_t length);
    size_t write(uint8_t byte);
    size_t write(const uint8_t* buffer, size_t size);
    void flush();

    // Connection status
    bool hasClient();
    bool isConnected();

    // Get device name (for logging)
    const char* getName() const { return deviceName; }

    // Set external buffer for pipeline integration
    void setInputBuffer(CircularBuffer* buffer) { externalInputBuffer = buffer; }

    // Callbacks from ESP-IDF (called from static callback)
    void onSppData(const uint8_t* data, uint16_t len);
    void onSppConnect(uint32_t handle);
    void onSppDisconnect();
};

// Global instance pointer (created in device_init.cpp)
extern BluetoothSPP* bluetoothSPP;

#endif // MINIKIT_BT_ENABLED
