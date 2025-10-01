// types.h - simplified version
#pragma once
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Include split headers
#include "device_stats.h"
#include "device_types.h"

// Protocol types from new architecture
#include "protocols/protocol_types.h"

// Forward declarations
class ProtocolPipeline;

// Keep only core types in types.h
// Bridge modes
typedef enum {
    BRIDGE_STANDALONE,
    BRIDGE_NET
} BridgeMode;

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
    LED_MODE_WIFI_ON,
    LED_MODE_DATA_FLASH,
    LED_MODE_WIFI_CLIENT_CONNECTED,
    LED_MODE_WIFI_CLIENT_SEARCHING,
    LED_MODE_WIFI_CLIENT_ERROR,
    LED_MODE_SAFE_MODE
} LedMode;

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

    // Firmware update flag
    bool firmwareUpdateInProgress;   // True during OTA firmware update
} SystemState;


// Forward declarations for interface types
class UartInterface;
class UsbInterface;
class CircularBuffer;

// Forward declaration for protocol statistics
struct ProtocolStats;

// Bridge operation context - simplified after statistics refactoring
struct BridgeContext {
    // Adaptive buffering state
    struct {
        // Legacy fields for compatibility
        size_t bufferSize;
        unsigned long* lastByteTime;
        unsigned long* bufferStartTime;
    } adaptive;
    
    // Protocol buffers - separated by purpose
    struct {
        CircularBuffer* telemetryBuffer;   // Existing: FC→GCS
        CircularBuffer* logBuffer;          // Existing: Logger mode
        CircularBuffer* udpRxBuffer;       // Existing: UDP receive (keep for AsyncUDP)
        
        // NEW: Input buffers for each source
        CircularBuffer* usbInputBuffer;    // USB→FC commands
        CircularBuffer* udpInputBuffer;    // UDP→FC commands (after udpRxBuffer)
        CircularBuffer* uart2InputBuffer;  // UART2→FC commands
        CircularBuffer* uart3InputBuffer;  // UART3→FC commands
    } buffers;
    
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
    
    // Timing controls - LED timing removed (handled by LED monitor task)
    struct {
        unsigned long* lastWifiYield;
        unsigned long* lastDropLog;
    } timing;
    
    // Current mode and configuration
    struct {
        BridgeMode* bridgeMode;  // Current bridge operation mode
        Config* config;
    } system;
    
    // Protocol state (simplified)
    struct {
        ProtocolType type;               // Protocol type from config
        ProtocolStats* stats;            // Protocol statistics
        bool enabled;                    // Protocol detection enabled flag
        size_t detectedPacketSize;       // Size of detected packet
        bool packetInProgress;           // Currently receiving packet
        uint32_t packetStartTime;        // When packet reception started
        
        // Track last detected packet to avoid double counting
        size_t lastDetectedOffset;       // Position of last counted packet in buffer
        size_t lastDetectedSize;         // Size of last counted packet
        bool statsUpdated;               // Were stats already updated for current packet
        
        // Error tracking for future adaptive behavior
        uint32_t consecutiveErrors;     // Count of failed detection attempts
        uint32_t lastValidPacketTime;   // Timestamp of last valid packet
        bool temporarilyDisabled;       // Reserved for future auto-disable feature
        
        // Buffer analysis state
        size_t lastAnalyzedOffset;      // Position in buffer we've analyzed up to
        size_t currentPacketStart;      // Start position of current packet in buffer
        bool packetFound;               // Valid packet found in buffer
        size_t skipBytes;               // Bytes to skip before packet (from detector to TX)
    } protocol;
    
    // NEW: Protocol pipeline for Parser + Sender architecture
    class ProtocolPipeline* protocolPipeline;
};

// Initialize BridgeContext - simplified after statistics refactoring
inline void initBridgeContext(BridgeContext* ctx,
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
    unsigned long* lastWifiYield, unsigned long* lastDropLog,
    // System
    BridgeMode* bridgeMode, Config* config)
{
    // Adaptive buffer
    ctx->adaptive.bufferSize = bufferSize;
    ctx->adaptive.lastByteTime = lastByteTime;
    ctx->adaptive.bufferStartTime = bufferStartTime;
    
    // Initialize buffers - will be allocated by buffer manager
    ctx->buffers.telemetryBuffer = nullptr;
    ctx->buffers.logBuffer = nullptr;
    ctx->buffers.udpRxBuffer = nullptr;
    
    // NEW: Initialize input buffers
    ctx->buffers.usbInputBuffer = nullptr;
    ctx->buffers.udpInputBuffer = nullptr;
    ctx->buffers.uart2InputBuffer = nullptr;
    ctx->buffers.uart3InputBuffer = nullptr;
    
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
    ctx->timing.lastWifiYield = lastWifiYield;
    ctx->timing.lastDropLog = lastDropLog;
    
    // System
    ctx->system.bridgeMode = bridgeMode;
    ctx->system.config = config;
    
    // Protocol state initialization
    ctx->protocol.enabled = false;
    ctx->protocol.type = PROTOCOL_NONE;
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
    ctx->protocol.lastAnalyzedOffset = 0;
    ctx->protocol.currentPacketStart = 0;
    ctx->protocol.packetFound = false;
    ctx->protocol.skipBytes = 0;
    
    // NEW: Initialize protocol pipeline
    ctx->protocolPipeline = nullptr;  // Will be initialized later
}