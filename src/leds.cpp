#include "leds.h"
#include "logging.h"
#include "defines.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// LED brightness configuration (0-255)
static const uint8_t LED_BRIGHTNESS = 50;        // Normal brightness (20%)
static const uint8_t LED_BRIGHTNESS_DIM = 25;    // Dimmed for effects (10%)
static const uint8_t LED_BRIGHTNESS_BRIGHT = 100; // Bright for alerts (40%)

// WS2812 on ESP32-S3-Zero (GPIO21)
static Adafruit_NeoPixel pixel(1, BLUE_LED_PIN, NEO_GRB + NEO_KHZ800);

// External LED state from main.cpp
extern LedState ledState;
extern DeviceMode currentMode;
extern const int DEBUG_MODE;

// Internal variables
static LedMode currentLedMode = LED_MODE_OFF;

// Helper function to set WS2812 LED state
static void setLED(bool on) {
  if (on) {
    pixel.setPixelColor(0, 0, 0, 255);  // Blue color (R=0, G=0, B=255)
  } else {
    pixel.setPixelColor(0, 0, 0, 0);     // Off
  }
  pixel.show();
}

// Initialize LED
void leds_init() {
  // WS2812 initialization
  pixel.begin();
  pixel.setBrightness(LED_BRIGHTNESS);  // Use defined constant
  
  // Rainbow startup effect (1 second)
  log_msg("Starting rainbow effect...");
  unsigned long startTime = millis();
  
  // Complete 3 full rainbow cycles in 1 second
  for(int cycle = 0; cycle < 3; cycle++) {
    for(int i = 0; i < 360; i += 6) {  // 60 steps per cycle
      // Calculate RGB values using phase-shifted sine waves
      int r = (sin(i * 0.017453) * 127) + 128;          // 0° phase
      int g = (sin((i + 120) * 0.017453) * 127) + 128;  // 120° phase
      int b = (sin((i + 240) * 0.017453) * 127) + 128;  // 240° phase
      
      pixel.setPixelColor(0, r/5, g/5, b/5);  // Divided by 5 for dimmer effect
      pixel.show();
      delay(5);  // ~5ms per step = ~300ms per cycle = ~1 second total
    }
  }
  
  // Turn off LED after rainbow effect
  pixel.setPixelColor(0, 0, 0, 0);
  pixel.show();
  
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
  // Turn off LED after data flash (only in normal mode)
  if (currentMode == MODE_NORMAL && ledState.dataLedState) {
    if (millis() - ledState.lastDataLedTime > 50) {  // 50ms flash duration
      setLED(false);  // LED OFF
      ledState.dataLedState = false;
    }
  }
  
  // WiFi Config Mode - keep LED constantly ON
  if (currentMode == MODE_CONFIG) {
    static unsigned long lastConfigUpdate = 0;
    // Update every 100ms to ensure LED stays on (WS2812 might need refresh)
    if (millis() - lastConfigUpdate > 100) {
      setLED(true);  // LED ON
      lastConfigUpdate = millis();
    }
    
    if (DEBUG_MODE == 1) {
      static unsigned long lastWifiDebug = 0;
      if (millis() - lastWifiDebug > 10000) {
        log_msg("WiFi Config Mode - LED constantly ON");
        lastWifiDebug = millis();
      }
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