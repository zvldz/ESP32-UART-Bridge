#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <LittleFS.h>
#include "soc/soc.h"          // for disableBrownout()
#include "soc/rtc_cntl_reg.h" // for disableBrownout()
#include "defines.h"
#include "types.h"
#include "logging.h"
#include "config.h"
#include "leds.h"
#include "web_interface.h"
#include "uartbridge.h"
#include "crashlog.h"

// Global constants
#ifndef DEBUG_MODE_BUILD
  #define DEBUG_MODE_BUILD 0
#endif
const int DEBUG_MODE = DEBUG_MODE_BUILD;  // 0 = production UART bridge, 1 = debug only (no bridge functionality)

// Global objects
Config config;
UartStats uartStats = {0};
SystemState systemState = {0};
DeviceMode currentMode = MODE_NORMAL;
Preferences preferences;
HardwareSerial uartBridgeSerial(1);  // UART1
FlowControlStatus flowControlStatus = {false, false};

// Task handles
TaskHandle_t uartBridgeTaskHandle = NULL;
TaskHandle_t webServerTaskHandle = NULL;

// Mutexes for thread safety
SemaphoreHandle_t logMutex = NULL;

// Spinlock for statistics critical sections
portMUX_TYPE statsMux = portMUX_INITIALIZER_UNLOCKED;

// RTC variables for crash logging (survive reset but not power loss)
RTC_DATA_ATTR uint32_t g_last_heap = 0;
RTC_DATA_ATTR uint32_t g_last_uptime = 0;
RTC_DATA_ATTR uint32_t g_min_heap = 0xFFFFFFFF;

// Function declarations
void initPins();
void detectMode();
void initNormalMode();
void initConfigMode();
void bootButtonISR();
void createMutexes();
void createTasks();
void printBootInfo();
void handleButtonInput();
void debugOutput();
void updateCrashLogVariables();

// FIRST thing - disable brownout
void disableBrownout() __attribute__((constructor));
void disableBrownout() {
  uint32_t val = READ_PERI_REG(RTC_CNTL_BROWN_OUT_REG);
  val &= ~(1 << 27);  // RTC_CNTL_BROWN_OUT_ENA
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, val);
}

//================================================================
// MAIN ARDUINO FUNCTIONS
//================================================================

