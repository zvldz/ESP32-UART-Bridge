#include "leds.h"
#include "logging.h"
#include "defines.h"
#include <Arduino.h>
#include <FastLED.h>

// LED brightness configuration (0-255)
static const uint8_t LED_BRIGHTNESS = 25;        // Normal brightness (10%)

// FastLED configuration
#define NUM_LEDS 1
static CRGB leds[NUM_LEDS];

// Mutex for thread-safe LED access
static SemaphoreHandle_t ledMutex = NULL;

// External objects from main.cpp
extern DeviceMode currentMode;

// Internal variables
static LedMode currentLedMode = LED_MODE_OFF;

// LED state tracking
static volatile bool ledUpdateNeeded = false;
static volatile uint32_t pendingColorValue = 0;  // Store as uint32_t instead of CRGB
static volatile unsigned long ledOffTime = 0;
static volatile bool ledIsOn = false;
static volatile ActivityType lastActivity = ACTIVITY_NONE;

// Activity counters
static volatile uint32_t uartRxCounter = 0;
static volatile uint32_t usbRxCounter = 0;

// Rapid blink state
static volatile bool rapidBlinkActive = false;
static volatile int rapidBlinkCount = 0;
static volatile int rapidBlinkDelay = 0;
static volatile unsigned long rapidBlinkNextTime = 0;
static volatile bool rapidBlinkState = false;
static volatile uint32_t rapidBlinkColorValue = CRGB::Purple;  // Store as uint32_t

// Button feedback state
static volatile bool buttonBlinkActive = false;
static volatile int buttonBlinkCount = 0;
static volatile unsigned long buttonBlinkNextTime = 0;
static volatile bool buttonBlinkState = false;

// Helper function to set LED state (thread-safe)
static void setLED(bool on) {
 if (ledMutex == NULL) return;

 if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
   pendingColorValue = on ? (uint32_t)CRGB::Blue : (uint32_t)CRGB::Black;
   ledUpdateNeeded = true;
   xSemaphoreGive(ledMutex);
 }
}

// Initialize LED
void leds_init() {
 // Create mutex first
 ledMutex = xSemaphoreCreateMutex();
 if (ledMutex == NULL) {
   log_msg("Failed to create LED mutex!", LOG_ERROR);
   return;
 }

 // FastLED initialization
 FastLED.addLeds<WS2812B, LED_PIN1, GRB>(leds, NUM_LEDS);
 FastLED.setBrightness(LED_BRIGHTNESS);
 FastLED.setMaxPowerInVoltsAndMilliamps(5, 100); // Limit power consumption

 // Rainbow startup effect
 log_msg("Starting rainbow effect...", LOG_DEBUG);
 unsigned long startTime = millis();

 // Complete 3 full rainbow cycles in 1 second
 for(int cycle = 0; cycle < 3; cycle++) {
   for(int i = 0; i < 360; i += 6) {  // 60 steps per cycle
     if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
       // Use FastLED's HSV for smooth rainbow
       leds[0] = CHSV(map(i, 0, 360, 0, 255), 255, 255);
       FastLED.show();
       xSemaphoreGive(ledMutex);
     }
     vTaskDelay(pdMS_TO_TICKS(5));  // ~5ms per step = ~300ms per cycle = ~1 second total
   }
 }

 // Turn off LED after rainbow effect
 if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
   leds[0] = CRGB::Black;
   FastLED.show();
   xSemaphoreGive(ledMutex);
 }

 unsigned long effectDuration = millis() - startTime;
 log_msg("WS2812 RGB LED initialized on GPIO" + String(LED_PIN1) +
         " (rainbow effect took " + String(effectDuration) + "ms)", LOG_INFO);
}

// Set LED mode
void led_set_mode(LedMode mode) {
 currentLedMode = mode;

 switch (mode) {
   case LED_MODE_OFF:
     if (ledMutex != NULL && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
       pendingColorValue = (uint32_t)CRGB::Black;
       ledUpdateNeeded = true;
       xSemaphoreGive(ledMutex);
     }
     break;

   case LED_MODE_WIFI_ON:
     // Force LED ON for WiFi config mode - purple
     if (ledMutex != NULL && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
       pendingColorValue = (uint32_t)CRGB::Purple;
       ledUpdateNeeded = true;
       xSemaphoreGive(ledMutex);
     }
     log_msg("LED forced ON (purple) for WiFi config mode", LOG_DEBUG);
     break;

   case LED_MODE_DATA_FLASH:
     // Will be handled by activity notifications
     break;
 }
}

// Notify functions for UART task
void led_notify_uart_rx() {
 uartRxCounter = uartRxCounter + 1;
}

void led_notify_usb_rx() {
 usbRxCounter = usbRxCounter + 1;
}

// Flash LED for data activity
static void led_flash_activity(ActivityType activity) {
 if (currentMode == MODE_NORMAL && ledMutex != NULL) {
   if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
     unsigned long now = millis();

     // Check if LED is still on from previous activity
     if (ledOffTime > now && ledIsOn && activity != lastActivity) {
       // Different activity while LED is on - mix colors
       pendingColorValue = (uint32_t)CRGB::Cyan;
       lastActivity = ACTIVITY_BOTH;
     } else {
       // Single activity or LED is off
       switch(activity) {
         case ACTIVITY_UART_RX:
           pendingColorValue = (uint32_t)CRGB::Blue;
           break;
         case ACTIVITY_USB_RX:
           pendingColorValue = (uint32_t)CRGB::Green;
           break;
         case ACTIVITY_BOTH:
           pendingColorValue = (uint32_t)CRGB::Cyan;
           break;
         default:
           pendingColorValue = (uint32_t)CRGB::Black;
           break;
       }
       lastActivity = activity;
     }

     ledUpdateNeeded = true;
     ledIsOn = true;
     ledOffTime = now + LED_DATA_FLASH_MS;

     xSemaphoreGive(ledMutex);
   }
 }
}

