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

// Combined mode tracking
static LedMode currentWifiMode = LED_MODE_OFF;  // Base WiFi state
#if defined(BLE_ENABLED)
static bool bleIsActive = false;                 // BLE active/connected
#endif

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

// Blink patterns enum
enum BlinkType {
    BLINK_RAPID = 0,
    BLINK_BUTTON,
    BLINK_CLIENT_SEARCH,
    BLINK_CLIENT_ERROR,
    BLINK_SAFE_MODE,
    BLINK_BT_CONNECTED,
    BLINK_BLE_ONLY,         // Fast blink for BLE only (single-color)
    BLINK_MAX
};

// Universal blink state structure
struct BlinkState {
    bool active;
    int count;              // -1 for infinite
    int onTime;             // milliseconds
    int offTime;            // milliseconds (gap between blinks for multi-blink)
    int pauseTime;          // milliseconds (pause after pattern, 0 for simple blinks)
    int blinksInPattern;    // 1 for simple, 2 for double, 3 for triple
    int currentBlink;       // current blink in pattern (0 to blinksInPattern-1)
    unsigned long nextTime;
    bool isOn;
    uint32_t colorValue;    // Store as uint32_t for thread safety
};

// RGB fade state for combined WiFi+BLE modes
#if !defined(LED_TYPE_SINGLE_COLOR)
struct FadeState {
    bool active;
    CRGB color1;           // First color (WiFi color)
    CRGB color2;           // Second color (BLE color = Purple)
    uint8_t position;      // 0-255 blend position
    int8_t direction;      // +1 or -1
    unsigned long nextStep;
};
static FadeState fadeState = {false, CRGB::Black, CRGB::Black, 0, 1, 0};  // Not volatile - accessed under mutex

// Fade timing
#define FADE_STEP_MS 20      // Time between fade steps (50 fps)
#define FADE_STEP_SIZE 5     // Position increment per step (full cycle ~2 seconds)
#endif

// Consolidated blink states - entire array is volatile
// Fields: active, count, onTime, offTime, pauseTime, blinksInPattern, currentBlink, nextTime, isOn, colorValue
static volatile BlinkState blinkStates[BLINK_MAX] = {
    // BLINK_RAPID - simple blink
    {false, 0, 0, 0, 0, 1, 0, 0, false, (uint32_t)CRGB::Purple},
    // BLINK_BUTTON - simple blink
    {false, 0, 0, 0, 0, 1, 0, 0, false, (uint32_t)CRGB::White},
    // BLINK_CLIENT_SEARCH - simple blink
    {false, -1, 0, 0, 0, 1, 0, 0, false, (uint32_t)CRGB::Orange},
    // BLINK_CLIENT_ERROR - simple blink
    {false, -1, 0, 0, 0, 1, 0, 0, false, (uint32_t)CRGB::Red},
    // BLINK_SAFE_MODE - simple blink
    {false, -1, 500, 4500, 0, 1, 0, 0, false, (uint32_t)CRGB::Red},
    // BLINK_BT_CONNECTED - simple blink (MiniKit only)
    {false, -1, LED_BT_CONNECTED_BLINK_MS, LED_BT_CONNECTED_BLINK_MS, 0, 1, 0, 0, false, (uint32_t)CRGB::Blue},
    // BLINK_BLE_ONLY - fast blink for BLE only (single-color)
    {false, -1, LED_BLE_FAST_BLINK_MS, LED_BLE_FAST_BLINK_MS, 0, 1, 0, 0, false, (uint32_t)CRGB::Purple}
};

// Safe millis() comparison that handles wrap-around correctly
static inline bool time_reached(unsigned long now, unsigned long deadline) {
    return (long)(now - deadline) >= 0;
}

