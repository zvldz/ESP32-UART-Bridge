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
extern BridgeMode bridgeMode;
extern Config config;

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
static volatile uint32_t device3TxCounter = 0;
static volatile uint32_t device3RxCounter = 0;

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

// Client mode blink state
static volatile bool clientBlinkActive = false;
static volatile unsigned long clientBlinkNextTime = 0;
static volatile bool clientBlinkState = false;
static volatile LedMode clientBlinkMode = LED_MODE_OFF;

// Safe mode blink state (separate from client blink to avoid conflicts)
static volatile bool safeModeBlinkActive = false;
static volatile unsigned long safeModeBlinkNextTime = 0;
static volatile bool safeModeBlinkState = false;

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
   log_msg(LOG_ERROR, "Failed to create LED mutex!");
   return;
 }

 // FastLED initialization
 FastLED.addLeds<WS2812B, LED_PIN1, GRB>(leds, NUM_LEDS);
 FastLED.setBrightness(LED_BRIGHTNESS);
 FastLED.setMaxPowerInVoltsAndMilliamps(5, 100); // Limit power consumption

 // Rainbow startup effect
 log_msg(LOG_DEBUG, "Starting rainbow effect...");
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
 log_msg(LOG_INFO, "WS2812 RGB LED initialized on GPIO%d (rainbow effect took %lums)", LED_PIN1, effectDuration);
}

// Removed first duplicate led_set_mode function - using implementation at end of file

// Notify functions for UART task
void led_notify_uart_rx() {
 uartRxCounter = uartRxCounter + 1;
}

void led_notify_usb_rx() {
 usbRxCounter = usbRxCounter + 1;
}

void led_notify_device3_tx() {
 device3TxCounter = device3TxCounter + 1;
}

void led_notify_device3_rx() {
 device3RxCounter = device3RxCounter + 1;
}

