#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Bridge modes
typedef enum {
  BRIDGE_STANDALONE,  // Standalone UART bridge mode
  BRIDGE_NET         // Network setup mode
} BridgeMode;

// LED modes
typedef enum {
  LED_MODE_OFF,
  LED_MODE_WIFI_ON,      // Constantly ON in network setup mode
  LED_MODE_DATA_FLASH    // Flash on data activity in standalone mode
} LedMode;

// USB operation modes
enum UsbMode {
    USB_MODE_DEVICE = 0,
    USB_MODE_HOST,
    USB_MODE_AUTO
};

// Log levels
enum LogLevel {
    LOG_OFF     = -1, // Logging disabled
    LOG_ERROR   = 0,  // Critical errors only
    LOG_WARNING = 1,  // Warnings and important events
    LOG_INFO    = 2,  // General information
    LOG_DEBUG   = 3   // Detailed debug information
};

// Device 1 role
// TODO: Add SBUS support in future versions (2.4.x)
// Will require hardware signal inversion and special packet handling
enum Device1Role {
    D1_UART1 = 0      // Standard UART bridge
};

// Device 2 role
enum Device2Role {
    D2_NONE = 0,
    D2_UART2 = 1,
    D2_USB = 2
};

// Device 3 role
enum Device3Role {
    D3_NONE = 0,
    D3_UART3_MIRROR = 1,
    D3_UART3_BRIDGE = 2,
    D3_UART3_LOG = 3
};

// Device 4 role
enum Device4Role {
    D4_NONE = 0,
    D4_NETWORK_BRIDGE = 1,  // Future: Network Bridge
    D4_LOG_NETWORK = 2      // Future: Network Logger
};

// Device configuration
typedef struct {
  uint8_t role;
} DeviceConfig;

// Configuration structure
typedef struct {
  // Version for migration
  uint16_t config_version;
  
  // UART settings - now using ESP-IDF types
  uint32_t baudrate;
  uart_word_length_t databits;     // ESP-IDF enum instead of uint8_t
  uart_parity_t parity;             // ESP-IDF enum instead of String
  uart_stop_bits_t stopbits;        // ESP-IDF enum instead of uint8_t
  bool flowcontrol;

  // WiFi settings
  String ssid;
  String password;
  bool permanent_network_mode;  // Wi-Fi remains active without timeout

  // System info
  String device_version;
  String device_name;

  // USB mode
  UsbMode usb_mode;

  // Device configurations
  DeviceConfig device1;  // For future SBUS
  DeviceConfig device2;
  DeviceConfig device3;
  DeviceConfig device4;
  
  // Log levels
  LogLevel log_level_web;
  LogLevel log_level_uart;
  LogLevel log_level_network;
} Config;

// Traffic statistics
typedef struct {
  // Per-device byte counters
  unsigned long device1RxBytes;  // Device 1 RX (from external device)
  unsigned long device1TxBytes;  // Device 1 TX (to external device)
  unsigned long device2RxBytes;  // Device 2 RX
  unsigned long device2TxBytes;  // Device 2 TX
  unsigned long device3RxBytes;  // Device 3 RX (Bridge mode only)
  unsigned long device3TxBytes;  // Device 3 TX (Mirror/Bridge/Log)
  
  // General statistics
  unsigned long lastActivityTime;
  unsigned long deviceStartTime;
  unsigned long totalUartPackets;
} UartStats;

// System state
typedef struct {
  bool networkActive;              // Network mode is active
  bool isTemporaryNetwork;         // True for setup AP, false for permanent network
  unsigned long networkStartTime;  // When network mode started
  volatile int clickCount;
  volatile unsigned long lastClickTime;
  volatile bool buttonPressed;
  volatile unsigned long buttonPressTime;
} SystemState;

// Flow control detection results
typedef struct {
  bool flowControlDetected;
  bool flowControlActive;
} FlowControlStatus;

// Spinlock for statistics critical sections
extern portMUX_TYPE statsMux;

// Inline functions for convenient critical section handling
inline void enterStatsCritical() {
    taskENTER_CRITICAL(&statsMux);
}

inline void exitStatsCritical() {
    taskEXIT_CRITICAL(&statsMux);
}

// Forward declarations for interface types
class UartInterface;
class UsbInterface;

// Bridge operation context - contains pointers to local variables
struct BridgeContext {
    // Statistics (pointers to task-local variables)
    struct {
        unsigned long* device1RxBytes;
        unsigned long* device1TxBytes;
        unsigned long* device2RxBytes;
        unsigned long* device2TxBytes;
        unsigned long* device3RxBytes;
        unsigned long* device3TxBytes;
        unsigned long* lastActivity;
        unsigned long* totalUartPackets;
    } stats;
    
    // Adaptive buffering state
    struct {
        uint8_t* buffer;
        size_t bufferSize;
        int* bufferIndex;
        unsigned long* lastByteTime;
        unsigned long* bufferStartTime;
    } adaptive;
    