// Universal blink processor - assumes mutex already taken!
// Supports simple blinks (blinksInPattern=1) and multi-blinks (double=2, triple=3)
static bool processBlinkPatternUnderMutex(BlinkType type) {
    volatile BlinkState& blink = blinkStates[type];

    if (!blink.active) return false;

    unsigned long now = millis();
    if (time_reached(now, blink.nextTime)) {
        // NO mutex take here - already held by caller!
        if (blink.isOn) {
            // Turn off
#if defined(LED_TYPE_SINGLE_COLOR)
  #if defined(LED_ACTIVE_HIGH)
            digitalWrite(LED_PIN1, LOW);   // OFF (normal logic)
  #else
            digitalWrite(LED_PIN1, HIGH);  // OFF (inverted)
  #endif
#else
            leds[0] = CRGB::Black;
            FastLED.show();
#endif
            blink.isOn = false;

            // Multi-blink pattern handling
            if (blink.blinksInPattern > 1) {
                // More blinks in this pattern?
                if (blink.currentBlink < blink.blinksInPattern - 1) {
                    // Short gap before next blink
                    blink.nextTime = now + blink.offTime;
                    blink.currentBlink = blink.currentBlink + 1;
                } else {
                    // Last blink done, long pause before pattern repeats
                    blink.nextTime = now + blink.pauseTime;
                    blink.currentBlink = 0;

                    // Handle count for multi-blink (count = pattern repetitions)
                    if (blink.count > 0) {
                        blink.count = blink.count - 1;
                        if (blink.count == 0) {
                            blink.active = false;
                        }
                    }
                }
            } else {
                // Simple blink - original behavior
                blink.nextTime = now + blink.offTime;

                // Handle count
                if (blink.count > 0) {
                    blink.count = blink.count - 1;
                    if (blink.count == 0) {
                        blink.active = false;
                    }
                }
            }
        } else {
            // Turn on with stored color
#if defined(LED_TYPE_SINGLE_COLOR)
  #if defined(LED_ACTIVE_HIGH)
            digitalWrite(LED_PIN1, HIGH);  // ON (normal logic)
  #else
            digitalWrite(LED_PIN1, LOW);   // ON (inverted)
  #endif
#else
            leds[0] = CRGB(blink.colorValue);
            FastLED.show();
#endif
            blink.isOn = true;
            blink.nextTime = now + blink.onTime;
        }
    }

    return blink.active;  // Still processing
}