// Rapid blink function
void led_rapid_blink(int count, int delay_ms) {
 if (ledMutex != NULL && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
   rapidBlinkCount = count;
   rapidBlinkDelay = delay_ms;
   rapidBlinkActive = true;
   rapidBlinkState = false;
   rapidBlinkNextTime = millis();
   rapidBlinkColorValue = (uint32_t)CRGB::Purple;  // Purple for WiFi reset
   xSemaphoreGive(ledMutex);
 }
}

// Visual indication of click count
void led_blink_click_feedback(int clickCount) {
 if (currentMode == MODE_NORMAL && clickCount > 0) {
   if (ledMutex != NULL && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
     buttonBlinkCount = clickCount;
     buttonBlinkActive = true;
     buttonBlinkState = false;
     buttonBlinkNextTime = millis();
     xSemaphoreGive(ledMutex);
   }
 }
}

// Process LED updates from main loop
void led_process_updates() {
 // Skip all if not initialized
 if (ledMutex == NULL) return;

 // Process pending LED changes first (before mode checks)
 if (ledUpdateNeeded) {
   if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
     leds[0] = CRGB(pendingColorValue);
     FastLED.show();
     ledUpdateNeeded = false;
     xSemaphoreGive(ledMutex);
   }
 }

 // WiFi mode - purple constant, only rapid blink can interrupt
 if (currentMode == MODE_CONFIG) {
   // Handle rapid blink if active
   if (rapidBlinkActive && millis() >= rapidBlinkNextTime) {
     if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
       if (rapidBlinkState) {
         leds[0] = CRGB::Black;
         rapidBlinkState = false;
         rapidBlinkCount = rapidBlinkCount - 1;
         if (rapidBlinkCount <= 0) {
           rapidBlinkActive = false;
           // Return to purple for WiFi mode
           leds[0] = CRGB::Purple;
         }
       } else {
         leds[0] = CRGB(rapidBlinkColorValue);
         rapidBlinkState = true;
       }
       FastLED.show();
       rapidBlinkNextTime = millis() + rapidBlinkDelay;
       xSemaphoreGive(ledMutex);
     }
   }
   // Skip data activity in WiFi mode
   return;
 }

 // Handle rapid blink
 if (rapidBlinkActive && millis() >= rapidBlinkNextTime) {
   if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
     if (rapidBlinkState) {
       leds[0] = CRGB::Black;
       rapidBlinkState = false;
       rapidBlinkCount = rapidBlinkCount - 1;
       if (rapidBlinkCount <= 0) {
         rapidBlinkActive = false;
       }
     } else {
       leds[0] = CRGB(rapidBlinkColorValue);
       rapidBlinkState = true;
     }
     FastLED.show();
     rapidBlinkNextTime = millis() + rapidBlinkDelay;
     xSemaphoreGive(ledMutex);
   }
   return;  // Skip other updates during rapid blink
 }

 // Handle button feedback blink
 if (buttonBlinkActive && millis() >= buttonBlinkNextTime) {
   if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
     if (buttonBlinkState) {
       leds[0] = CRGB::Black;
       buttonBlinkState = false;
       buttonBlinkCount = buttonBlinkCount - 1;
       if (buttonBlinkCount <= 0) {
         buttonBlinkActive = false;
       }
       buttonBlinkNextTime = millis() + LED_BUTTON_FLASH_MS * 2;  // Longer pause between blinks
     } else {
       leds[0] = CRGB::White;  // White for button feedback
       buttonBlinkState = true;
       buttonBlinkNextTime = millis() + LED_BUTTON_FLASH_MS;
     }
     FastLED.show();
     xSemaphoreGive(ledMutex);
   }
   return;  // Skip data activity during button feedback
 }

 // Normal mode - check data activity
 static uint32_t lastUartCount = 0;
 static uint32_t lastUsbCount = 0;

 // Check activity
 bool uartActivity = (uartRxCounter != lastUartCount);
 bool usbActivity = (usbRxCounter != lastUsbCount);

 if (uartActivity || usbActivity) {
   ActivityType activity;
   if (uartActivity && usbActivity) {
     activity = ACTIVITY_BOTH;
   } else if (uartActivity) {
     activity = ACTIVITY_UART_RX;
   } else {
     activity = ACTIVITY_USB_RX;
   }

   led_flash_activity(activity);

   lastUartCount = uartRxCounter;
   lastUsbCount = usbRxCounter;
 }

 // Handle automatic LED off
 if (ledOffTime > 0 && millis() >= ledOffTime) {
   if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
     leds[0] = CRGB::Black;
     FastLED.show();
     ledOffTime = 0;
     ledIsOn = false;
     lastActivity = ACTIVITY_NONE;
     xSemaphoreGive(ledMutex);
   }
 }
}