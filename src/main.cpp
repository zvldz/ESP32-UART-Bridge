#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <LittleFS.h>
#include "defines.h"
#include "types.h"
#include "logging.h"
#include "config.h"
#include "leds.h"
#include "web_interface.h"
#include "uartbridge.h"
#include "crashlog.h"

// Global constants
const int DEBUG_MODE = 1;  // 0 = production UART bridge, 1 = debug only (no bridge functionality)

// Global objects
Config config;
UartStats uartStats = {0};
SystemState systemState = {0};
LedState ledState = {0};
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
void IRAM_ATTR bootButtonISR();
void createMutexes();
void createTasks();

void setup() {
  // Initialize Serial ONLY in debug mode
  if (DEBUG_MODE == 1) {
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== " + config.device_name + " Starting (DEBUG MODE) ===");
    Serial.println("WARNING: UART bridge functionality DISABLED in debug mode");
    Serial.println("Set DEBUG_MODE to 0 for production use");
  }
  
  ///////////////////
    esp_reset_reason_t reason = esp_reset_reason();
  
  // ВСЕГДА выводим причину сброса, даже в production mode
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n=== BOOT INFO ===");
  Serial.print("Reset reason: ");
  Serial.println(esp_reset_reason());
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
  
  // Проверка стека при panic
  if (reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT) {
    Serial.println("CRASH DETECTED!");
  }



  ///////////////////

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
  log_msg("Mode detected: " + String(currentMode == MODE_NORMAL ? "Normal" : 
                                     currentMode == MODE_FLASHING ? "Flashing" : "Config"));
  
  if (currentMode == MODE_FLASHING) {
    log_msg("Flashing mode detected - waiting for firmware upload");
    while(true) {
      delay(1000);
    }
  }
  
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
    log_msg("Blue LED will stay ON during WiFi config mode");
    initConfigMode();
  }
  
  // Create FreeRTOS tasks
  createTasks();
  
  log_msg("Setup complete!");
}

