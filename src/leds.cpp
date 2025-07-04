#include "leds.h"
#include "logging.h"
#include "defines.h"
#include <Arduino.h>

// External LED state from main.cpp
extern LedState ledState;
extern DeviceMode currentMode;
extern const int DEBUG_MODE;

// Internal variables
static LedMode currentLedMode = LED_MODE_OFF;

// Initialize LED pins
void leds_init() {
  pinMode(BLUE_LED_PIN, OUTPUT);
  // Note: GPIO2 (red LED) not initialized - always on when ESP32 is powered
  
  // Initialize blue LED (OFF by default - for inverted logic use HIGH)
  digitalWrite(BLUE_LED_PIN, HIGH);  // HIGH = LED OFF (inverted logic)
  
  log_msg("Blue LED: GPIO8 (controllable, inverted logic: LOW=ON, HIGH=OFF)");
  log_msg("Red LED: GPIO2 (always on when powered)");
}

// Set LED mode
void led_set_mode(LedMode mode) {
  currentLedMode = mode;
  
  switch (mode) {
    case LED_MODE_OFF:
      digitalWrite(BLUE_LED_PIN, HIGH);  // HIGH = LED OFF
      break;
      
    case LED_MODE_WIFI_ON:
      // Force blue LED ON for WiFi config mode (inverted logic: LOW = ON)
      digitalWrite(BLUE_LED_PIN, LOW);  // LOW = LED ON
      log_msg("Blue LED forced ON for WiFi config mode (GPIO8 = LOW)");
      break;
      
    case LED_MODE_DATA_FLASH:
      // Will be handled by led_update()
      break;
  }
}

// Flash blue LED for data activity (only in normal mode, inverted logic)
void led_flash_activity() {
  if (currentMode == MODE_NORMAL) {
    digitalWrite(BLUE_LED_PIN, LOW);  // LOW = LED ON
    ledState.lastDataLedTime = millis();
    ledState.dataLedState = true;
    
    if (DEBUG_MODE == 1) {
      static unsigned long lastDataDebug = 0;
      if (millis() - lastDataDebug > 1000) {  // Debug every second to avoid spam
        log_msg("Data activity - Blue LED flash (GPIO8 = LOW)");
        lastDataDebug = millis();
      }
    }
  }
}

// Update LED state - called from main loop
void led_update() {
  // Turn off blue LED after data flash (only in normal mode, inverted logic)
  if (currentMode == MODE_NORMAL && ledState.dataLedState) {
    if (millis() - ledState.lastDataLedTime > 50) {  // 50ms flash duration
      digitalWrite(BLUE_LED_PIN, HIGH);  // HIGH = LED OFF
      ledState.dataLedState = false;
    }
  }
  
  // WiFi Config Mode - keep blue LED constantly ON (inverted logic: LOW = ON)
  if (currentMode == MODE_CONFIG) {
    digitalWrite(BLUE_LED_PIN, LOW);  // LOW = LED ON
    
    if (DEBUG_MODE == 1) {
      static unsigned long lastWifiDebug = 0;
      if (millis() - lastWifiDebug > 10000) {
        log_msg("WiFi Config Mode - Blue LED constantly ON (GPIO8 = LOW)");
        lastWifiDebug = millis();
      }
    }
  }
}

// Visual indication of click count (only in normal mode, don't interfere with WiFi mode)
void led_blink_click_feedback(int clickCount) {
  if (currentMode == MODE_NORMAL && clickCount > 0) {
    static unsigned long lastBlinkTime = 0;
    static bool blinkState = false;
    
    // Blink blue LED to show click count (inverted logic)
    if (millis() - lastBlinkTime > 200) {
      blinkState = !blinkState;
      digitalWrite(BLUE_LED_PIN, blinkState ? LOW : HIGH);  // LOW = ON, HIGH = OFF
      lastBlinkTime = millis();
      
      if (DEBUG_MODE == 1) {
        log_msg("LED blink: clickCount=" + String(clickCount) + 
                ", state=" + String(blinkState ? "ON" : "OFF"));
      }
    }
  }
}