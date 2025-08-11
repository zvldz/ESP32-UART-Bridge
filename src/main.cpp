#include <Arduino.h>
#include <Preferences.h>
#include <LittleFS.h>
#include "defines.h"
#include "types.h"
#include "logging.h"
#include "config.h"
#include "leds.h"
#include "web/web_interface.h"
#include "uart/uartbridge.h"
#include "crashlog.h"
#include "usb/usb_interface.h"
#include "diagnostics.h"
#include "system_utils.h"
#include "scheduler_tasks.h"
#include "devices/device_init.h"
#include "wifi/wifi_manager.h"
#include "uart/uart_interface.h"
#include "uart/uart_dma.h"

// Global objects
Config config;
UartStats uartStats = {0};
SystemState systemState = {0};  // All fields initialized to 0/false
BridgeMode bridgeMode = BRIDGE_STANDALONE;
Preferences preferences;
FlowControlStatus flowControlStatus = {false, false};

// UART serial objects - always DMA
static UartDMA* uartBridgeSerialDMA = nullptr;
UartInterface* uartBridgeSerial = nullptr;

// Global USB mode variable
UsbMode usbMode = USB_MODE_DEVICE;

// Task handles
TaskHandle_t uartBridgeTaskHandle = NULL;
TaskHandle_t device3TaskHandle = NULL;
TaskHandle_t device4TaskHandle = NULL;

// USB interface
UsbInterface* usbInterface = NULL;

// Mutexes for thread safety
SemaphoreHandle_t logMutex = NULL;

// Spinlock for statistics critical sections
portMUX_TYPE statsMux = portMUX_INITIALIZER_UNLOCKED;

// Function declarations
void initPins();
void detectMode();
void initStandaloneMode();
void initNetworkMode();
void bootButtonISR();
void createMutexes();
void createTasks();
void handleButtonInput();

// Helper functions for device role names
const char* getDevice2RoleName(uint8_t role);
const char* getDevice3RoleName(uint8_t role);

// WiFi Manager callbacks
void on_wifi_connected();
void on_wifi_disconnected();

// FIRST thing - disable brownout using constructor attribute
void disableBrownout() __attribute__((constructor));

//================================================================
// MAIN ARDUINO FUNCTIONS
//================================================================

void setup() {
  // Disable USB Serial/JTAG peripheral interrupts
  disableUsbJtagInterrupts();

  // Print boot info
  printBootInfo();

  // Initialize configuration
  config_init(&config);

  // Clear bootloader messages
  clearBootloaderSerialBuffer();

  // Create mutexes FIRST!
  createMutexes();

  // NOW initialize logging
  logging_init();

  log_msg(LOG_INFO, "%s v%s starting", config.device_name.c_str(), config.device_version.c_str());

  // Initialize filesystem
  log_msg(LOG_INFO, "Initializing LittleFS...");

  #ifdef FORMAT_FILESYSTEM
    log_msg(LOG_WARNING, "FORMAT_FILESYSTEM flag detected - formatting LittleFS...");
    if (LittleFS.format()) {
      log_msg(LOG_INFO, "LittleFS formatted successfully");
    } else {
      log_msg(LOG_ERROR, "LittleFS format failed");
    }
  #endif

  if (!LittleFS.begin()) {
    log_msg(LOG_WARNING, "LittleFS mount failed, formatting...");
    if (LittleFS.format()) {
      log_msg(LOG_INFO, "LittleFS formatted successfully");
      if (LittleFS.begin()) {
        log_msg(LOG_INFO, "LittleFS mounted after format");
      } else {
        log_msg(LOG_ERROR, "LittleFS mount failed even after format");
        return;
      }
    } else {
      log_msg(LOG_ERROR, "LittleFS format failed");
      return;
    }
  } else {
    log_msg(LOG_INFO, "LittleFS mounted successfully");
  }

  // Load configuration from file (this may override defaults including usb_mode)
  log_msg(LOG_INFO, "Loading configuration...");
  config_load(&config);
  log_msg(LOG_INFO, "Configuration loaded");

  // Update global usbMode after loading config
  usbMode = config.usb_mode;
  
  // Initialize devices based on configuration
  initDevices();

  // Check for crash and log if needed
  crashlog_check_and_save();

  // Initialize hardware
  log_msg(LOG_INFO, "Initializing pins...");
  initPins();
  leds_init();
  log_msg(LOG_INFO, "Hardware initialized");

  // Detect boot mode
  log_msg(LOG_INFO, "Detecting boot mode...");
  detectMode();
  log_msg(LOG_INFO, "Mode detected: %s", bridgeMode == BRIDGE_STANDALONE ? "Standalone" : "Network");

  // Initialize TaskScheduler
  initializeScheduler();

  // Mode-specific initialization
  if (bridgeMode == BRIDGE_STANDALONE) {
    log_msg(LOG_INFO, "Starting standalone mode - UART bridge active");
    log_msg(LOG_INFO, "Use triple-click BOOT to enter network setup mode");
    log_msg(LOG_INFO, "Blue LED will flash on data activity");
    initStandaloneMode();
    // Enable mode-specific tasks
    enableStandaloneTasks();
  } else if (bridgeMode == BRIDGE_NET) {
    log_msg(LOG_INFO, "Starting network mode...");
    log_msg(LOG_INFO, "Purple LED will stay ON during network mode");
    initNetworkMode();
    // Enable mode-specific tasks
    enableNetworkTasks(systemState.isTemporaryNetwork);
  }

  // Create FreeRTOS tasks
  createTasks();

  log_msg(LOG_INFO, "Setup complete!");
}