// Process data activity under mutex (does NOT take its own mutex)
static void processDataActivityUnderMutex() {
    // Static counters for activity tracking
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
        // Flash activity - LED handling under current mutex
        unsigned long now = millis();

        // Color mixing logic for overlapping activities
        uint32_t pendingColor = 0;

        if (ledOffTime > now && ledIsOn && activity != lastActivity) {
            // Different activity while LED is on - mix colors
            if ((lastActivity == ACTIVITY_UART_RX || lastActivity == ACTIVITY_USB_RX) &&
                (activity == ACTIVITY_DEVICE3_TX || activity == ACTIVITY_DEVICE3_RX)) {
                // Main channel + Device 3 = White
                pendingColor = (uint32_t)CRGB::White;
            } else if ((lastActivity == ACTIVITY_DEVICE3_TX || lastActivity == ACTIVITY_DEVICE3_RX) &&
                       (activity == ACTIVITY_UART_RX || activity == ACTIVITY_USB_RX)) {
                // Device 3 + Main channel = White
                pendingColor = (uint32_t)CRGB::White;
            } else if (lastActivity == ACTIVITY_UART_RX && activity == ACTIVITY_USB_RX) {
                pendingColor = (uint32_t)CRGB::Cyan;
            } else if (lastActivity == ACTIVITY_USB_RX && activity == ACTIVITY_UART_RX) {
                pendingColor = (uint32_t)CRGB::Cyan;
            } else if (lastActivity == ACTIVITY_DEVICE3_TX && activity == ACTIVITY_DEVICE3_RX) {
                pendingColor = (uint32_t)CRGB::Orange;
            }
            lastActivity = ACTIVITY_BOTH;  // Generic "multiple activities"
        } else {
            // Single activity or LED is off
            switch(activity) {
                case ACTIVITY_UART_RX:
                    pendingColor = (uint32_t)CRGB::Blue;
                    break;
                case ACTIVITY_USB_RX:
                    pendingColor = (uint32_t)CRGB::Green;
                    break;
                case ACTIVITY_BOTH:
                    pendingColor = (uint32_t)CRGB::Cyan;
                    break;
                case ACTIVITY_DEVICE3_TX:
                    pendingColor = COLOR_MAGENTA;  // Defined in defines.h
                    break;
                case ACTIVITY_DEVICE3_RX:
                    pendingColor = COLOR_YELLOW;   // Defined in defines.h
                    break;
                case ACTIVITY_DEVICE3_BOTH:
                    pendingColor = (uint32_t)CRGB::Orange;
                    break;
                default:
                    pendingColor = (uint32_t)CRGB::Black;
                    break;
            }
            lastActivity = activity;
        }

        // Set LED color and schedule auto-off
#if defined(LED_TYPE_SINGLE_COLOR)
  #if defined(LED_ACTIVE_HIGH)
        digitalWrite(LED_PIN1, HIGH);  // ON (normal logic)
  #else
        digitalWrite(LED_PIN1, LOW);   // ON (inverted)
  #endif
#else
        leds[0] = CRGB(pendingColor);
        FastLED.show();
#endif
        ledIsOn = true;
        ledOffTime = now + LED_DATA_FLASH_MS;
    }

    // Handle automatic LED off
    if (ledOffTime > 0 && time_reached(millis(), ledOffTime)) {
#if defined(LED_TYPE_SINGLE_COLOR)
  #if defined(LED_ACTIVE_HIGH)
        digitalWrite(LED_PIN1, LOW);   // OFF (normal logic)
  #else
        digitalWrite(LED_PIN1, HIGH);  // OFF (inverted)
  #endif
#else
        leds[0] = CRGB::Black;
        FastLED.show();
#endif
        ledOffTime = 0;
        ledIsOn = false;
        lastActivity = ACTIVITY_NONE;
    }

    // Update counters once at the end if there was any activity
    if (activity != ACTIVITY_NONE) {
        lastUartCount = uartRxCounter;
        lastUsbCount = usbRxCounter;
        lastDevice3TxCount = device3TxCounter;
        lastDevice3RxCount = device3RxCounter;
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

#if defined(LED_TYPE_SINGLE_COLOR)
    // Single color GPIO LED
    pinMode(LED_PIN1, OUTPUT);
  #if defined(LED_ACTIVE_HIGH)
    // MiniKit: Normal logic (HIGH=ON, LOW=OFF)
    digitalWrite(LED_PIN1, LOW);   // OFF initially

    log_msg(LOG_DEBUG, "Starting LED test...");
    for(int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN1, HIGH);  // ON
        vTaskDelay(pdMS_TO_TICKS(100));
        digitalWrite(LED_PIN1, LOW);   // OFF
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    log_msg(LOG_INFO, "Single color LED initialized on GPIO%d (normal)", LED_PIN1);
  #else
    // XIAO: Inverted logic (LOW=ON, HIGH=OFF)
    digitalWrite(LED_PIN1, HIGH);  // OFF initially

    log_msg(LOG_DEBUG, "Starting LED test...");
    for(int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN1, LOW);   // ON
        vTaskDelay(pdMS_TO_TICKS(100));
        digitalWrite(LED_PIN1, HIGH);  // OFF
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    log_msg(LOG_INFO, "Single color LED initialized on GPIO%d (inverted)", LED_PIN1);
  #endif
#else
    // Zero/SuperMini: FastLED for RGB LED
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
#endif
}

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

// Forward declaration (defined below)
static void clearAllBlinks();

// Process fade animation (RGB only, call under mutex)
#if !defined(LED_TYPE_SINGLE_COLOR)
static bool processFadeUnderMutex() {
    if (!fadeState.active) return false;

    unsigned long now = millis();
    if (!time_reached(now, fadeState.nextStep)) return true;

    // Update position
    int16_t newPos = fadeState.position + (fadeState.direction * FADE_STEP_SIZE);

    // Reverse direction at boundaries
    if (newPos >= 255) {
        newPos = 255;
        fadeState.direction = -1;
    } else if (newPos <= 0) {
        newPos = 0;
        fadeState.direction = 1;
    }
    fadeState.position = (uint8_t)newPos;

    // Blend colors and display
    CRGB blended = blend(fadeState.color1, fadeState.color2, fadeState.position);
    leds[0] = blended;
    FastLED.show();

    fadeState.nextStep = now + FADE_STEP_MS;
    return true;
}

// Start fade animation (RGB only)
static void startFade(CRGB color1, CRGB color2) {
    if (ledMutex && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        clearAllBlinks();
        fadeState.active = true;
        fadeState.color1 = color1;
        fadeState.color2 = color2;
        fadeState.position = 0;
        fadeState.direction = 1;
        fadeState.nextStep = millis();
        xSemaphoreGive(ledMutex);
    }
}
#endif

// Clear all blink states (call under mutex)
static void clearAllBlinks() {
    for (int i = 0; i < BLINK_MAX; i++) {
        blinkStates[i].active = false;
        blinkStates[i].isOn = false;
    }
#if !defined(LED_TYPE_SINGLE_COLOR)
    fadeState.active = false;
#endif
}

