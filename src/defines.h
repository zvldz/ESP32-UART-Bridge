#ifndef DEFINES_H
#define DEFINES_H

/*
===============================================================================
                          ESP32 UART Bridge
===============================================================================

Universal UART to USB bridge with web configuration interface.
Optimized for drone autopilots (ArduPilot, PX4) but works with any UART protocol.

Hardware: ESP32-S3-Zero
- GPIO4: UART RX (connect to device TX)
- GPIO5: UART TX (connect to device RX)
- GPIO21: RGB LED (WS2812 - data activity indicator)
- GPIO0: BOOT button (triple-click for WiFi config)

===============================================================================

Note: Some features like Enhanced Flow Control diagnostics, UART error detection
(overrun, framing, parity errors) cannot be implemented with Arduino framework
due to limited access to hardware registers. Migration to ESP-IDF would enable
these features but is not justified for current requirements.

*/

// Device identification
#define DEVICE_NAME "ESP32 UART Bridge"
#define DEVICE_VERSION "2.1.1"

// Hardware pins
#define BOOT_PIN 0          // BOOT button is on GPIO0
#define UART_RX_PIN 4
#define UART_TX_PIN 5
#define BLUE_LED_PIN 21     // WS2812 RGB LED on GPIO21 for S3-Zero
#define RTS_PIN 6           // Reserved for flow control
#define CTS_PIN 7           // Reserved for flow control

// WiFi settings
#define WIFI_ACTIVATION_CLICKS 3
#define CLICK_TIMEOUT 3000  // 3 seconds between clicks
#define WIFI_TIMEOUT 1200000  // 20 minutes in milliseconds

// Default WiFi AP credentials
#define DEFAULT_AP_SSID "ESP-Bridge"
#define DEFAULT_AP_PASSWORD "12345678"

// Logging system
#define LOG_BUFFER_SIZE 100
#define LOG_DISPLAY_COUNT 50

// UART buffering
#define UART_BUFFER_SIZE 256

// UART task statistics update interval
#define UART_STATS_UPDATE_INTERVAL_MS 500  // How often UART task updates shared statistics

// Crash logging
#define CRASHLOG_MAX_ENTRIES 10              // Maximum number of crash entries to keep
#define CRASHLOG_FILE_PATH "/crashlog.json"  // Path to crash log file
#define CRASHLOG_MIN_HEAP_WARNING 15000      // Show warning if heap < 15KB
#define CRASHLOG_UPDATE_INTERVAL_MS 5000     // How often to update RTC variables

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