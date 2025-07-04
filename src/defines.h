#ifndef DEFINES_H
#define DEFINES_H

/*
===============================================================================
                          ESP32 UART Bridge v2.1.0
===============================================================================

Universal UART to USB bridge with web configuration interface.
Optimized for drone autopilots (ArduPilot, PX4) but works with any UART protocol.

Hardware: ESP32-C3 SuperMini
- GPIO4: UART RX (connect to device TX)
- GPIO5: UART TX (connect to device RX)
- GPIO8: Blue LED (data activity indicator)
- GPIO9: BOOT button (triple-click for WiFi config)

===============================================================================
                                TODO LIST
===============================================================================

PRIORITY 1 (Next tasks):
- [ ] Consider adding watchdog timer for UART task protection
      against Serial.write() blocking and mutex deadlocks

PRIORITY 2 (Medium priority):
- [ ] WiFi mode management - web interface settings:
      * "Persistent WiFi mode" option
      * Disable automatic WiFi timeout
      * Manual WiFi on/off control
- [ ] MAVLink protocol parsing mode (optional):
      * Parse and forward complete MAVLink packets
      * Only beneficial at speeds ≥ 460800 baud
      * Potential performance gain up to 20%
      * Not critical as high speeds are already fast
- [ ] Configurable GPIO pins for other ESP32 boards:
      * Web interface pin selection for TX/RX/CTS/RTS
      * Support different ESP32 variants pin layouts
      * Save pin configuration to config.json
      * Required when expanding beyond ESP32-C3 SuperMini

PRIORITY 3 (Future features):
- [ ] Alternative data transmission modes for WiFi CONFIG mode:
      * UDP forwarding - UART data over WiFi UDP packets
      * TCP server mode - UART data over TCP connection
      * WebSocket streaming for real-time data
      * Allow UART bridge to work simultaneously with WiFi
- [ ] Dark theme for web interface (store preference in config.json)
- [ ] Button handler module - extract button logic when adding features like:
      * Long press detection
      * Double-click actions
      * Button debouncing improvements
      * Additional button patterns
      (Currently ~100 lines in main.cpp, not worth extracting yet)
- [ ] SBUS protocol support:
      * SBUS input mode (RC receiver → USB)
      * SBUS output mode (USB → RC servos)
      * Simple implementation (inverted UART 100000 8E2)
      * Popular in RC/drone applications
      * Extends compatibility with flight controllers

COMPLETED:
- [x] Logging system with web buffer
- [x] DEBUG modes (0=production, 1=debug only)
- [x] Adaptive buffering for UART protocols
- [x] Web interface with real-time statistics

===============================================================================

Note: Some features like Enhanced Flow Control diagnostics, UART error detection
(overrun, framing, parity errors) cannot be implemented with Arduino framework
due to limited access to hardware registers. Migration to ESP-IDF would enable
these features but is not justified for current requirements.

*/

// Device identification
#define DEVICE_NAME "ESP32 UART Bridge"
#define DEVICE_VERSION "2.1.0"

// Hardware pins
#define BOOT_PIN 9          // BOOT button is on GPIO9
#define UART_RX_PIN 4
#define UART_TX_PIN 5
#define BLUE_LED_PIN 8      // Blue LED - controllable (data activity / WiFi status)
#define RED_LED_PIN 2       // Red LED - always on when ESP32 is powered (not controllable)
#define RTS_PIN 20          // Reserved for flow control
#define CTS_PIN 21          // Reserved for flow control

// WiFi settings
#define WIFI_ACTIVATION_CLICKS 3
#define CLICK_TIMEOUT 3000  // 3 seconds between clicks
#define WIFI_TIMEOUT 1200000  // 20 minutes in milliseconds

// Logging system
#define LOG_BUFFER_SIZE 100
#define LOG_DISPLAY_COUNT 20

// UART buffering
#define UART_BUFFER_SIZE 256

// FreeRTOS priorities based on core count
#ifdef CONFIG_FREERTOS_UNICORE
    // Single core - UART below WiFi (WiFi typically at configMAX_PRIORITIES-2)
    #define UART_TASK_PRIORITY (configMAX_PRIORITIES - 3)  
    #define WEB_TASK_PRIORITY (configMAX_PRIORITIES - 20)
#else
    // Multi core - UART highest priority
    #define UART_TASK_PRIORITY (configMAX_PRIORITIES - 1)
    #define WEB_TASK_PRIORITY (configMAX_PRIORITIES - 15)
#endif

// Debug mode - 0 = production UART bridge, 1 = debug only (no bridge functionality)
extern const int DEBUG_MODE;

#endif // DEFINES_H