// Clear other blinks except specified type (call under mutex) 
static void clearOtherBlinks(BlinkType keepActive) {
    for (int i = 0; i < BLINK_MAX; i++) {
        if (i != keepActive) {
            blinkStates[i].active = false;
            blinkStates[i].isOn = false;
        }
    }
}

// Set static LED color and clear all blinks
static void setStaticLED(CRGB color) {
    if (ledMutex && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
#if defined(LED_TYPE_SINGLE_COLOR)
  #if defined(LED_ACTIVE_HIGH)
        digitalWrite(LED_PIN1, (color == CRGB::Black) ? LOW : HIGH);   // normal logic
  #else
        digitalWrite(LED_PIN1, (color == CRGB::Black) ? HIGH : LOW);   // inverted
  #endif
#else
        leds[0] = color;
        FastLED.show();
#endif
        clearAllBlinks();
        xSemaphoreGive(ledMutex);
    }
}

// Universal blink starter (supports simple and multi-blink patterns)
// For simple blinks: blinksInPattern=1, pauseMs=0
// For multi-blink: blinksInPattern=2 or 3, pauseMs=pause after pattern
static void startBlink(BlinkType type, CRGB color, int count, int onMs, int offMs,
                       int pauseMs = 0, int blinksInPattern = 1) {
    if (ledMutex && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        volatile BlinkState& blink = blinkStates[type];
        blink.active = true;
        blink.count = count;
        blink.onTime = onMs;
        blink.offTime = offMs;
        blink.pauseTime = pauseMs;
        blink.blinksInPattern = blinksInPattern;
        blink.currentBlink = 0;
        blink.nextTime = millis();
        blink.isOn = false;
        blink.colorValue = (uint32_t)color;
        xSemaphoreGive(ledMutex);
    }
}

// Rapid blink function
void led_rapid_blink(int count, int delay_ms) {
    startBlink(BLINK_RAPID, CRGB::Purple, count, delay_ms, delay_ms);
}

// Visual indication of click count (works in standalone and WiFi client modes)
void led_blink_click_feedback(int clickCount) {
    // Show feedback only in modes where button clicks do something useful
    bool showFeedback = (bridgeMode == BRIDGE_STANDALONE) || 
                        (bridgeMode == BRIDGE_NET && config.wifi_mode == BRIDGE_WIFI_MODE_CLIENT);
                        
    if (showFeedback && clickCount > 0) {
        startBlink(BLINK_BUTTON, CRGB::White, clickCount, 
                   LED_BUTTON_FLASH_MS, LED_BUTTON_FLASH_MS * 2);
    }
}

