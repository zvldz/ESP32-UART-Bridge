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
#include "usb_interface.h"
#include "diagnostics.h"
#include "system_utils.h"
#include "scheduler_tasks.h"
#include "device_init.h"

// DMA support - always enabled
#include "uart_interface.h"
#include "uart_dma.h"

// Global objects
Config config;
UartStats uartStats = {0};
SystemState systemState = {0};
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
TaskHandle_t webServerTaskHandle = NULL;
TaskHandle_t device3TaskHandle = NULL;

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

  log_msg(config.device_name + " v" + config.device_version + " starting", LOG_INFO);

  // Initialize filesystem
  log_msg("Initializing LittleFS...", LOG_INFO);

  #ifdef FORMAT_FILESYSTEM
    log_msg("FORMAT_FILESYSTEM flag detected - formatting LittleFS...", LOG_WARNING);
    if (LittleFS.format()) {
      log_msg("LittleFS formatted successfully", LOG_INFO);
    } else {
      log_msg("LittleFS format failed", LOG_ERROR);
    }
  #endif

  if (!LittleFS.begin()) {
    log_msg("LittleFS mount failed, formatting...", LOG_WARNING);
    if (LittleFS.format()) {
      log_msg("LittleFS formatted successfully", LOG_INFO);
      if (LittleFS.begin()) {
        log_msg("LittleFS mounted after format", LOG_INFO);
      } else {
        log_msg("LittleFS mount failed even after format", LOG_ERROR);
        return;
      }
    } else {
      log_msg("LittleFS format failed", LOG_ERROR);
      return;
    }
  } else {
    log_msg("LittleFS mounted successfully", LOG_INFO);
  }

  // Load configuration from file (this may override defaults including usb_mode)
  log_msg("Loading configuration...", LOG_INFO);
  config_load(&config);
  log_msg("Configuration loaded", LOG_INFO);

  // Update global usbMode after loading config
  usbMode = config.usb_mode;
  
  // Initialize devices based on configuration
  initDevices();

  // Check for crash and log if needed
  crashlog_check_and_save();

  // Initialize hardware
  log_msg("Initializing pins...", LOG_INFO);
  initPins();
  leds_init();
  log_msg("Hardware initialized", LOG_INFO);

  // Detect boot mode
  log_msg("Detecting boot mode...", LOG_INFO);
  detectMode();
  log_msg("Mode detected: " + String(bridgeMode == BRIDGE_STANDALONE ? "Standalone" : "Network"), LOG_INFO);

  // Initialize TaskScheduler
  initializeScheduler();

  // Mode-specific initialization
  if (bridgeMode == BRIDGE_STANDALONE) {
    log_msg("Starting standalone mode - UART bridge active", LOG_INFO);
    log_msg("Use triple-click BOOT to enter network setup mode", LOG_INFO);
    log_msg("Blue LED will flash on data activity", LOG_INFO);
    initStandaloneMode();
    // Enable mode-specific tasks
    enableStandaloneTasks();
  } else if (bridgeMode == BRIDGE_NET) {
    log_msg("Starting network mode...", LOG_INFO);
    log_msg("Purple LED will stay ON during network mode", LOG_INFO);
    initNetworkMode();
    // Enable mode-specific tasks
    enableNetworkTasks(systemState.isTemporaryNetwork);
  }

  // Create FreeRTOS tasks
  createTasks();

  log_msg("Setup complete!", LOG_INFO);
}

void loop() {
  // Process LED updates from main thread - MUST be first
  led_process_updates();

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

  log_msg("BOOT button configured on GPIO" + String(BOOT_BUTTON_PIN), LOG_DEBUG);
}

void detectMode() {
  // Check for WiFi mode flag in preferences
  preferences.begin("uartbridge", false);
  bool wifiModeRequested = preferences.getBool("wifi_mode", false);
  preferences.end();

  if (wifiModeRequested) {
    log_msg("WiFi mode requested via preferences - entering network mode", LOG_INFO);
    // Clear the flag
    preferences.begin("uartbridge", false);
    preferences.putBool("wifi_mode", false);
    preferences.end();
    bridgeMode = BRIDGE_NET;
    systemState.isTemporaryNetwork = true;  // Setup AP is temporary
    return;
  }

  log_msg("Click count at startup: " + String(systemState.clickCount), LOG_DEBUG);

  // Note: On ESP32-S3, holding BOOT (GPIO0) during power-on will enter bootloader mode
  // and this code won't run at all.

  // Check for triple click (network mode)
  log_msg("Waiting for potential clicks...", LOG_DEBUG);
  vTaskDelay(pdMS_TO_TICKS(500));  // Wait for possible triple-click
  log_msg("Final click count: " + String(systemState.clickCount), LOG_DEBUG);

  if (systemState.clickCount >= WIFI_ACTIVATION_CLICKS) {
    log_msg("Triple click detected - entering network mode", LOG_INFO);
    bridgeMode = BRIDGE_NET;
    systemState.isTemporaryNetwork = true;  // Setup AP is temporary
    systemState.clickCount = 0;
    return;
  }

  log_msg("Entering standalone mode", LOG_INFO);
  bridgeMode = BRIDGE_STANDALONE;
}