    // Cached device flags (for performance)
    struct {
        bool device2IsUSB;
        bool device2IsUART2;
        bool device3Active;
        bool device3IsBridge;
    } devices;
    
    // Diagnostics counters
    struct {
        unsigned long* droppedBytes;
        unsigned long* totalDroppedBytes;
        unsigned long* dropEvents;
        int* maxDropSize;
        int* timeoutDropSizes;      // Array [10]
        int* timeoutDropIndex;
    } diagnostics;
    
    // External interfaces
    struct {
        UartInterface* uartBridgeSerial;
        UsbInterface* usbInterface;
        UartInterface* device2Serial;
        UartInterface* device3Serial;
    } interfaces;
    
    // Timing controls
    struct {
        unsigned long* lastUartLedNotify;
        unsigned long* lastUsbLedNotify;
        unsigned long* lastWifiYield;
        unsigned long* lastDropLog;
    } timing;
    
    // Mutexes and synchronization
    struct {
        SemaphoreHandle_t* device3Mutex;
    } sync;
    
    // Current mode and configuration
    struct {
        BridgeMode* bridgeMode;  // Current bridge operation mode
        Config* config;
        UartStats* globalStats;
    } system;
};

// Initialize BridgeContext with all necessary pointers
inline void initBridgeContext(BridgeContext* ctx,
    // Statistics
    unsigned long* device1RxBytes, unsigned long* device1TxBytes,
    unsigned long* device2RxBytes, unsigned long* device2TxBytes,
    unsigned long* device3RxBytes, unsigned long* device3TxBytes,
    unsigned long* lastActivity, unsigned long* totalUartPackets,
    // Adaptive buffer
    uint8_t* buffer, size_t bufferSize, int* bufferIndex,
    unsigned long* lastByteTime, unsigned long* bufferStartTime,
    // Device flags
    bool device2IsUSB, bool device2IsUART2, bool device3Active, bool device3IsBridge,
    // Diagnostics
    unsigned long* droppedBytes, unsigned long* totalDroppedBytes,
    unsigned long* dropEvents, int* maxDropSize,
    int* timeoutDropSizes, int* timeoutDropIndex,
    // Interfaces
    UartInterface* uartBridgeSerial, UsbInterface* usbInterface,
    UartInterface* device2Serial, UartInterface* device3Serial,
    // Timing
    unsigned long* lastUartLedNotify, unsigned long* lastUsbLedNotify,
    unsigned long* lastWifiYield, unsigned long* lastDropLog,
    // Sync
    SemaphoreHandle_t* device3Mutex,
    // System
    BridgeMode* bridgeMode, Config* config, UartStats* globalStats)
{
    // Statistics
    ctx->stats.device1RxBytes = device1RxBytes;
    ctx->stats.device1TxBytes = device1TxBytes;
    ctx->stats.device2RxBytes = device2RxBytes;
    ctx->stats.device2TxBytes = device2TxBytes;
    ctx->stats.device3RxBytes = device3RxBytes;
    ctx->stats.device3TxBytes = device3TxBytes;
    ctx->stats.lastActivity = lastActivity;
    ctx->stats.totalUartPackets = totalUartPackets;
    
    // Adaptive buffer
    ctx->adaptive.buffer = buffer;
    ctx->adaptive.bufferSize = bufferSize;
    ctx->adaptive.bufferIndex = bufferIndex;
    ctx->adaptive.lastByteTime = lastByteTime;
    ctx->adaptive.bufferStartTime = bufferStartTime;
    
    // Device flags
    ctx->devices.device2IsUSB = device2IsUSB;
    ctx->devices.device2IsUART2 = device2IsUART2;
    ctx->devices.device3Active = device3Active;
    ctx->devices.device3IsBridge = device3IsBridge;
    
    // Diagnostics
    ctx->diagnostics.droppedBytes = droppedBytes;
    ctx->diagnostics.totalDroppedBytes = totalDroppedBytes;
    ctx->diagnostics.dropEvents = dropEvents;
    ctx->diagnostics.maxDropSize = maxDropSize;
    ctx->diagnostics.timeoutDropSizes = timeoutDropSizes;
    ctx->diagnostics.timeoutDropIndex = timeoutDropIndex;
    
    // Interfaces
    ctx->interfaces.uartBridgeSerial = uartBridgeSerial;
    ctx->interfaces.usbInterface = usbInterface;
    ctx->interfaces.device2Serial = device2Serial;
    ctx->interfaces.device3Serial = device3Serial;
    
    // Timing
    ctx->timing.lastUartLedNotify = lastUartLedNotify;
    ctx->timing.lastUsbLedNotify = lastUsbLedNotify;
    ctx->timing.lastWifiYield = lastWifiYield;
    ctx->timing.lastDropLog = lastDropLog;
    
    // Sync
    ctx->sync.device3Mutex = device3Mutex;
    
    // System
    ctx->system.bridgeMode = bridgeMode;
    ctx->system.config = config;
    ctx->system.globalStats = globalStats;
}

#endif // TYPES_H