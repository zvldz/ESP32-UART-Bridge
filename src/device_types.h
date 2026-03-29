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

// WiFi AP mode at boot
enum WifiApMode {
    WIFI_AP_DISABLED = 0,   // No WiFi at boot (buttons still work)
    WIFI_AP_TEMPORARY = 1,  // WiFi at boot, auto-disable after timeout
    WIFI_AP_ALWAYS_ON = 2   // WiFi always on
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
    D1_SBUS_IN = 1,     // SBUS input from RC receiver (100000 8E2 inverted)
    D1_CRSF_IN = 2      // CRSF/ELRS input (420000 8N1 non-inverted)
};

// Device 2 role
enum Device2Role {
    D2_NONE = 0,
    D2_UART2 = 1,
    D2_USB = 2,
    D2_SBUS_IN = 3,        // SBUS input from RC receiver (UART2)
    D2_SBUS_OUT = 4,       // SBUS output to servos (UART2)
    D2_USB_SBUS_TEXT = 5,  // SBUS text output via USB (for MiniKit)
    D2_USB_LOG = 6,        // USB logger output (Device mode only)
    D2_USB_CRSF_TEXT = 7,  // CRSF/ELRS text output via USB
    D2_USB_CRSF_BRIDGE = 8 // CRSF/ELRS binary bridge via USB (bidirectional)
};

// Device 3 role
enum Device3Role {
    D3_NONE = 0,
    D3_UART3_MIRROR = 1,
    D3_UART3_BRIDGE = 2,
    D3_UART3_LOG = 3,
    D3_SBUS_IN = 4,    // SBUS input from RC receiver
    D3_SBUS_OUT = 5,   // SBUS output to servos
    D3_CRSF_BRIDGE = 6 // CRSF/ELRS binary bridge via UART3 (bidirectional)
};

// Device 4 role
enum Device4Role {
    D4_NONE = 0,
    D4_NETWORK_BRIDGE = 1,  // Full bridge (MAVLink/Raw)
    D4_LOG_NETWORK = 2,     // Logger
    D4_SBUS_UDP_TX = 3,     // SBUS→UDP only
    D4_SBUS_UDP_RX = 4,     // UDP→SBUS only
    D4_CRSF_TEXT = 5         // CRSF text output via UDP
};

// Device 5 role (Bluetooth - Classic SPP on MiniKit, BLE on S3)
#if defined(MINIKIT_BT_ENABLED) || defined(BLE_ENABLED)
enum Device5Role {
    D5_NONE = 0,
    D5_BT_BRIDGE = 1,      // Bluetooth bridge (Raw/MAVLink per protocolOptimization)
    D5_BT_SBUS_TEXT = 2,   // SBUS text format over Bluetooth
    D5_BT_CRSF_TEXT = 3    // CRSF text output over Bluetooth
};
#endif

// SBUS output format
enum SbusOutputFormat : uint8_t {
    SBUS_FMT_BINARY = 0,   // Standard SBUS (100000 8E2 INV) → FC
    SBUS_FMT_TEXT = 1      // "RC 1500,..." (115200 8N1) → PC/App
#ifdef SBUS_MAVLINK_SUPPORT
    , SBUS_FMT_MAVLINK = 2 // RC_CHANNELS_OVERRIDE → MP/FC
#endif
    // Note: value 2 reserved for MAVLINK, new formats should start from 3
};

// CRSF text filter bitmask (which frame groups to output as text)
// Default 0x3F = all groups enabled. Binary bridge outputs ignore this filter.
enum CrsfFilterBit : uint8_t {
    CRSF_FILTER_RC         = 0x01,  // Bit 0: RC Channels (type 0x16)
    CRSF_FILTER_LINK_STATS = 0x02,  // Bit 1: Link Statistics (LQ, RSSI, SNR)
    CRSF_FILTER_BATTERY    = 0x04,  // Bit 2: Battery (voltage, current, mAh)
    CRSF_FILTER_GPS        = 0x08,  // Bit 3: GPS (coordinates, speed, heading)
    CRSF_FILTER_ATTITUDE   = 0x10,  // Bit 4: Attitude (pitch, roll, yaw)
    CRSF_FILTER_FLIGHT_MODE = 0x20, // Bit 5: Flight Mode (string)
    CRSF_FILTER_ALL        = 0x3F   // Bits 0-5: All groups enabled
};

// Device configuration
typedef struct {
    uint8_t role;
    uint8_t sbusOutputFormat;  // SbusOutputFormat: BINARY, TEXT, or MAVLINK
    uint8_t outRate;           // Text output rate in Hz (10-70, default 50)
    uint8_t crsfFilter;        // CRSF text filter bitmask (default: CRSF_FILTER_ALL)
} DeviceConfig;

// Device 4 Configuration
struct Device4Config {
    char target_ip[96];     // Multiple IPs comma-separated (ignored if auto_broadcast=true)
    uint16_t port;
    uint8_t role;
    bool auto_broadcast;    // Use dynamic broadcast from DHCP subnet (Client mode only)
    uint8_t sbusOutputFormat;  // SbusOutputFormat: BINARY, TEXT, or MAVLINK (only for D4_SBUS_UDP_TX)
    uint16_t udpSourceTimeout; // UDP source timeout in ms (100-5000, default 1000) for D4_SBUS_UDP_RX
    uint8_t udpSendRate;       // Send rate in Hz (10-70, default 50) for D4_SBUS_UDP_TX
    uint8_t crsfFilter;        // CRSF text filter bitmask (default: CRSF_FILTER_ALL)
};

// Device 5 Configuration (Bluetooth - Classic SPP on MiniKit, BLE on S3)
// Note: BT device name uses mDNS hostname (config.mdns_hostname)
// Uses "Just Works" pairing (no PIN required)
#if defined(MINIKIT_BT_ENABLED) || defined(BLE_ENABLED)
struct Device5Config {
    uint8_t role;           // Device5Role
    uint8_t btSendRate;     // Send rate in Hz for SBUS Text mode (10-70, default 50)
    uint8_t crsfFilter;     // CRSF text filter bitmask (default: CRSF_FILTER_ALL)
};
#endif

// WiFi network credentials for Client mode
struct WiFiNetwork {
    String ssid;
    String password;
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
    WifiApMode wifi_ap_mode;
    
    // WiFi mode selection
    BridgeWiFiMode wifi_mode;
    WiFiNetwork wifi_networks[5];  // Client mode networks (index 0 = primary/highest priority)
    uint8_t wifi_tx_power;  // WiFi TX power level (8-80, in 0.25dBm steps: 8=2dBm, 80=20dBm)
    uint8_t wifi_ap_channel;  // WiFi AP channel (1-13, 0 = auto/default to 1)
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
    bool terminalAnsi;  // Terminal: use xterm.js ANSI rendering

    // SBUS settings
    bool sbusTimingKeeper;

#if defined(MINIKIT_BT_ENABLED) || defined(BLE_ENABLED)
    // Device 5 - Bluetooth (Classic SPP or BLE)
    Device5Config device5_config;
#endif
} Config;