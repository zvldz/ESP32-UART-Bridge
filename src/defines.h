#ifndef DEFINES_H
#define DEFINES_H

/*
===============================================================================
                          ESP32 UART Bridge
===============================================================================

Universal UART to USB bridge with web configuration interface.
Optimized for drone autopilots (ArduPilot, PX4) but works with any UART protocol.

Hardware: ESP32-S3-Zero / ESP32-S3 Super Mini / XIAO ESP32-S3
- GPIO0: BOOT button (triple-click for WiFi config)
- Device 1 - Main UART:
  - Zero/SuperMini: GPIO4/5 (RX/TX)
  - XIAO: GPIO4/5 (D3/D4 pins)
- RTS/CTS flow control:
  - Zero/SuperMini: GPIO6/7
  - XIAO: GPIO1/2 (D0/D1 pins)
- Device 2 - Secondary UART:
  - Zero/SuperMini: GPIO8/9 (RX/TX)
  - XIAO: GPIO8/9 (D8/D9 pins)
- Device 3 - Logger/Mirror/Bridge UART:
  - Zero/SuperMini: GPIO11/12 (RX/TX)
  - XIAO: GPIO43/44 (D6/D7 pins)
- LED:
  - S3-Zero: GPIO21 (WS2812 RGB)
  - Super Mini: GPIO48 (WS2815 RGB)
  - XIAO: GPIO21 (single-color, inverted)

Board differences:
- S3 Zero: USB Host support, WS2812 LED on GPIO21
- S3 Super Mini: No USB Host, WS2815 LED on GPIO48
- XIAO ESP32-S3: USB Host support, single-color LED on GPIO21, compact pinout

===============================================================================

*/

// Ensure multi-core ESP32 only
#if CONFIG_FREERTOS_UNICORE == 1
    #error "This firmware requires a multi-core ESP32. Single-core variants are not supported."
#endif

// Device identification
#define DEVICE_NAME "ESP32 UART Bridge"
#define DEVICE_VERSION "2.18.10"

// Hardware pins - Device 1 (Main UART)
#define BOOT_BUTTON_PIN     0
#define UART_RX_PIN         4   // Zero/SuperMini: GPIO4, XIAO: GPIO4 (D3)
#define UART_TX_PIN         5   // Zero/SuperMini: GPIO5, XIAO: GPIO5 (D4)
// Board type verification at compile time
#if defined(BOARD_ESP32_S3_SUPER_MINI) && defined(BOARD_ESP32_S3_ZERO)
  #error "Both BOARD_ESP32_S3_SUPER_MINI and BOARD_ESP32_S3_ZERO are defined - this should not happen"
#endif

// LED pin varies by board
#if defined(BOARD_ESP32_S3_SUPER_MINI)
  #define LED_PIN1          48  // WS2815 RGB LED on GPIO48 for Super Mini
  #define BOARD_TYPE_STRING "ESP32-S3 Super Mini"
#elif defined(BOARD_XIAO_ESP32_S3)
  #define LED_PIN1          21  // Single color LED on GPIO21 for XIAO (inverted: LOW=ON)
  #define BOARD_TYPE_STRING "XIAO ESP32-S3"
  #define LED_TYPE_SINGLE_COLOR  // Blink-only mode (no RGB colors)
#elif defined(BOARD_ESP32_S3_ZERO)
  #define LED_PIN1          21  // WS2812 RGB LED on GPIO21 for S3-Zero
  #define BOARD_TYPE_STRING "ESP32-S3-Zero"
#else
  #define LED_PIN1          21  // WS2812 RGB LED on GPIO21 for S3-Zero (default fallback)
  #define BOARD_TYPE_STRING "ESP32-S3-Zero (default)"
  #warning "Board type not specified, defaulting to ESP32-S3-Zero configuration"
#endif
// RTS/CTS pins vary by board
#if defined(BOARD_XIAO_ESP32_S3)
  #define RTS_PIN             1   // XIAO: GPIO1 (D0) - flow control
  #define CTS_PIN             2   // XIAO: GPIO2 (D1) - flow control
#else
  #define RTS_PIN             6   // Zero/SuperMini: GPIO6 - flow control
  #define CTS_PIN             7   // Zero/SuperMini: GPIO7 - flow control
#endif

// Device 2 - Secondary UART pins
#define DEVICE2_UART_RX_PIN 8   // Zero/SuperMini: GPIO8, XIAO: GPIO8 (D8)
#define DEVICE2_UART_TX_PIN 9   // Zero/SuperMini: GPIO9, XIAO: GPIO9 (D9)

// Device 3 - Logger/Mirror/Bridge pins
#if defined(BOARD_XIAO_ESP32_S3)
  #define DEVICE3_UART_RX_PIN 44  // XIAO: GPIO44 (D7) - UART RX
  #define DEVICE3_UART_TX_PIN 43  // XIAO: GPIO43 (D6) - UART TX
#else
  #define DEVICE3_UART_RX_PIN 11  // Zero/SuperMini: GPIO11 - used only in Bridge mode
  #define DEVICE3_UART_TX_PIN 12  // Zero/SuperMini: GPIO12 - used in all modes
#endif

// WiFi settings
#define WIFI_TIMEOUT            1200000 // 20 minutes in milliseconds
#define WIFI_ACTIVATION_CLICKS  3
#define CLICK_TIMEOUT           3000    // 3 seconds between clicks

// Default WiFi AP credentials
#define DEFAULT_AP_SSID     "ESP-Bridge"
#define DEFAULT_AP_PASSWORD "12345678"

// Logging system
#define LOG_BUFFER_SIZE     100
#define LOG_DISPLAY_COUNT   95

// Crash logging (need move to crashlog)
#define CRASHLOG_MAX_ENTRIES        10                  // Maximum number of crash entries to keep
#define CRASHLOG_FILE_PATH          "/crashlog.json"    // Path to crash log file
#define CRASHLOG_MIN_HEAP_WARNING   15000               // Show warning if heap < 15KB
#define CRASHLOG_UPDATE_INTERVAL_MS 5000                // How often to update RTC variables
#define CRASHLOG_MAX_FILE_SIZE      4096                // Maximum crash log file size

// FreeRTOS priorities for multi-core ESP32
#define UART_TASK_PRIORITY  (configMAX_PRIORITIES - 4)  // Highest priority for UART !!
#define WEB_TASK_PRIORITY   (configMAX_PRIORITIES - 15) // Lower priority for web server

// Core assignments for multi-core ESP32
#define UART_TASK_CORE      0   // Main UART bridge task
#define UART_DMA_TASK_CORE  0   // UART DMA task (same as UART)
#define WEB_TASK_CORE       1   // Web server task

// Input buffer sizes
#define INPUT_BUFFER_SIZE 4096  // 4KB for GCS→FC commands

// TX ring buffer for UART1
#define UART1_TX_RING_SIZE 8192 // 8KB for single-writer architecture

#endif // DEFINES_H