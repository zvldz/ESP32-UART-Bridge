#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Forward declarations
class ProtocolPipeline;

// Protocol types from new architecture
#include "protocols/protocol_types.h"

// Bridge modes
typedef enum {
  BRIDGE_STANDALONE,  // Standalone UART bridge mode
  BRIDGE_NET         // Network setup mode
} BridgeMode;

// WiFi operation modes for our bridge
enum BridgeWiFiMode {
    BRIDGE_WIFI_MODE_AP = 0,      // Access Point mode (current)
    BRIDGE_WIFI_MODE_CLIENT = 1,  // Client mode (STA)
    BRIDGE_WIFI_MODE_AP_STA = 2   // Future: simultaneous AP+STA
};

// WiFi Client connection states
enum WiFiClientState {
    CLIENT_IDLE = 0,
    CLIENT_SCANNING,
    CLIENT_CONNECTING,
    CLIENT_CONNECTED,
    CLIENT_WRONG_PASSWORD,
    CLIENT_NO_SSID
};

// LED modes
typedef enum {
  LED_MODE_OFF,
  LED_MODE_WIFI_ON,                // AP mode - purple constant
  LED_MODE_DATA_FLASH,             // Data activity flash
  LED_MODE_WIFI_CLIENT_CONNECTED,  // Client connected - orange constant
  LED_MODE_WIFI_CLIENT_SEARCHING,  // Client searching - orange slow blink
  LED_MODE_WIFI_CLIENT_ERROR,      // Wrong password - orange fast blink
  LED_MODE_SAFE_MODE               // Safe mode - red blink every 5s
} LedMode;

// USB operation modes
enum UsbMode {
    USB_MODE_DEVICE = 0,
    USB_MODE_HOST
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
    D4_NETWORK_BRIDGE = 1,  // Network Bridge
    D4_LOG_NETWORK = 2      // Network Logger
};

// OLD enum ProtocolType removed - now using protocol_types.h
// PROTOCOL_NONE is now mapped to PROTOCOL_RAW in new architecture

// Device 4 Configuration
struct Device4Config {
    char target_ip[16];      // Target IP address for UDP packets
    uint16_t port;           // Target port for UDP packets
    uint8_t role;            // Copy of role from device4
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
  
  // WiFi mode selection
  BridgeWiFiMode wifi_mode;
  String wifi_client_ssid;
  String wifi_client_password;

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
  
  // Device 4 network configuration
  Device4Config device4_config;
  
  // Log levels
  LogLevel log_level_web;
  LogLevel log_level_uart;
  LogLevel log_level_network;
  
  // Protocol optimization
  uint8_t protocolOptimization;  // Protocol detection mode (ProtocolType enum)
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
  
  // Device 4 statistics
  unsigned long device4TxBytes;    // Device 4 TX (Network Logger/Bridge)
  unsigned long device4TxPackets;  // Device 4 TX packet count
  unsigned long device4RxBytes;    // Device 4 RX (Bridge mode only)
  unsigned long device4RxPackets;  // Device 4 RX packet count
  
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
  
  // Client mode state
  WiFiClientState wifiClientState;
  int wifiRetryCount;
  bool wifiClientConnected;
  int wifiRSSI;  // Signal strength in dBm
  
  // Temporary mode override
  bool tempForceApMode;            // Force AP mode for this session (triple click from client)
  
