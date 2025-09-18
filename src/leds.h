#ifndef LEDS_H
#define LEDS_H

#include "types.h"

// LED timing constants
#define LED_DATA_FLASH_MS      50    // Data activity flash duration
#define LED_BUTTON_FLASH_MS    100   // Button click feedback duration
#define LED_WIFI_RESET_BLINK_MS 100  // WiFi reset rapid blink interval
#define LED_WIFI_SEARCH_BLINK_MS 2000 // Orange slow blink for WiFi searching
#define LED_WIFI_ERROR_BLINK_MS  500  // Fast blink for WiFi errors

// LED colors for Device 3
#define COLOR_MAGENTA   0xFF00FF  // Device 3 TX
#define COLOR_YELLOW    0xFFFF00  // Device 3 RX (Bridge mode)

// LED colors - WiFi Client mode
#define COLOR_ORANGE    0xFF8000  // Client mode connected
#define COLOR_RED       0xFF0000  // Client mode error

// Activity type for LED color selection
enum ActivityType {
  ACTIVITY_NONE = 0,      // No activity
  ACTIVITY_UART_RX,       // Blue - data from device
  ACTIVITY_USB_RX,        // Green - data from computer
  ACTIVITY_BOTH,          // Cyan - bidirectional data
  ACTIVITY_DEVICE3_TX,    // Magenta - Device 3 TX
  ACTIVITY_DEVICE3_RX,    // Yellow - Device 3 RX
  ACTIVITY_DEVICE3_BOTH   // Orange - Device 3 bidirectional
};

// LED control interface
void leds_init();
void led_set_mode(LedMode mode);
void led_process_updates();
void led_notify_uart_rx();
void led_notify_usb_rx();
void led_notify_device3_tx();
void led_notify_device3_rx();
void led_rapid_blink(int count, int delay_ms);
void led_blink_click_feedback(int clickCount);

#endif // LEDS_H