// Process LED updates from main loop
void led_process_updates() {
    if (!ledMutex) return;
    
    // Take mutex once for entire update cycle - IMPORTANT!
    if (xSemaphoreTake(ledMutex, pdMS_TO_TICKS(1)) != pdTRUE) return;
    
    // Process in priority order - same as current early returns
    // Note: processBlinkPatternUnderMutex now works under already taken mutex
    if (processBlinkPatternUnderMutex(BLINK_SAFE_MODE)) {
        xSemaphoreGive(ledMutex);
        return;
    }
    
    bool buttonActive = processBlinkPatternUnderMutex(BLINK_BUTTON);
    if (buttonActive) {
        xSemaphoreGive(ledMutex);
        return;
    }
    
    // Process pending LED changes after button feedback
    if (ledUpdateNeeded) {
        leds[0] = CRGB(pendingColorValue);
        FastLED.show();
        ledUpdateNeeded = false;
    }
    
    // Client modes
    if (currentLedMode == LED_MODE_WIFI_CLIENT_SEARCHING) {
        if (processBlinkPatternUnderMutex(BLINK_CLIENT_SEARCH)) {
            xSemaphoreGive(ledMutex);
            return;
        }
    }
    if (currentLedMode == LED_MODE_WIFI_CLIENT_ERROR) {
        if (processBlinkPatternUnderMutex(BLINK_CLIENT_ERROR)) {
            xSemaphoreGive(ledMutex);
            return;
        }
    }

#if defined(MINIKIT_BT_ENABLED)
    // Bluetooth connected mode (MiniKit with BT only)
    if (currentLedMode == LED_MODE_BT_CONNECTED) {
        if (processBlinkPatternUnderMutex(BLINK_BT_CONNECTED)) {
            xSemaphoreGive(ledMutex);
            return;
        }
    }
#endif

#if defined(LED_TYPE_SINGLE_COLOR)
  #if defined(BLE_ENABLED)
    // BLE only mode: fast blink (single-color only)
    if (currentLedMode == LED_MODE_BLE_ONLY) {
        if (processBlinkPatternUnderMutex(BLINK_BLE_ONLY)) {
            xSemaphoreGive(ledMutex);
            return;
        }
    }
  #endif
    // Note: WiFi+BLE combined modes use solid ON for single-color (no blink processing needed)
#else
  #if defined(BLE_ENABLED)
    // RGB LED: fade animation for combined WiFi+BLE modes
    if (currentLedMode == LED_MODE_WIFI_AP_BLE ||
        currentLedMode == LED_MODE_WIFI_CLIENT_BLE) {
        if (processFadeUnderMutex()) {
            xSemaphoreGive(ledMutex);
            return;
        }
    }
  #endif
#endif

    // Network mode check
    if (bridgeMode == BRIDGE_NET) {
        bool rapidActive = processBlinkPatternUnderMutex(BLINK_RAPID);
        if (!rapidActive && !blinkStates[BLINK_RAPID].active) {
            // Keep LED constant (blue for RGB AP, just ON for single color AP)
            // Note: Client and BLE modes are handled by their specific blink patterns above
#if defined(LED_TYPE_SINGLE_COLOR)
  #if defined(LED_ACTIVE_HIGH)
            digitalWrite(LED_PIN1, HIGH);  // ON (normal logic)
  #else
            digitalWrite(LED_PIN1, LOW);   // ON (inverted)
  #endif
#else
            leds[0] = CRGB::Blue;
            FastLED.show();
#endif
        }
        xSemaphoreGive(ledMutex);
        return;
    }
    
    // Rapid blink for standalone
    if (processBlinkPatternUnderMutex(BLINK_RAPID)) {
        xSemaphoreGive(ledMutex);
        return;
    }
    
    // Data activity - MUST work under same mutex!
    processDataActivityUnderMutex();  // Does NOT take its own mutex
    
    xSemaphoreGive(ledMutex);
}

// Compute combined LED mode based on WiFi and BLE states
static LedMode computeCombinedMode() {
    // WiFi transient states (searching/error) take priority - show the problem
    if (currentWifiMode == LED_MODE_WIFI_CLIENT_SEARCHING ||
        currentWifiMode == LED_MODE_WIFI_CLIENT_ERROR) {
        return currentWifiMode;
    }

#if defined(BLE_ENABLED)
    if (bleIsActive) {
        // BLE is active - check WiFi state for combined mode
        switch (currentWifiMode) {
            case LED_MODE_WIFI_ON:
                return LED_MODE_WIFI_AP_BLE;
            case LED_MODE_WIFI_CLIENT_CONNECTED:
                return LED_MODE_WIFI_CLIENT_BLE;
            default:
                // WiFi not active (OFF state)
                return LED_MODE_BLE_ONLY;
        }
    }
#endif
    // BLE not active (or BLE disabled)
    if (currentWifiMode == LED_MODE_OFF && bridgeMode == BRIDGE_STANDALONE) {
        // Standalone mode - show data activity
        return LED_MODE_DATA_FLASH;
    }
    return currentWifiMode;
}

// Set WiFi-related LED mode (coordinates with BLE state)
void led_set_wifi_mode(LedMode wifiMode) {
    currentWifiMode = wifiMode;
    LedMode combined = computeCombinedMode();
    led_set_mode(combined);
}

// Set BLE active state (coordinates with WiFi state)
#if defined(BLE_ENABLED)
void led_set_ble_active(bool active) {
    bleIsActive = active;
    LedMode combined = computeCombinedMode();
    led_set_mode(combined);
}
#endif

