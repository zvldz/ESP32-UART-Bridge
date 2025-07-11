#ifndef LEDS_H
#define LEDS_H

#include "types.h"

// Activity type for LED color selection
enum ActivityType {
  ACTIVITY_NONE = 0,    // No activity
  ACTIVITY_UART_RX,     // Blue - data from device
  ACTIVITY_USB_RX,      // Green - data from computer
  ACTIVITY_BOTH         // Cyan - bidirectional data
};

// LED control interface
void leds_init();
void led_set_mode(LedMode mode);
void led_process_updates();
void led_notify_uart_rx();
void led_notify_usb_rx();
void led_rapid_blink(int count, int delay_ms);
void led_blink_click_feedback(int clickCount);

#endif // LEDS_H