// Flash LED for data activity
static void led_flash_activity(ActivityType activity) {
 if (bridgeMode == BRIDGE_STANDALONE && ledMutex != NULL) {
   if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
     unsigned long now = millis();

     // Check if LED is still on from previous activity
     if (ledOffTime > now && ledIsOn && activity != lastActivity) {
       // Different activity while LED is on - mix colors
       // Check for mixed activities
       if ((lastActivity == ACTIVITY_UART_RX || lastActivity == ACTIVITY_USB_RX) &&
           (activity == ACTIVITY_DEVICE3_TX || activity == ACTIVITY_DEVICE3_RX)) {
         // Main channel + Device 3 = White
         pendingColorValue = (uint32_t)CRGB::White;
       } else if ((lastActivity == ACTIVITY_DEVICE3_TX || lastActivity == ACTIVITY_DEVICE3_RX) &&
                  (activity == ACTIVITY_UART_RX || activity == ACTIVITY_USB_RX)) {
         // Device 3 + Main channel = White
         pendingColorValue = (uint32_t)CRGB::White;
       } else if (lastActivity == ACTIVITY_UART_RX && activity == ACTIVITY_USB_RX) {
         pendingColorValue = (uint32_t)CRGB::Cyan;
       } else if (lastActivity == ACTIVITY_DEVICE3_TX && activity == ACTIVITY_DEVICE3_RX) {
         pendingColorValue = (uint32_t)CRGB::Orange;
       }
       lastActivity = ACTIVITY_BOTH;  // Generic "multiple activities"
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
         case ACTIVITY_DEVICE3_TX:
           pendingColorValue = COLOR_MAGENTA;  // Defined in defines.h
           break;
         case ACTIVITY_DEVICE3_RX:
           pendingColorValue = COLOR_YELLOW;   // Defined in defines.h
           break;
         case ACTIVITY_DEVICE3_BOTH:
           pendingColorValue = (uint32_t)CRGB::Orange;
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

// Visual indication of click count (works in standalone and WiFi client modes)
void led_blink_click_feedback(int clickCount) {
 // Show feedback only in modes where button clicks do something useful
 bool showFeedback = (bridgeMode == BRIDGE_STANDALONE) || 
                     (bridgeMode == BRIDGE_NET && config.wifi_mode == BRIDGE_WIFI_MODE_CLIENT);
                     
 if (showFeedback && clickCount > 0) {
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

 // Safe Mode blink processing (highest priority)
 if (safeModeBlinkActive && millis() >= safeModeBlinkNextTime) {
   if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
     if (safeModeBlinkState) {
       leds[0] = CRGB::Black;
       safeModeBlinkState = false;
       safeModeBlinkNextTime = millis() + 4500;  // Long pause (4.5 sec)
     } else {
       leds[0] = CRGB::Red;
       safeModeBlinkState = true;
       safeModeBlinkNextTime = millis() + 500;   // Short flash (0.5 sec)
     }
     FastLED.show();
     xSemaphoreGive(ledMutex);
   }
   return;  // Skip other updates during safe mode blink
 }

 // Button feedback has second priority - check next
 if (buttonBlinkActive && millis() >= buttonBlinkNextTime) {
   if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
     if (buttonBlinkState) {
       leds[0] = CRGB::Black;
       buttonBlinkState = false;
       buttonBlinkCount = buttonBlinkCount - 1;
       if (buttonBlinkCount <= 0) {
         buttonBlinkActive = false;
         // Restore main LED mode after button feedback ends  
         ledUpdateNeeded = true;  // Force restore of pendingColorValue
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
   return;  // Skip other updates during button feedback
 }

 // Process pending LED changes (after button feedback check)
 if (ledUpdateNeeded) {
   if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
     leds[0] = CRGB(pendingColorValue);
     FastLED.show();
     ledUpdateNeeded = false;
     xSemaphoreGive(ledMutex);
   }
 }

 // Handle client mode blinking FIRST (before network mode check)
 if ((currentLedMode == LED_MODE_WIFI_CLIENT_SEARCHING || 
      currentLedMode == LED_MODE_WIFI_CLIENT_ERROR) && 
     millis() >= clientBlinkNextTime) {
   
   int blinkInterval = (currentLedMode == LED_MODE_WIFI_CLIENT_SEARCHING) ? 
                       WIFI_CLIENT_BLINK_INTERVAL_MS : 
                       WIFI_CLIENT_ERROR_BLINK_MS;
   
   if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
     if (clientBlinkState) {
       leds[0] = CRGB::Black;
       clientBlinkState = false;
     } else {
       // Use orange for searching, red for error
       uint32_t blinkColor = (currentLedMode == LED_MODE_WIFI_CLIENT_SEARCHING) ? 
                             COLOR_ORANGE : COLOR_RED;
       leds[0] = CRGB(blinkColor);
       clientBlinkState = true;
       
       // Log LED activity for error mode
       if (currentLedMode == LED_MODE_WIFI_CLIENT_ERROR) {
         log_msg(LOG_DEBUG, "LED blinking RED (error mode)");
       }
     }
     FastLED.show();
     clientBlinkNextTime = millis() + blinkInterval;
     xSemaphoreGive(ledMutex);
   }
   return;  // Skip other updates during client blink
 }

 // Network mode - purple constant, only rapid blink can interrupt
 if (bridgeMode == BRIDGE_NET) {
   // Handle rapid blink if active
   if (rapidBlinkActive && millis() >= rapidBlinkNextTime) {
     if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
       if (rapidBlinkState) {
         leds[0] = CRGB::Black;
         rapidBlinkState = false;
         rapidBlinkCount = rapidBlinkCount - 1;
         if (rapidBlinkCount <= 0) {
           rapidBlinkActive = false;
           // Return to purple for network mode
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
   // Skip data activity in network mode
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


 // Standalone mode - check data activity
 static uint32_t lastUartCount = 0;
 static uint32_t lastUsbCount = 0;
 static uint32_t lastDevice3TxCount = 0;
 static uint32_t lastDevice3RxCount = 0;

 // Check activity
 bool uartActivity = (uartRxCounter != lastUartCount);
 bool usbActivity = (usbRxCounter != lastUsbCount);
 bool device3TxActivity = (device3TxCounter != lastDevice3TxCount);
 bool device3RxActivity = (device3RxCounter != lastDevice3RxCount);

 // Determine activity type
 ActivityType activity = ACTIVITY_NONE;
 
 // Check Device 3 activity first
 if (device3TxActivity && device3RxActivity) {
   activity = ACTIVITY_DEVICE3_BOTH;
 } else if (device3TxActivity) {
   activity = ACTIVITY_DEVICE3_TX;
 } else if (device3RxActivity) {
   activity = ACTIVITY_DEVICE3_RX;
 } 
 // Then check main channel activity
 else if (uartActivity && usbActivity) {
   activity = ACTIVITY_BOTH;
 } else if (uartActivity) {
   activity = ACTIVITY_UART_RX;
 } else if (usbActivity) {
   activity = ACTIVITY_USB_RX;
 }

 if (activity != ACTIVITY_NONE) {
   led_flash_activity(activity);

   lastUartCount = uartRxCounter;
   lastUsbCount = usbRxCounter;
   lastDevice3TxCount = device3TxCounter;
   lastDevice3RxCount = device3RxCounter;
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

// Set LED mode (implementation)
void led_set_mode(LedMode mode) {
  currentLedMode = mode;
  
  switch(mode) {
    case LED_MODE_OFF:
      if (ledMutex != NULL && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        leds[0] = CRGB::Black;
        FastLED.show();
        clientBlinkActive = false;
        safeModeBlinkActive = false;
        xSemaphoreGive(ledMutex);
      }
      break;
      
    case LED_MODE_WIFI_ON:
      if (ledMutex != NULL && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        leds[0] = CRGB::Purple;
        FastLED.show();
        clientBlinkActive = false;
        safeModeBlinkActive = false;
        xSemaphoreGive(ledMutex);
      }
      break;
      
    case LED_MODE_WIFI_CLIENT_CONNECTED:
      if (ledMutex != NULL && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        leds[0] = CRGB::Orange;
        FastLED.show();
        clientBlinkActive = false;
        safeModeBlinkActive = false;
        xSemaphoreGive(ledMutex);
      }
      break;
      
    case LED_MODE_WIFI_CLIENT_SEARCHING:
      if (ledMutex != NULL && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        clientBlinkActive = true;
        clientBlinkState = false;
        clientBlinkNextTime = millis();
        clientBlinkMode = mode;
        safeModeBlinkActive = false;
        log_msg(LOG_DEBUG, "LED set to WiFi Client Searching - orange slow blink");
        xSemaphoreGive(ledMutex);
      }
      break;
      
    case LED_MODE_WIFI_CLIENT_ERROR:
      if (ledMutex != NULL && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        clientBlinkActive = true;
        clientBlinkState = false;
        clientBlinkNextTime = millis();
        clientBlinkMode = mode;
        safeModeBlinkActive = false;
        log_msg(LOG_DEBUG, "LED set to WiFi Client Error - red fast blink");
        xSemaphoreGive(ledMutex);
      }
      break;
      
    case LED_MODE_SAFE_MODE:
      if (ledMutex != NULL && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        safeModeBlinkActive = true;
        safeModeBlinkState = false;
        safeModeBlinkNextTime = millis();
        clientBlinkActive = false;
        log_msg(LOG_DEBUG, "LED set to Safe Mode - red blink every 5s");
        xSemaphoreGive(ledMutex);
      }
      break;
      
    case LED_MODE_DATA_FLASH:
    default:
      // Data flash mode is handled by activity notifications
      if (ledMutex != NULL && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        clientBlinkActive = false;
        safeModeBlinkActive = false;
        xSemaphoreGive(ledMutex);
      }
      break;
  }
}