void loop() {
  // Process LED updates from main thread - MUST be first
  led_process_updates();

  // Process WiFi Manager in network mode
  if (bridgeMode == BRIDGE_NET) {
    wifi_manager_process();
  }

  // Handle all button interactions
  handleButtonInput();

  // Execute all scheduled tasks
  taskScheduler.execute();

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

  log_msg(LOG_DEBUG, "BOOT button configured on GPIO%d", BOOT_BUTTON_PIN);
}

void detectMode() {
  // Check for temporary network mode flag in preferences
  preferences.begin("uartbridge", false);
  bool tempNetworkRequested = preferences.getBool("temp_net", false);
  String tempNetworkMode = preferences.getString("temp_net_mode", "AP");  // Max 15 chars!
  preferences.end();

  if (tempNetworkRequested) {
    log_msg(LOG_INFO, "Temporary network mode requested via preferences: %s", tempNetworkMode.c_str());
    // Clear the flags
    preferences.begin("uartbridge", false);
    preferences.remove("temp_net");
    preferences.remove("temp_net_mode");  // Max 15 chars!
    preferences.end();
    
    bridgeMode = BRIDGE_NET;
    systemState.isTemporaryNetwork = true;
    
    // Temporarily override WiFi mode for this session only
    if (tempNetworkMode == "CLIENT") {
      log_msg(LOG_INFO, "Temporary override: using Client mode");
      // Will use config.wifi_mode (CLIENT) as saved
    } else {
      log_msg(LOG_INFO, "Temporary override: forcing AP mode");
      // Need to temporarily force AP mode - will handle in initNetworkMode()
      systemState.tempForceApMode = true;
    }
    return;
  }

  // Check for permanent network mode configuration
  if (config.permanent_network_mode) {
    log_msg(LOG_INFO, "Permanent network mode enabled - entering network mode");
    bridgeMode = BRIDGE_NET;
    systemState.isTemporaryNetwork = false;  // Permanent network mode
    return;
  }

  log_msg(LOG_DEBUG, "Click count at startup: %d", systemState.clickCount);

  // Note: On ESP32-S3, holding BOOT (GPIO0) during power-on will enter bootloader mode
  // and this code won't run at all.

  // Check for triple click (network mode)
  log_msg(LOG_DEBUG, "Waiting for potential clicks...");
  vTaskDelay(pdMS_TO_TICKS(500));  // Wait for possible triple-click
  log_msg(LOG_DEBUG, "Final click count: %d", systemState.clickCount);

  if (systemState.clickCount >= WIFI_ACTIVATION_CLICKS) {
    log_msg(LOG_INFO, "Triple click detected - entering network mode");
    bridgeMode = BRIDGE_NET;
    systemState.isTemporaryNetwork = true;  // Setup AP is temporary
    systemState.clickCount = 0;
    return;
  }

  log_msg(LOG_INFO, "Entering standalone mode");
  bridgeMode = BRIDGE_STANDALONE;
}