void loop() {
  // This loop runs on Core 1 for multi-core systems - dedicated to WiFi, web server, and UI
  
  // Memory diagnostics in debug mode only
  if (DEBUG_MODE == 1) {
    static unsigned long lastMemoryCheck = 0;
    if (millis() - lastMemoryCheck > 10000) {  // Every 10 seconds
      log_msg("=== System Memory Status ===");
      log_msg("Free heap: " + String(ESP.getFreeHeap()) + " bytes");
      log_msg("Largest free block: " + String(ESP.getMaxAllocHeap()) + " bytes");
      log_msg("Min free heap: " + String(ESP.getMinFreeHeap()) + " bytes");
      lastMemoryCheck = millis();
    }
  }
  
  // Handle button press in main loop (safer than ISR)
  if (systemState.buttonPressed) {
    systemState.buttonPressed = false;
    unsigned long currentTime = systemState.buttonPressTime;
    
    log_msg("Button pressed! Time: " + String(currentTime) + 
            ", Last: " + String(systemState.lastClickTime) + 
            ", Diff: " + String(currentTime - systemState.lastClickTime));
    
    if (systemState.lastClickTime == 0 || (currentTime - systemState.lastClickTime < CLICK_TIMEOUT)) {
      systemState.clickCount++;
      log_msg("Click registered, count: " + String(systemState.clickCount));
      
      // Check for activation immediately after each click
      if (systemState.clickCount >= WIFI_ACTIVATION_CLICKS && currentMode == MODE_NORMAL) {
        log_msg("*** TRIPLE CLICK DETECTED! Activating WiFi Config Mode ***");
        log_msg("*** Setting WiFi mode flag and restarting ***");
        
        // Set flag in preferences to enter WiFi mode after restart
        preferences.begin("uartbridge", false);
        preferences.putBool("wifi_mode", true);
        preferences.end();
        
        delay(1000);
        ESP.restart();
      }
    } else {
      log_msg("Click timeout exceeded (" + String(currentTime - systemState.lastClickTime) + 
              " ms), resetting to 1");
      systemState.clickCount = 1;
    }
    
    systemState.lastClickTime = currentTime;
  }

  // Debug heartbeat every 10 seconds in debug mode
  if (DEBUG_MODE == 1) {
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 10000) {
      log_msg("System heartbeat: clickCount=" + String(systemState.clickCount) + 
              ", currentMode=" + String(currentMode) + 
              ", uptime=" + String(millis()/1000) + "s");
      lastHeartbeat = millis();
    }
  }
  
  // Visual indication of click count (only in normal mode, don't interfere with WiFi mode)
  if (currentMode == MODE_NORMAL) {
    if (systemState.clickCount > 0 && millis() - systemState.lastClickTime < CLICK_TIMEOUT) {
      led_blink_click_feedback(systemState.clickCount);
    } else {
      if (systemState.clickCount > 0) {
        log_msg("Click timeout expired, resetting clickCount from " + 
                String(systemState.clickCount) + " to 0");
        systemState.clickCount = 0;  // Reset click count when timeout expires
      }
    }
  }
  
  // Handle long press for WiFi reset (5+ seconds)
  static unsigned long buttonHoldStart = 0;
  static bool buttonHoldDetected = false;
  
  if (digitalRead(BOOT_PIN) == LOW) {
    if (buttonHoldStart == 0) {
      buttonHoldStart = millis();
    } else if (!buttonHoldDetected && (millis() - buttonHoldStart > 5000)) {
      buttonHoldDetected = true;
      log_msg("Button held for 5 seconds - resetting WiFi to defaults");
      
      // Reset to default WiFi settings
      config.ssid = DEFAULT_AP_SSID;
      config.password = DEFAULT_AP_PASSWORD;
      config_save(&config);
      
      // Visual feedback - rapid LED blinking
      for (int i = 0; i < 10; i++) {
        digitalWrite(BLUE_LED_PIN, LOW);   // LED ON (inverted logic)
        delay(100);
        digitalWrite(BLUE_LED_PIN, HIGH);  // LED OFF (inverted logic)
        delay(100);
      }
      
      log_msg("WiFi reset to defaults: SSID=" + String(DEFAULT_AP_SSID) + ", Password=" + String(DEFAULT_AP_PASSWORD));
      log_msg("Restarting...");
      delay(1000);
      ESP.restart();
    }
  } else {
    buttonHoldStart = 0;
    buttonHoldDetected = false;
  }
  
  // Update LED state
  led_update();
  
  // Update RTC variables for crash logging
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
  
  if (currentMode == MODE_CONFIG) {
    if (checkWiFiTimeout()) {
      log_msg("WiFi timeout - switching to normal mode");
      ESP.restart();
    }
  }
  
  // Small delay to prevent watchdog issues
  vTaskDelay(pdMS_TO_TICKS(10));
}

void initPins() {
  pinMode(BOOT_PIN, INPUT_PULLUP);  // GPIO9 - real BOOT button
  
  // Attach interrupt for button clicks on correct pin
  attachInterrupt(digitalPinToInterrupt(BOOT_PIN), bootButtonISR, FALLING);
  
  log_msg("BOOT button configured on GPIO" + String(BOOT_PIN));
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
  delay(500);  // Wait a bit for potential clicks
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
      delay(500);  // Allow time for USB enumeration to complete
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
    delay(1000);
    log_msg("Serial USB initialized in WiFi mode at " + String(config.baudrate) + " baud");
  }

  // Initialize UART bridge even in config mode (for statistics)
  uartbridge_init(&uartBridgeSerial, &config, &uartStats);

  // Initialize web server
  webserver_init(&config, &uartStats, &systemState);
}

void IRAM_ATTR bootButtonISR() {
  // Minimal ISR - just set flag and time
  systemState.buttonPressed = true;
  systemState.buttonPressTime = millis();
}

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
        32768,                 // Увеличьте стек для WiFi
        NULL,                  
        WEB_TASK_PRIORITY,     
        &webServerTaskHandle,
        1                      // Core 1 для WiFi!
      );
    #endif    
    
    log_msg("Web Server task created (priority " + String(WEB_TASK_PRIORITY) + ")");
  }
}