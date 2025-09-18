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

*/

// Ensure multi-core ESP32 only
#if CONFIG_FREERTOS_UNICORE == 1
    #error "This firmware requires a multi-core ESP32. Single-core variants are not supported."
#endif

// Device identification
#define DEVICE_NAME "ESP32 UART Bridge"
#define DEVICE_VERSION "2.18.2"

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

// Logging system
#define LOG_BUFFER_SIZE 100
#define LOG_DISPLAY_COUNT 95

// Crash logging (need move to crashlog)
#define CRASHLOG_MAX_ENTRIES 10              // Maximum number of crash entries to keep
#define CRASHLOG_FILE_PATH "/crashlog.json"  // Path to crash log file
#define CRASHLOG_MIN_HEAP_WARNING 15000      // Show warning if heap < 15KB
#define CRASHLOG_UPDATE_INTERVAL_MS 5000     // How often to update RTC variables
#define CRASHLOG_MAX_FILE_SIZE  4096         // Maximum crash log file size

// FreeRTOS priorities for multi-core ESP32
#define UART_TASK_PRIORITY (configMAX_PRIORITIES - 4)  // Highest priority for UART !!
#define WEB_TASK_PRIORITY (configMAX_PRIORITIES - 15) // Lower priority for web server

// Core assignments for multi-core ESP32
#define UART_TASK_CORE 0      // Main UART bridge task
#define UART_DMA_TASK_CORE 0  // UART DMA task (same as UART)
#define WEB_TASK_CORE 1       // Web server task
//#define DEVICE3_TASK_CORE 0   // Device 3 operations (same as UART)
//#define DEVICE4_TASK_CORE 1   // Device 4 network operations (same as Web)

// Input buffer sizes
#define INPUT_BUFFER_SIZE 4096  // 4KB for GCS→FC commands

// TX ring buffer for UART1
#define UART1_TX_RING_SIZE 8192  // 8KB for single-writer architecture

#endif // DEFINES_H