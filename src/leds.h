#ifndef LEDS_H
#define LEDS_H

#include "types.h"

// LED control interface
void leds_init();
void led_set_mode(LedMode mode);
void led_flash_activity();
void led_update();  // Called from main loop()
void led_blink_click_feedback(int clickCount);

#endif // LEDS_H