void setup() {
  // Print boot info first
  printBootInfo();

  // Initialize Serial based on mode
  if (DEBUG_MODE == 1) {
    Serial.begin(115200);
    vTaskDelay(pdMS_TO_TICKS(1000));
    Serial.println("=== " + config.device_name + " Starting (DEBUG MODE) ===");
    Serial.println("WARNING: UART bridge functionality DISABLED in debug mode");
    Serial.println("Set DEBUG_MODE to 0 for production use");
  } else {
    // Production mode - suppress logs and clear buffer
    #if defined(CORE_DEBUG_LEVEL) && CORE_DEBUG_LEVEL == 0
      esp_log_level_set("*", ESP_LOG_NONE);
    #endif
    
    // Clear bootloader messages from serial buffer
    Serial.begin(115200);
    vTaskDelay(pdMS_TO_TICKS(100));
    while (Serial.available()) Serial.read();
    Serial.flush();
    Serial.end();
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  // Create mutexes for thread safety
  createMutexes();

  // Record device start time
  uartStats.deviceStartTime = millis();

  // Initialize logging system
  logging_init();

  // Initialize configuration first to get device name
  config_init(&config);

  log_msg(config.device_name + " v" + config.version + " starting");

  // Initialize filesystem
  log_msg("Initializing LittleFS...");

  #ifdef FORMAT_FILESYSTEM
    log_msg("FORMAT_FILESYSTEM flag detected - formatting LittleFS...");
    if (LittleFS.format()) {
      log_msg("LittleFS formatted successfully");
    } else {
      log_msg("ERROR: LittleFS format failed");
    }
  #endif

  if (!LittleFS.begin()) {
    log_msg("LittleFS mount failed, formatting...");
    if (LittleFS.format()) {
      log_msg("LittleFS formatted successfully");
      if (LittleFS.begin()) {
        log_msg("LittleFS mounted after format");
      } else {
        log_msg("ERROR: LittleFS mount failed even after format");
        return;
      }
    } else {
      log_msg("ERROR: LittleFS format failed");
      return;
    }
  } else {
    log_msg("LittleFS mounted successfully");
  }

  // Initialize configuration
  log_msg("Initializing configuration...");
  config_init(&config);

  // Load configuration from file
  log_msg("Loading configuration...");
  config_load(&config);
  log_msg("Configuration loaded");

  // Check for crash and log if needed
  crashlog_check_and_save();

  // Initialize hardware
  log_msg("Initializing pins...");
  initPins();
  leds_init();
  log_msg("Hardware initialized");

  // Detect boot mode
  log_msg("Detecting boot mode...");
  detectMode();
  log_msg("Mode detected: " + String(currentMode == MODE_NORMAL ? "Normal" : "Config"));

  // Mode-specific initialization
  if (currentMode == MODE_NORMAL) {
    if (DEBUG_MODE == 1) {
      log_msg("WARNING: Debug mode enabled - UART forwarding disabled!");
      log_msg("UART task will only read GPIO pins without data transfer");
    } else {
      log_msg("Starting normal mode - UART bridge active");
    }
    log_msg("Use triple-click BOOT to enter config mode");
    log_msg("Blue LED will flash on data activity");
    initNormalMode();
  } else if (currentMode == MODE_CONFIG) {
    log_msg("Starting WiFi config mode...");
    log_msg("Purple LED will stay ON during WiFi config mode");
    initConfigMode();
  }

  // Create FreeRTOS tasks
  createTasks();

  log_msg("Setup complete!");
}

void loop() {
  // Process LED updates from main thread - MUST be first
  led_process_updates();

  // Debug output (memory stats, heartbeat) - only in debug mode
  debugOutput();

  // Handle all button interactions
  handleButtonInput();

  // Update crash log RTC variables
  updateCrashLogVariables();

  // Check WiFi timeout in config mode
  if (currentMode == MODE_CONFIG) {
    if (checkWiFiTimeout()) {
      log_msg("WiFi timeout - switching to normal mode");
      ESP.restart();
    }
  }

  // Small delay to prevent watchdog issues
  vTaskDelay(pdMS_TO_TICKS(10));
}

//================================================================
// INITIALIZATION FUNCTIONS
//================================================================

void initPins() {
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  // Attach interrupt for button clicks on correct pin
  attachInterrupt(digitalPinToInterrupt(BOOT_BUTTON_PIN), bootButtonISR, FALLING);

  log_msg("BOOT button configured on GPIO" + String(BOOT_BUTTON_PIN));
}

void detectMode() {
  // Check for WiFi mode flag in preferences
  preferences.begin("uartbridge", false);
  bool wifiModeRequested = preferences.getBool("wifi_mode", false);
  preferences.end();

  if (wifiModeRequested) {
    log_msg("WiFi mode requested via preferences - entering config mode");
    // Clear the flag
    preferences.begin("uartbridge", false);
    preferences.putBool("wifi_mode", false);
    preferences.end();
    currentMode = MODE_CONFIG;
    return;
  }

  log_msg("Click count at startup: " + String(systemState.clickCount));

  // Note: On ESP32-S3, holding BOOT (GPIO0) during power-on will enter bootloader mode
  // and this code won't run at all.

  // Check for triple click (config mode)
  log_msg("Waiting for potential clicks...");
  vTaskDelay(pdMS_TO_TICKS(500));  // Wait for possible triple-click
  log_msg("Final click count: " + String(systemState.clickCount));

  if (systemState.clickCount >= WIFI_ACTIVATION_CLICKS) {
    log_msg("Triple click detected - entering config mode");
    currentMode = MODE_CONFIG;
    systemState.clickCount = 0;
    return;
  }

  log_msg("Entering normal mode");
  currentMode = MODE_NORMAL;
}

void initNormalMode() {
  // Set LED mode for data flash explicitly
  led_set_mode(LED_MODE_DATA_FLASH);

  // Initialize Serial for UART bridge ONLY in production mode
  if (DEBUG_MODE == 0) {
    Serial.begin(config.baudrate);

    // Increase RX buffer for better performance at high speeds
    if (config.baudrate >= 115200) {
      Serial.setRxBufferSize(1024);
    }

    // Wait for USB connection (maximum 2 seconds)
    unsigned long startTime = millis();
    while (!Serial && (millis() - startTime < 2000)) {
      delay(10);
    }

    // Add stabilization delay only if USB is connected
    if (Serial) {
      vTaskDelay(pdMS_TO_TICKS(500));  // Allow USB detection time
      log_msg("USB connected, serial ready at " + String(config.baudrate) + " baud");
    } else {
      log_msg("No USB detected, continuing without USB connection");
    }
  }

  // Initialize UART bridge
  uartbridge_init(&uartBridgeSerial, &config, &uartStats);

  if (DEBUG_MODE == 0) {
    log_msg("UART Bridge ready - transparent forwarding active");
  } else {
    log_msg("DEBUG MODE: UART task will only read pins (no data forwarding)");
  }
}

void initConfigMode() {
  // Set LED mode for WiFi config
  led_set_mode(LED_MODE_WIFI_ON);

  // Initialize USB Serial even in config mode (for bridge functionality)
  if (DEBUG_MODE == 0) {
    Serial.begin(config.baudrate);
    vTaskDelay(pdMS_TO_TICKS(1000));
    log_msg("Serial USB initialized in WiFi mode at " + String(config.baudrate) + " baud");
  }

  // Initialize UART bridge even in config mode (for statistics)
  uartbridge_init(&uartBridgeSerial, &config, &uartStats);

  // Initialize web server
  webserver_init(&config, &uartStats, &systemState);
}

//================================================================
// BUTTON HANDLING
//================================================================

void bootButtonISR() {
  // Minimal ISR - just set flag and time
  systemState.buttonPressed = true;
  systemState.buttonPressTime = millis();
}

void handleButtonInput() {
  static unsigned long buttonHoldStart = 0;
  static bool buttonHoldDetected = false;
  static bool clickProcessed = false;
  static int lastLedClickCount = 0;

  // Check for click timeout
  if (systemState.clickCount > 0 && millis() - systemState.lastClickTime >= CLICK_TIMEOUT) {
    log_msg("Click timeout expired, resetting clickCount from " +
            String(systemState.clickCount) + " to 0");
    systemState.clickCount = 0;
  }

  // Update LED feedback only when click count changes
  if (currentMode == MODE_NORMAL && systemState.clickCount != lastLedClickCount) {
    if (systemState.clickCount > 0) {
      led_blink_click_feedback(systemState.clickCount);
    }
    lastLedClickCount = systemState.clickCount;
  }

  bool buttonCurrentlyPressed = (digitalRead(BOOT_BUTTON_PIN) == LOW);

  // Handle button press event from ISR
  if (systemState.buttonPressed && !clickProcessed) {
    // Mark as processed but don't clear flag yet
    clickProcessed = true;
    buttonHoldStart = millis();  // Start tracking potential long press
  }

  // Handle long press
  if (buttonCurrentlyPressed && buttonHoldStart > 0) {
    if (!buttonHoldDetected && (millis() - buttonHoldStart > 5000)) {
      buttonHoldDetected = true;
      log_msg("Button held for 5 seconds - resetting WiFi to defaults");

      // Reset to default WiFi settings
      config.ssid = DEFAULT_AP_SSID;
      config.password = DEFAULT_AP_PASSWORD;
      config_save(&config);

      // Visual feedback
      led_rapid_blink(10, LED_WIFI_RESET_BLINK_MS);

      log_msg("WiFi reset to defaults: SSID=" + String(DEFAULT_AP_SSID) +
              ", Password=" + String(DEFAULT_AP_PASSWORD));
      log_msg("Restarting...");
      vTaskDelay(pdMS_TO_TICKS(2000));  // Wait for LED blink to complete
      ESP.restart();
    }
  }

  // Handle button release (clicks)
  if (!buttonCurrentlyPressed && clickProcessed && !buttonHoldDetected) {
    // Button was released without long press - process as click
    systemState.buttonPressed = false;
    clickProcessed = false;

    unsigned long currentTime = systemState.buttonPressTime;

    log_msg("Button click detected! Time: " + String(currentTime) +
            ", Last: " + String(systemState.lastClickTime) +
            ", Diff: " + String(currentTime - systemState.lastClickTime));

    if (systemState.lastClickTime == 0 || (currentTime - systemState.lastClickTime < CLICK_TIMEOUT)) {
      systemState.clickCount = systemState.clickCount + 1;
      log_msg("Click registered, count: " + String(systemState.clickCount));

      // Check for triple click
      if (systemState.clickCount >= WIFI_ACTIVATION_CLICKS && currentMode == MODE_NORMAL) {
        log_msg("*** TRIPLE CLICK DETECTED! Activating WiFi Config Mode ***");
        log_msg("*** Setting WiFi mode flag and restarting ***");

        preferences.begin("uartbridge", false);
        preferences.putBool("wifi_mode", true);
        preferences.end();

        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP.restart();
      }
    } else {
      log_msg("Click timeout exceeded (" + String(currentTime - systemState.lastClickTime) +
              " ms), resetting to 1");
      systemState.clickCount = 1;
    }

    systemState.lastClickTime = currentTime;
  }

  // Reset states when button is released
  if (!buttonCurrentlyPressed) {
    if (buttonHoldDetected || clickProcessed) {
      systemState.buttonPressed = false;
      clickProcessed = false;
    }
    buttonHoldStart = 0;
    buttonHoldDetected = false;
  }
}

//================================================================
// SYSTEM FUNCTIONS
//================================================================

void printBootInfo() {
  if (DEBUG_MODE != 1) return;
  
  // Initialize Serial for boot info
  Serial.begin(115200);
  vTaskDelay(pdMS_TO_TICKS(100));

  // Get reset reason
  esp_reset_reason_t reason = esp_reset_reason();

  Serial.println("\n\n=== BOOT INFO ===");
  Serial.print("Reset reason: ");
  Serial.println(reason);
  Serial.print("Reset reason text: ");

  switch(reason) {
    case ESP_RST_UNKNOWN: Serial.println("UNKNOWN"); break;
    case ESP_RST_POWERON: Serial.println("POWERON"); break;
    case ESP_RST_SW: Serial.println("SW_RESET"); break;
    case ESP_RST_PANIC: Serial.println("PANIC"); break;
    case ESP_RST_INT_WDT: Serial.println("INT_WDT"); break;
    case ESP_RST_TASK_WDT: Serial.println("TASK_WDT"); break;
    case ESP_RST_WDT: Serial.println("WDT"); break;
    case ESP_RST_DEEPSLEEP: Serial.println("DEEPSLEEP"); break;
    case ESP_RST_BROWNOUT: Serial.println("BROWNOUT"); break;
    case ESP_RST_SDIO: Serial.println("SDIO"); break;
    default: Serial.println("Unknown code: " + String(reason)); break;
  }

  // Stack check at panic
  if (reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT) {
    Serial.println("CRASH DETECTED!");
  }

  Serial.println("===================\n");
  Serial.flush();  // Ensure all data is sent

  // Close Serial to allow proper initialization later
  Serial.end();
  vTaskDelay(pdMS_TO_TICKS(50));  // Small delay to ensure Serial closes properly
}

void debugOutput() {
  if (DEBUG_MODE != 1) return;

  // Memory diagnostics
  static unsigned long lastMemoryCheck = 0;
  if (millis() - lastMemoryCheck > 10000) {
    log_msg("=== System Memory Status ===");
    log_msg("Free heap: " + String(ESP.getFreeHeap()) + " bytes");
    log_msg("Largest free block: " + String(ESP.getMaxAllocHeap()) + " bytes");
    log_msg("Min free heap: " + String(ESP.getMinFreeHeap()) + " bytes");
    lastMemoryCheck = millis();
  }

  // Debug heartbeat
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 10000) {
    log_msg("System heartbeat: clickCount=" + String(systemState.clickCount) +
            ", currentMode=" + String(currentMode) +
            ", uptime=" + String(millis()/1000) + "s");
    lastHeartbeat = millis();
  }
}

