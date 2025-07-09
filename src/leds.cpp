#include "leds.h"
#include "logging.h"
#include "defines.h"
#include <Arduino.h>
#include <FastLED.h>

// LED brightness configuration (0-255)
static const uint8_t LED_BRIGHTNESS = 25;        // Normal brightness (10%)
static const uint8_t LED_BRIGHTNESS_DIM = 25;    // Dimmed for effects (10%)
static const uint8_t LED_BRIGHTNESS_BRIGHT = 100; // Bright for alerts (40%)

// FastLED configuration
#define NUM_LEDS 1
static CRGB leds[NUM_LEDS];

// Mutex for thread-safe LED access
static SemaphoreHandle_t ledMutex = NULL;

// External LED state from main.cpp
extern LedState ledState;
extern DeviceMode currentMode;
extern const int DEBUG_MODE;

// Internal variables
static LedMode currentLedMode = LED_MODE_OFF;

// Helper function to set WS2812 LED state (thread-safe)
static void setLED(bool on) {
  if (ledMutex == NULL) return;
  
  if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    if (on) {
      leds[0] = CRGB::Blue;  // Blue color
    } else {
      leds[0] = CRGB::Black; // Off
    }
    FastLED.show();
    xSemaphoreGive(ledMutex);
  }
}

// Initialize LED
void leds_init() {
  // Create mutex first
  ledMutex = xSemaphoreCreateMutex();
  if (ledMutex == NULL) {
    log_msg("ERROR: Failed to create LED mutex!");
    return;
  }
  
  // FastLED initialization
  FastLED.addLeds<WS2812B, BLUE_LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 100); // Limit power consumption
  
  // Rainbow startup effect (1 second)
  log_msg("Starting rainbow effect...");
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
      delay(5);  // ~5ms per step = ~300ms per cycle = ~1 second total
    }
  }
  
  // Turn off LED after rainbow effect
  setLED(false);
  
  unsigned long effectDuration = millis() - startTime;
  log_msg("WS2812 RGB LED initialized on GPIO" + String(BLUE_LED_PIN) + 
          " (rainbow effect took " + String(effectDuration) + "ms)");
}

// Set LED mode
void led_set_mode(LedMode mode) {
  currentLedMode = mode;
  
  switch (mode) {
    case LED_MODE_OFF:
      setLED(false);  // LED OFF
      break;
      
    case LED_MODE_WIFI_ON:
      // Force LED ON for WiFi config mode
      setLED(true);   // LED ON
      log_msg("LED forced ON for WiFi config mode");
      break;
      
    case LED_MODE_DATA_FLASH:
      // Will be handled by led_update()
      break;
  }
}

// Flash LED for data activity (only in normal mode)
void led_flash_activity() {
  if (currentMode == MODE_NORMAL) {
    setLED(true);  // LED ON
    ledState.lastDataLedTime = millis();
    ledState.dataLedState = true;
    
    if (DEBUG_MODE == 1) {
      static unsigned long lastDataDebug = 0;
      if (millis() - lastDataDebug > 1000) {  // Debug every second to avoid spam
        log_msg("Data activity - LED flash");
        lastDataDebug = millis();
      }
    }
  }
}

// Update LED state - called from main loop
void led_update() {
  // Skip update in WiFi mode - LED already set once
  if (currentMode == MODE_CONFIG) {
    return;
  }
  
  // Turn off LED after data flash (only in normal mode)
  if (currentMode == MODE_NORMAL && ledState.dataLedState) {
    if (millis() - ledState.lastDataLedTime > 50) {  // 50ms flash duration
      setLED(false);  // LED OFF
      ledState.dataLedState = false;
    }
  }
}

// Visual indication of click count (only in normal mode)
void led_blink_click_feedback(int clickCount) {
  if (currentMode == MODE_NORMAL && clickCount > 0) {
    static unsigned long lastBlinkTime = 0;
    static bool blinkState = false;
    static int blinkCounter = 0;
    
    // Blink LED to show click count
    if (millis() - lastBlinkTime > 200) {
      if (blinkState) {
        // LED was on, turn it off
        setLED(false);
        blinkState = false;
        blinkCounter++;
        
        // If we've blinked enough times, add a longer pause
        if (blinkCounter >= clickCount) {
          lastBlinkTime = millis() + 400;  // Extra pause between sequences
          blinkCounter = 0;
        }
      } else {
        // LED was off, turn it on
        if (blinkCounter < clickCount) {
          setLED(true);
          blinkState = true;
        }
      }
      
      lastBlinkTime = millis();
      
      if (DEBUG_MODE == 1) {
        log_msg("LED blink: clickCount=" + String(clickCount) + 
                ", state=" + String(blinkState ? "ON" : "OFF") +
                ", blinkCounter=" + String(blinkCounter));
      }
    }
  }
}