void initStandaloneMode() {
  // Set LED mode for data flash explicitly
  led_set_mode(LED_MODE_DATA_FLASH);
  
  // Ensure network is not active in standalone mode
  systemState.networkActive = false;

  // Set global USB mode based on configuration
  usbMode = config.usb_mode;

  // Initialize UART DMA - always use DMA implementation
  if (!uartBridgeSerialDMA) {
    uartBridgeSerialDMA = new UartDMA(UART_NUM_1);
    uartBridgeSerial = uartBridgeSerialDMA;
    log_msg(LOG_INFO, "UART DMA interface created");
  }

  // Create USB interface based on configuration (only if Device 2 is USB)
  if (config.device2.role == D2_USB) {
    switch(config.usb_mode) {
      case USB_MODE_HOST:
        log_msg(LOG_INFO, "Creating USB Host interface");
        usbInterface = createUsbHost(config.baudrate);
        break;
      case USB_MODE_DEVICE:
      default:
        log_msg(LOG_INFO, "Creating USB Device interface");
        usbInterface = createUsbDevice(config.baudrate);
        break;
    }

    if (usbInterface) {
      usbInterface->init();
      log_msg(LOG_INFO, "USB interface initialized");
    } else {
      log_msg(LOG_ERROR, "Failed to create USB interface");
    }
  } else {
    log_msg(LOG_INFO, "Device 2 is not configured for USB, skipping USB initialization");
  }

  // Initialize UART bridge
  initMainUART(uartBridgeSerial, &config, &uartStats, usbInterface);

  log_msg(LOG_INFO, "UART Bridge ready - transparent forwarding active");
}