void updateCrashLogVariables() {
  static unsigned long lastRtcUpdate = 0;
  if (millis() - lastRtcUpdate > CRASHLOG_UPDATE_INTERVAL_MS) {
    g_last_heap = ESP.getFreeHeap();
    g_last_uptime = millis() / 1000;

    // Track minimum heap
    if (g_last_heap < g_min_heap) {
      g_min_heap = g_last_heap;
    }

    lastRtcUpdate = millis();
  }
}

//================================================================
// TASK MANAGEMENT
//================================================================

void createMutexes() {
  // Create logging mutex
  logMutex = xSemaphoreCreateMutex();
  if (logMutex == NULL) {
    if (DEBUG_MODE == 1) {
      Serial.println("Failed to create logging mutex!");
    }
    return;
  }
}

void createTasks() {
  // Create UART bridge task with increased stack size
  #ifdef CONFIG_FREERTOS_UNICORE
    xTaskCreate(
      uartBridgeTask,        // Task function
      "UART_Bridge_Task",    // Task name
      16384,                 // Stack size (increased from 8192)
      NULL,                  // Parameters
      UART_TASK_PRIORITY,    // Priority
      &uartBridgeTaskHandle  // Task handle
    );
  #else
    xTaskCreatePinnedToCore(
      uartBridgeTask,        // Task function
      "UART_Bridge_Task",    // Task name
      16384,                 // Stack size (increased from 8192)
      NULL,                  // Parameters
      UART_TASK_PRIORITY,    // Priority
      &uartBridgeTaskHandle, // Task handle
      0                      // Core 0 (dedicated for UART bridge)
    );
  #endif

  log_msg("UART Bridge task created (priority " + String(UART_TASK_PRIORITY) + ")");
  log_msg("Adaptive buffering: " + String(UART_BUFFER_SIZE) + " byte buffer, protocol optimized");
  log_msg("Thresholds: 200μs/1ms/5ms/15ms for optimal performance");

  // Create web server task only in CONFIG mode with increased stack size
  if (currentMode == MODE_CONFIG) {
    disableBrownout();
    #ifdef CONFIG_FREERTOS_UNICORE
      xTaskCreate(
        webServerTask,         // Task function
        "Web_Server_Task",     // Task name
        16384,                 // Stack size (increased from 8192)
        NULL,                  // Parameters
        WEB_TASK_PRIORITY,     // Priority
        &webServerTaskHandle   // Task handle
      );
    #else
      xTaskCreatePinnedToCore(
        webServerTask,
        "Web_Server_Task",
        32768,
        NULL,
        WEB_TASK_PRIORITY,
        &webServerTaskHandle,
        1
      );
    #endif

    log_msg("Web Server task created (priority " + String(WEB_TASK_PRIORITY) + ")");
  }
}