// Set LED mode (implementation)
void led_set_mode(LedMode mode) {
    currentLedMode = mode;
    
    switch(mode) {
        case LED_MODE_OFF:
            setStaticLED(CRGB::Black);
            break;
            
        case LED_MODE_WIFI_ON:
            // WiFi AP mode: Blue for RGB, solid ON for single-color
            setStaticLED(CRGB::Blue);
            break;

        case LED_MODE_WIFI_CLIENT_CONNECTED:
#if defined(LED_TYPE_SINGLE_COLOR)
            // Single-color: solid ON (same as AP - "stable connection")
            setStaticLED(CRGB::White);
            log_msg(LOG_DEBUG, "LED set to WiFi Client Connected - solid ON");
#else
            // RGB: static green
            setStaticLED(CRGB::Green);
#endif
            break;
            
        case LED_MODE_WIFI_CLIENT_SEARCHING:
            if (ledMutex && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                clearOtherBlinks(BLINK_CLIENT_SEARCH);
                xSemaphoreGive(ledMutex);
            }
            startBlink(BLINK_CLIENT_SEARCH, CRGB(COLOR_ORANGE), -1,
                      LED_WIFI_SEARCH_BLINK_MS, LED_WIFI_SEARCH_BLINK_MS);
            log_msg(LOG_DEBUG, "LED set to WiFi Client Searching - orange slow blink");
            break;
            
        case LED_MODE_WIFI_CLIENT_ERROR:
            if (ledMutex && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                clearOtherBlinks(BLINK_CLIENT_ERROR);
                xSemaphoreGive(ledMutex);
            }
            startBlink(BLINK_CLIENT_ERROR, CRGB(COLOR_RED), -1,
                      LED_WIFI_ERROR_BLINK_MS, LED_WIFI_ERROR_BLINK_MS);
            log_msg(LOG_DEBUG, "LED set to WiFi Client Error - red fast blink");
            break;
            
        case LED_MODE_SAFE_MODE:
            if (ledMutex && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                clearOtherBlinks(BLINK_SAFE_MODE);
                xSemaphoreGive(ledMutex);
            }
            startBlink(BLINK_SAFE_MODE, CRGB(COLOR_RED), -1, 500, 4500);
            log_msg(LOG_DEBUG, "LED set to Safe Mode - red blink every 5s");
            break;

#if defined(MINIKIT_BT_ENABLED)
        case LED_MODE_BT_CONNECTED:
            if (ledMutex && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                clearOtherBlinks(BLINK_BT_CONNECTED);
                xSemaphoreGive(ledMutex);
            }
            startBlink(BLINK_BT_CONNECTED, CRGB::Blue, -1,
                      LED_BT_CONNECTED_BLINK_MS, LED_BT_CONNECTED_BLINK_MS);
            log_msg(LOG_DEBUG, "LED set to BT Connected - blinking");
            break;
#endif

#if defined(BLE_ENABLED)
        case LED_MODE_BLE_ONLY:
#if defined(LED_TYPE_SINGLE_COLOR)
            // Single-color: fast blink
            if (ledMutex && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                clearOtherBlinks(BLINK_BLE_ONLY);
                xSemaphoreGive(ledMutex);
            }
            startBlink(BLINK_BLE_ONLY, CRGB::Purple, -1,
                      LED_BLE_FAST_BLINK_MS, LED_BLE_FAST_BLINK_MS);
            log_msg(LOG_DEBUG, "LED set to BLE Only - fast blink");
#else
            // RGB: static purple
            setStaticLED(CRGB::Purple);
            log_msg(LOG_DEBUG, "LED set to BLE Only - purple");
#endif
            break;

        case LED_MODE_WIFI_AP_BLE:
#if defined(LED_TYPE_SINGLE_COLOR)
            // Single-color: solid ON (stable = all good)
            setStaticLED(CRGB::White);
            log_msg(LOG_DEBUG, "LED set to WiFi AP + BLE - solid ON");
#else
            // RGB: fade blue↔purple
            startFade(CRGB::Blue, CRGB::Purple);
            log_msg(LOG_DEBUG, "LED set to WiFi AP + BLE - blue/purple fade");
#endif
            break;

        case LED_MODE_WIFI_CLIENT_BLE:
#if defined(LED_TYPE_SINGLE_COLOR)
            // Single-color: solid ON (stable = all good)
            setStaticLED(CRGB::White);
            log_msg(LOG_DEBUG, "LED set to WiFi Client + BLE - solid ON");
#else
            // RGB: fade green↔purple
            startFade(CRGB::Green, CRGB::Purple);
            log_msg(LOG_DEBUG, "LED set to WiFi Client + BLE - green/purple fade");
#endif
            break;
#endif

        case LED_MODE_DATA_FLASH:
        default:
            // Data flash mode is handled by activity notifications
            if (ledMutex && xSemaphoreTake(ledMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                clearAllBlinks();
                xSemaphoreGive(ledMutex);
            }
            break;
    }
}