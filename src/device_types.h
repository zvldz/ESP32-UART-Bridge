// device_types.h
#pragma once
#include <Arduino.h>  // for String
#include "driver/uart.h"

// WiFi modes
enum BridgeWiFiMode {
    BRIDGE_WIFI_MODE_AP = 0,
    BRIDGE_WIFI_MODE_CLIENT = 1,
    BRIDGE_WIFI_MODE_AP_STA = 2
};

// USB modes
enum UsbMode {
    USB_MODE_DEVICE = 0,
    USB_MODE_HOST
};

// Log levels
enum LogLevel {
    LOG_OFF = -1,
    LOG_ERROR = 0,
    LOG_WARNING = 1,
    LOG_INFO = 2,
    LOG_DEBUG = 3
};

// Device 1 role
enum Device1Role {
    D1_UART1 = 0,       // Default: Normal UART bridge at configured baudrate
    D1_SBUS_IN = 1      // NEW: SBUS input from RC receiver (100000 8E2 inverted)
};

// Device 2 role
enum Device2Role {
    D2_NONE = 0,
    D2_UART2 = 1,
    D2_USB = 2,
    D2_SBUS_IN = 3,   // NEW: SBUS input from RC receiver
    D2_SBUS_OUT = 4   // NEW: SBUS output to servos
};

// Device 3 role
enum Device3Role {
    D3_NONE = 0,
    D3_UART3_MIRROR = 1,
    D3_UART3_BRIDGE = 2,
    D3_UART3_LOG = 3,
    // 4 reserved
    D3_SBUS_OUT = 5
};

// Device 4 role
enum Device4Role {
    D4_NONE = 0,
    D4_NETWORK_BRIDGE = 1,  // Full bridge (MAVLink/Raw)
    D4_LOG_NETWORK = 2,     // Logger
    D4_SBUS_UDP_TX = 3,     // NEW: SBUS→UDP only
    D4_SBUS_UDP_RX = 4      // NEW: UDP→SBUS only
};

// Device configuration
typedef struct {
    uint8_t role;
} DeviceConfig;

// Device 4 Configuration
struct Device4Config {
    char target_ip[16];
    uint16_t port;
    uint8_t role;
};


// Configuration structure (moved here from types.h)
typedef struct {
    uint16_t config_version;
    
    // UART settings
    uint32_t baudrate;
    uart_word_length_t databits;
    uart_parity_t parity;
    uart_stop_bits_t stopbits;
    bool flowcontrol;
    
    // WiFi settings
    String ssid;
    String password;
    bool permanent_network_mode;
    
    // WiFi mode selection
    BridgeWiFiMode wifi_mode;
    String wifi_client_ssid;
    String wifi_client_password;
    uint8_t wifi_tx_power;  // WiFi TX power level (8-80, in 0.25dBm steps: 8=2dBm, 80=20dBm)
    String mdns_hostname;   // Custom mDNS hostname (empty = auto-generate on startup)
    
    // System info
    String device_version;
    String device_name;
    
    // USB mode
    UsbMode usb_mode;
    
    // Device configurations
    DeviceConfig device1;
    DeviceConfig device2;
    DeviceConfig device3;
    DeviceConfig device4;
    Device4Config device4_config;
    
    // Log levels
    LogLevel log_level_web;
    LogLevel log_level_uart;
    LogLevel log_level_network;
    
    // Protocol optimization
    uint8_t protocolOptimization;
    bool udpBatchingEnabled;
    bool mavlinkRouting;

    // SBUS settings
    bool sbusTimingKeeper;
} Config;