void initStandaloneMode() {
  // Set LED mode for data flash explicitly
  led_set_mode(LED_MODE_DATA_FLASH);

  // Set global USB mode based on configuration
  usbMode = config.usb_mode;

  // Initialize UART DMA - always use DMA implementation
  if (!uartBridgeSerialDMA) {
    uartBridgeSerialDMA = new UartDMA(UART_NUM_1);
    uartBridgeSerial = uartBridgeSerialDMA;
    log_msg("UART DMA interface created", LOG_INFO);
  }

  // Create USB interface based on configuration (only if Device 2 is USB)
  if (config.device2.role == D2_USB) {
    switch(config.usb_mode) {
      case USB_MODE_HOST:
        log_msg("Creating USB Host interface", LOG_INFO);
        usbInterface = createUsbHost(config.baudrate);
        break;
      case USB_MODE_AUTO:
        log_msg("Creating USB Auto-detect interface", LOG_INFO);
        usbInterface = createUsbAuto(config.baudrate);
        break;
      case USB_MODE_DEVICE:
      default:
        log_msg("Creating USB Device interface", LOG_INFO);
        usbInterface = createUsbDevice(config.baudrate);
        break;
    }

    if (usbInterface) {
      usbInterface->init();
      log_msg("USB interface initialized", LOG_INFO);
    } else {
      log_msg("Failed to create USB interface", LOG_ERROR);
    }
  } else {
    log_msg("Device 2 is not configured for USB, skipping USB initialization", LOG_INFO);
  }

  // Initialize UART bridge
  initMainUART(uartBridgeSerial, &config, &uartStats, usbInterface);

  log_msg("UART Bridge ready - transparent forwarding active", LOG_INFO);
}

void initNetworkMode() {
  // Set LED mode for network setup
  led_set_mode(LED_MODE_WIFI_ON);

  // Use configured USB mode (from config file) - only if Device 2 is USB
  usbMode = config.usb_mode;

  // Initialize UART DMA - always use DMA implementation
  if (!uartBridgeSerialDMA) {
    uartBridgeSerialDMA = new UartDMA(UART_NUM_1);
    uartBridgeSerial = uartBridgeSerialDMA;
    log_msg("UART DMA interface created for network mode", LOG_INFO);
  }

  // Create USB interface even in network mode (for bridge functionality)
  if (config.device2.role == D2_USB) {
    switch(config.usb_mode) {
      case USB_MODE_HOST:
        log_msg("Creating USB Host interface in network mode", LOG_INFO);
        usbInterface = createUsbHost(config.baudrate);
        break;
      case USB_MODE_AUTO:
        log_msg("Creating USB Auto-detect interface in network mode", LOG_INFO);
        usbInterface = createUsbAuto(config.baudrate);
        break;
      case USB_MODE_DEVICE:
      default:
        log_msg("Creating USB Device interface in network mode", LOG_INFO);
        usbInterface = createUsbDevice(config.baudrate);
        break;
    }
    
    if (usbInterface) {
      usbInterface->init();
      log_msg("USB interface initialized in network mode", LOG_INFO);
    } else {
      log_msg("Failed to create USB interface in network mode", LOG_ERROR);
    }
  } else {
    log_msg("Device 2 is not configured for USB in network mode", LOG_INFO);
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
    log_msg("Click timeout expired, resetting clickCount from " +
            String(systemState.clickCount) + " to 0", LOG_DEBUG);
    systemState.clickCount = 0;
  }

  // Update LED feedback only when click count changes
  if (bridgeMode == BRIDGE_STANDALONE && systemState.clickCount != lastLedClickCount) {
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
      log_msg("Button held for 5 seconds - resetting WiFi to defaults", LOG_INFO);

      // Reset to default WiFi settings
      config.ssid = DEFAULT_AP_SSID;
      config.password = DEFAULT_AP_PASSWORD;
      config_save(&config);

      // Visual feedback
      led_rapid_blink(10, LED_WIFI_RESET_BLINK_MS);

      log_msg("WiFi reset to defaults: SSID=" + String(DEFAULT_AP_SSID) +
              ", Password=" + String(DEFAULT_AP_PASSWORD), LOG_INFO);
      log_msg("Restarting...", LOG_INFO);
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
            ", Diff: " + String(currentTime - systemState.lastClickTime), LOG_DEBUG);

    if (systemState.lastClickTime == 0 || (currentTime - systemState.lastClickTime < CLICK_TIMEOUT)) {
      systemState.clickCount = systemState.clickCount + 1;
      log_msg("Click registered, count: " + String(systemState.clickCount), LOG_DEBUG);

      // Check for triple click
      if (systemState.clickCount >= WIFI_ACTIVATION_CLICKS && bridgeMode == BRIDGE_STANDALONE) {
        log_msg("*** TRIPLE CLICK DETECTED! Activating Network Mode ***", LOG_INFO);
        log_msg("*** Setting WiFi mode flag and restarting ***", LOG_INFO);

        preferences.begin("uartbridge", false);
        preferences.putBool("wifi_mode", true);
        preferences.end();

        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP.restart();
      }
    } else {
      log_msg("Click timeout exceeded (" + String(currentTime - systemState.lastClickTime) +
              " ms), resetting to 1", LOG_DEBUG);
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

  log_msg("UART Bridge task created on core " + String(UART_TASK_CORE) + 
          " (priority " + String(UART_TASK_PRIORITY) + ")", LOG_INFO);

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
    
    log_msg("Device 3 task created on core " + String(DEVICE3_TASK_CORE) + 
            " for " + String(config.device3.role == D3_UART3_MIRROR ? "Mirror" : "Bridge") + " mode", LOG_INFO);
  }

  // Create web server task only in NETWORK mode on core 1
  if (bridgeMode == BRIDGE_NET) {
    xTaskCreatePinnedToCore(
      webServerTask,         // Task function
      "Web_Server_Task",     // Task name
      32768,                 // Stack size (increased from 16384)
      NULL,                  // Parameters
      WEB_TASK_PRIORITY,     // Priority
      &webServerTaskHandle,  // Task handle
      WEB_TASK_CORE          // Core 1 (separate core for web server)
    );

    log_msg("Web Server task created on core " + String(WEB_TASK_CORE) + 
            " (priority " + String(WEB_TASK_PRIORITY) + ")", LOG_INFO);
  }
}