  // Safe mode flag
  bool wifiSafeMode;               // WiFi safe mode after initialization failures
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
class ProtocolDetector;
class CircularBuffer;

// Forward declaration for protocol statistics (will be defined in future phases)
struct ProtocolStats;

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
        unsigned long* device4RxBytes;      // Device 4 RX bytes
        unsigned long* device4TxBytes;      // Device 4 TX bytes
        unsigned long* device4RxPackets;    // Device 4 RX packet count
        unsigned long* device4TxPackets;    // Device 4 TX packet count
        unsigned long* lastActivity;
        unsigned long* totalUartPackets;
    } stats;
    
    // Adaptive buffering state
    struct {
        // Legacy fields for compatibility
        size_t bufferSize;
        unsigned long* lastByteTime;
        unsigned long* bufferStartTime;
        
        // New circular buffer
        CircularBuffer* circBuf;   // Main buffer implementation
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
    
    // Protocol detection state
    struct {
        bool enabled;                    // Is protocol detection active
        ProtocolDetector* detector;      // Current protocol detector
        size_t detectedPacketSize;       // Size of detected packet
        bool packetInProgress;           // Currently receiving packet
        uint32_t packetStartTime;        // When packet reception started
        ProtocolStats* stats;            // Protocol statistics (nullptr in Phase 4.1)
        size_t minBytesNeeded;           // Cached minimum bytes for detection
        
        // Track last detected packet to avoid double counting
        size_t lastDetectedOffset;       // Position of last counted packet in buffer
        size_t lastDetectedSize;         // Size of last counted packet
        bool statsUpdated;               // Were stats already updated for current packet
        
        // Error tracking for future adaptive behavior
        uint32_t consecutiveErrors;     // Count of failed detection attempts
        uint32_t lastValidPacketTime;   // Timestamp of last valid packet
        bool temporarilyDisabled;       // Reserved for future auto-disable feature
        
        // MAVFtp mode detection
        bool mavftpActive;              // True when FILE_TRANSFER_PROTOCOL detected
        uint32_t lastMavftpTime;        // Time of last MAVFtp packet
        uint16_t mavftpCount;           // Counter of consecutive MAVFtp packets
        
        // Buffer analysis state
        size_t lastAnalyzedOffset;      // Position in buffer we've analyzed up to
        size_t currentPacketStart;      // Start position of current packet in buffer
        bool packetFound;               // Valid packet found in buffer
        size_t skipBytes;               // Bytes to skip before packet (from detector to TX)
    } protocol;
    
    // NEW: Protocol pipeline for Parser + Sender architecture
    class ProtocolPipeline* protocolPipeline;
};

// Initialize BridgeContext with all necessary pointers
inline void initBridgeContext(BridgeContext* ctx,
    // Statistics
    unsigned long* device1RxBytes, unsigned long* device1TxBytes,
    unsigned long* device2RxBytes, unsigned long* device2TxBytes,
    unsigned long* device3RxBytes, unsigned long* device3TxBytes,
    unsigned long* device4RxBytes, unsigned long* device4TxBytes,
    unsigned long* device4RxPackets, unsigned long* device4TxPackets,
    unsigned long* lastActivity, unsigned long* totalUartPackets,
    // Adaptive buffer
    size_t bufferSize,
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
    ctx->stats.device4RxBytes = device4RxBytes;
    ctx->stats.device4TxBytes = device4TxBytes;
    ctx->stats.device4RxPackets = device4RxPackets;
    ctx->stats.device4TxPackets = device4TxPackets;
    ctx->stats.lastActivity = lastActivity;
    ctx->stats.totalUartPackets = totalUartPackets;
    
    // Adaptive buffer
    ctx->adaptive.bufferSize = bufferSize;
    ctx->adaptive.lastByteTime = lastByteTime;
    ctx->adaptive.bufferStartTime = bufferStartTime;
    ctx->adaptive.circBuf = nullptr;         // Will be allocated later
    
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
    
    // CRITICAL FIX: Initialize protocol structure to prevent crashes
    ctx->protocol.enabled = false;
    ctx->protocol.detector = nullptr;
    ctx->protocol.detectedPacketSize = 0;
    ctx->protocol.packetInProgress = false;
    ctx->protocol.packetStartTime = 0;
    ctx->protocol.stats = nullptr;
    ctx->protocol.lastDetectedOffset = 0;
    ctx->protocol.lastDetectedSize = 0;
    ctx->protocol.statsUpdated = false;
    ctx->protocol.consecutiveErrors = 0;
    ctx->protocol.lastValidPacketTime = 0;
    ctx->protocol.temporarilyDisabled = false;
    ctx->protocol.mavftpActive = false;
    ctx->protocol.lastMavftpTime = 0;
    ctx->protocol.mavftpCount = 0;
    ctx->protocol.lastAnalyzedOffset = 0;
    ctx->protocol.currentPacketStart = 0;
    ctx->protocol.packetFound = false;
    ctx->protocol.skipBytes = 0;
    
    // NEW: Initialize protocol pipeline
    ctx->protocolPipeline = nullptr;  // Will be initialized later
}

#endif // TYPES_H