void initNetworkMode() {
  // Инициализация WiFi Manager
  esp_err_t ret = wifi_manager_init();
  
  if (ret != ESP_OK && config.device4.role != D4_NONE) {
    // Критическая ошибка только если Device 4 включен
    log_msg(LOG_ERROR, "Failed to init WiFi, entering safe mode");
    systemState.wifiSafeMode = true;
    led_set_mode(LED_MODE_SAFE_MODE);
    return;
  } else if (ret != ESP_OK) {
    // Device 4 выключен - продолжаем без WiFi
    log_msg(LOG_WARNING, "WiFi init failed, but Device 4 disabled - continuing");
  }

  // Запуск WiFi в нужном режиме
  if (systemState.tempForceApMode) {
    log_msg(LOG_INFO, "Starting temporary WiFi AP mode (forced by triple click)");
    wifi_manager_start_ap(config.ssid, config.password);
    led_set_mode(LED_MODE_WIFI_ON);
    systemState.tempForceApMode = false;
  } else if (config.wifi_mode == BRIDGE_WIFI_MODE_CLIENT) {
    log_msg(LOG_INFO, "Starting WiFi Client mode");
    wifi_manager_start_client(config.wifi_client_ssid, config.wifi_client_password);
  } else {
    log_msg(LOG_INFO, "Starting WiFi AP mode");
    wifi_manager_start_ap(config.ssid, config.password);
    led_set_mode(LED_MODE_WIFI_ON);
  }
  
  // Set network as active
  systemState.networkActive = true;

  // Use configured USB mode (from config file) - only if Device 2 is USB
  usbMode = config.usb_mode;

  // Initialize UART DMA - always use DMA implementation
  if (!uartBridgeSerialDMA) {
    uartBridgeSerialDMA = new UartDMA(UART_NUM_1);
    uartBridgeSerial = uartBridgeSerialDMA;
    log_msg(LOG_INFO, "UART DMA interface created for network mode");
  }

  // Create USB interface even in network mode (for bridge functionality)
  if (config.device2.role == D2_USB) {
    switch(config.usb_mode) {
      case USB_MODE_HOST:
        log_msg(LOG_INFO, "Creating USB Host interface in network mode");
        usbInterface = createUsbHost(config.baudrate);
        break;
      case USB_MODE_DEVICE:
      default:
        log_msg(LOG_INFO, "Creating USB Device interface in network mode");
        usbInterface = createUsbDevice(config.baudrate);
        break;
    }
    
    if (usbInterface) {
      usbInterface->init();
      log_msg(LOG_INFO, "USB interface initialized in network mode");
    } else {
      log_msg(LOG_ERROR, "Failed to create USB interface in network mode");
    }
  } else {
    log_msg(LOG_INFO, "Device 2 is not configured for USB in network mode");
  }

  // Initialize UART bridge even in network mode (for statistics)
  initMainUART(uartBridgeSerial, &config, &uartStats, usbInterface);

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
    log_msg(LOG_DEBUG, "Click timeout expired, resetting clickCount from %d to 0", systemState.clickCount);
    systemState.clickCount = 0;
  }

  // Update LED feedback when click count changes (only for modes where clicks do something)
  if ((bridgeMode == BRIDGE_STANDALONE || 
       (bridgeMode == BRIDGE_NET && config.wifi_mode == BRIDGE_WIFI_MODE_CLIENT)) &&
      systemState.clickCount != lastLedClickCount) {
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
      log_msg(LOG_INFO, "Button held for 5 seconds - resetting WiFi to defaults");

      // Reset to default WiFi settings
      config.ssid = DEFAULT_AP_SSID;
      config.password = DEFAULT_AP_PASSWORD;
      config_save(&config);

      // Visual feedback
      led_rapid_blink(10, LED_WIFI_RESET_BLINK_MS);

      log_msg(LOG_INFO, "WiFi reset to defaults: SSID=%s, Password=%s", DEFAULT_AP_SSID, DEFAULT_AP_PASSWORD);
      log_msg(LOG_INFO, "Restarting...");
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

    log_msg(LOG_DEBUG, "Button click detected! Time: %lu, Last: %lu, Diff: %lu", 
            currentTime, systemState.lastClickTime, currentTime - systemState.lastClickTime);

    if (systemState.lastClickTime == 0 || (currentTime - systemState.lastClickTime < CLICK_TIMEOUT)) {
      systemState.clickCount = systemState.clickCount + 1;
      log_msg(LOG_DEBUG, "Click registered, count: %d", systemState.clickCount);

      // Check for triple click - enhanced logic
      if (systemState.clickCount >= WIFI_ACTIVATION_CLICKS && 
          (bridgeMode == BRIDGE_STANDALONE || 
           (bridgeMode == BRIDGE_NET && config.wifi_mode == BRIDGE_WIFI_MODE_CLIENT))) {
        
        preferences.begin("uartbridge", false);
        
        if (bridgeMode == BRIDGE_STANDALONE) {
          // From standalone → activate saved WiFi mode
          log_msg(LOG_INFO, "*** TRIPLE CLICK: Standalone → Saved WiFi Mode ***");
          
          
          preferences.putBool("temp_net", true);
          if (config.wifi_mode == BRIDGE_WIFI_MODE_CLIENT) {
            preferences.putString("temp_net_mode", "CLIENT");  // Max 15 chars!
            log_msg(LOG_INFO, "*** Will start in WiFi Client mode ***");
          } else {
            preferences.putString("temp_net_mode", "AP");  // Max 15 chars!
            log_msg(LOG_INFO, "*** Will start in WiFi AP mode ***");
          }
          
          // TEMPORARY DEBUG HACK - Move after preferences.end() for proper verification
        } else {
          // From active Client mode → force temporary AP mode
          log_msg(LOG_INFO, "*** TRIPLE CLICK: Client Mode → Force AP Mode ***");
          preferences.putBool("temp_net", true);
          preferences.putString("temp_net_mode", "AP");  // Max 15 chars!
          log_msg(LOG_INFO, "*** Will force temporary AP mode ***");
        }
        
        preferences.end();
        
        log_msg(LOG_INFO, "*** Restarting in 1 second ***");
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP.restart();
      }
    } else {
      log_msg(LOG_DEBUG, "Click timeout exceeded (%lu ms), resetting to 1", currentTime - systemState.lastClickTime);
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
// TASK MANAGEMENT
//================================================================

void createMutexes() {
  // Create logging mutex
  logMutex = xSemaphoreCreateMutex();
  if (logMutex == NULL) {
    // Cannot use log_msg here as logging system not initialized
    return;
  }

  // Always create Device 4 mutex (needed for logging)
  extern SemaphoreHandle_t device4LogMutex;
  device4LogMutex = xSemaphoreCreateMutex();
}

void createTasks() {
  // Create UART bridge task on core 0 (multi-core ESP32)
  xTaskCreatePinnedToCore(
    uartBridgeTask,        // Task function
    "UART_Bridge_Task",    // Task name
    16384,                 // Stack size (increased from 8192)
    NULL,                  // Parameters
    UART_TASK_PRIORITY,    // Priority
    &uartBridgeTaskHandle, // Task handle
    UART_TASK_CORE         // Core 0 (dedicated for UART bridge)
  );

  log_msg(LOG_INFO, "UART Bridge task created on core %d (priority %d)", UART_TASK_CORE, UART_TASK_PRIORITY);

  // Create Device 3 task only if needed
  if (config.device3.role != D3_NONE && config.device3.role != D3_UART3_LOG) {
    xTaskCreatePinnedToCore(
      device3Task,           // Task function
      "Device3_Task",        // Task name
      4096,                  // Stack size
      NULL,                  // Parameters
      UART_TASK_PRIORITY-1,  // Slightly lower priority than main UART
      &device3TaskHandle,    // Task handle
      DEVICE3_TASK_CORE      // Same core as UART (core 0)
    );
    
    log_msg(LOG_INFO, "Device 3 task created on core %d for %s mode", DEVICE3_TASK_CORE, config.device3.role == D3_UART3_MIRROR ? "Mirror" : "Bridge");
  }

  // Create Device 4 task only if configured
  if (config.device4.role != D4_NONE) {
    xTaskCreatePinnedToCore(
      device4Task,
      "Device4_Task",
      8192,
      NULL,
      UART_TASK_PRIORITY-2,
      &device4TaskHandle,
      DEVICE4_TASK_CORE      // Core 1 for network operations
    );
    log_msg(LOG_INFO, "Device 4 task created on core %d for %s", DEVICE4_TASK_CORE, getDevice4RoleName(config.device4.role));
  }
}

//================================================================
// WiFi Manager Callbacks
//================================================================

void on_wifi_connected() {
  log_msg(LOG_INFO, "WiFi Manager: Client connected successfully");
  
  // Set LED mode for connected client
  led_set_mode(LED_MODE_WIFI_CLIENT_CONNECTED);
  
  // Set network event group bit for Device 4 synchronization
  if (networkEventGroup) {
    xEventGroupSetBits(networkEventGroup, NETWORK_CONNECTED_BIT);
  }
}

void on_wifi_disconnected() {
  log_msg(LOG_WARNING, "WiFi Manager: Client disconnected");
  
  // Clear network event group bit
  if (networkEventGroup) {
    xEventGroupClearBits(networkEventGroup, NETWORK_CONNECTED_BIT);
  }
}