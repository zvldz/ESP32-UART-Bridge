#ifndef DEFINES_H
#define DEFINES_H

/*
===============================================================================
                          ESP32 UART Bridge
===============================================================================

Universal UART to USB bridge with web configuration interface.
Optimized for drone autopilots (ArduPilot, PX4) but works with any UART protocol.

Hardware: ESP32-S3-Zero
- GPIO0: BOOT button (triple-click for WiFi config)
- GPIO4/5: Device 1 - Main UART (RX/TX)
- GPIO6/7: RTS/CTS flow control
- GPIO8/9: Device 2 - Secondary UART (RX/TX) when enabled
- GPIO11/12: Device 3 - Logger/Mirror/Bridge UART (RX/TX)
- GPIO21: RGB LED (WS2812 - data activity indicator)

===============================================================================

Note: Some features like Enhanced Flow Control diagnostics, UART error detection
(overrun, framing, parity errors) cannot be implemented with Arduino framework
due to limited access to hardware registers. Migration to ESP-IDF would enable
these features but is not justified for current requirements.

*/

// Ensure multi-core ESP32 only
#if CONFIG_FREERTOS_UNICORE == 1
    #error "This firmware requires a multi-core ESP32. Single-core variants are not supported."
#endif

// Device identification
#define DEVICE_NAME "ESP32 UART Bridge"
#define DEVICE_VERSION "2.13.2"

// Hardware pins - Device 1 (Main UART)
#define BOOT_BUTTON_PIN 0
#define UART_RX_PIN 4
#define UART_TX_PIN 5
#define LED_PIN1 21         // WS2812 RGB LED on GPIO21 for S3-Zero
#define RTS_PIN 6           // Flow control
#define CTS_PIN 7           // Flow control

// Device 2 - Secondary UART pins
#define DEVICE2_UART_RX_PIN 8
#define DEVICE2_UART_TX_PIN 9

// Device 3 - Logger/Mirror/Bridge pins
#define DEVICE3_UART_RX_PIN 11  // Used only in Bridge mode
#define DEVICE3_UART_TX_PIN 12  // Used in all modes

// WiFi settings
#define WIFI_ACTIVATION_CLICKS 3
#define CLICK_TIMEOUT 3000  // 3 seconds between clicks
#define WIFI_TIMEOUT 1200000  // 20 minutes in milliseconds

// Default WiFi AP credentials
#define DEFAULT_AP_SSID "ESP-Bridge"
#define DEFAULT_AP_PASSWORD "12345678"

// WiFi Client Mode constants
#define WIFI_CLIENT_RETRY_INTERVAL_MS  10000  // Retry every 10 seconds
#define WIFI_CLIENT_SCAN_INTERVAL_MS   15000  // Scan for networks every 15 seconds
#define WIFI_CLIENT_BLINK_INTERVAL_MS  2000   // Orange slow blink for searching
#define WIFI_CLIENT_ERROR_BLINK_MS     500    // Fast blink for errors
#define WIFI_CLIENT_MAX_RETRIES        5      // Max password attempts before error state

// RSSI thresholds for percentage calculation
#define WIFI_RSSI_EXCELLENT -30  // 100% signal
#define WIFI_RSSI_POOR      -90  // 0% signal

// Logging system
#define LOG_BUFFER_SIZE 100
#define LOG_DISPLAY_COUNT 50

// UART buffering
#define DEVICE2_UART_BUFFER_SIZE 2048
#define DEVICE3_UART_BUFFER_SIZE 2048
#define DEVICE3_LOG_BUFFER_SIZE 256  // For UART logger TX only

// Device 4 buffering
#define DEVICE4_LOG_BUFFER_SIZE 2048
#define DEVICE4_BRIDGE_BUFFER_SIZE 2048

// UART task statistics update interval
#define UART_STATS_UPDATE_INTERVAL_MS 500  // How often UART task updates shared statistics

// LED timing constants
#define LED_DATA_FLASH_MS      50    // Data activity flash duration
#define LED_BUTTON_FLASH_MS    100   // Button click feedback duration
#define LED_WIFI_RESET_BLINK_MS 100  // WiFi reset rapid blink interval

// LED colors for Device 3
#define COLOR_MAGENTA   0xFF00FF  // Device 3 TX
#define COLOR_YELLOW    0xFFFF00  // Device 3 RX (Bridge mode)

// LED colors - WiFi Client mode
#define COLOR_ORANGE    0xFF8000  // Client mode connected
#define COLOR_RED       0xFF0000  // Client mode error

// Crash logging
#define CRASHLOG_MAX_ENTRIES 10              // Maximum number of crash entries to keep
#define CRASHLOG_FILE_PATH "/crashlog.json"  // Path to crash log file
#define CRASHLOG_MIN_HEAP_WARNING 15000      // Show warning if heap < 15KB
#define CRASHLOG_UPDATE_INTERVAL_MS 5000     // How often to update RTC variables

// FreeRTOS priorities for multi-core ESP32
#define UART_TASK_PRIORITY (configMAX_PRIORITIES - 1)  // Highest priority for UART
#define WEB_TASK_PRIORITY (configMAX_PRIORITIES - 15) // Lower priority for web server

// Core assignments for multi-core ESP32
#define UART_TASK_CORE 0      // Main UART bridge task
#define WEB_TASK_CORE 1       // Web server task
#define DEVICE3_TASK_CORE 0   // Device 3 operations (same as UART)
#define DEVICE4_TASK_CORE 1   // Device 4 network operations (same as Web)

#endif // DEFINES_H