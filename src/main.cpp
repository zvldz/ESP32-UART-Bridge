#include <Arduino.h>
#include <Preferences.h>
#include <LittleFS.h>
#include "defines.h"
#include "types.h"
#include "device_stats.h"
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
#include "device_init.h"
#include "wifi/wifi_manager.h"
#include "uart/uart_interface.h"
#include "uart/uart_dma.h"
#include "protocols/udp_sender.h"
#include "circular_buffer.h"
#include <AsyncUDP.h>

// Global objects
Config config;
SystemState systemState = {0};  // All fields initialized to 0/false
BridgeMode bridgeMode = BRIDGE_STANDALONE;
Preferences preferences;

// UART serial objects - always DMA
static UartDMA* uartBridgeSerialDMA = nullptr;
UartInterface* uartBridgeSerial = nullptr;

// Global USB mode variable
UsbMode usbMode = USB_MODE_DEVICE;

// Task handles
TaskHandle_t uartBridgeTaskHandle = NULL;

// USB interface
UsbInterface* usbInterface = NULL;

// UDP transport for Device4
AsyncUDP* udpTransport = nullptr;
CircularBuffer* udpRxBuffer = nullptr;

// Mutexes for thread safety
SemaphoreHandle_t logMutex = NULL;

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

    // Initialize device statistics
    initDeviceStatistics();

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

    // Check for crash and log if needed (MUST be early, before most initialization)
    crashlog_check_and_save();

    // Load configuration from file (this may override defaults including usb_mode)
    log_msg(LOG_INFO, "Loading configuration...");
    config_load(&config);
    log_msg(LOG_INFO, "Configuration loaded");

    // Validate SBUS configuration (block incompatible combinations)
    if (config.device1.role == D1_SBUS_IN) {
        // Block D1_SBUS_IN + D2_USB (requires SBUS→UART converter not implemented)
        if (config.device2.role == D2_USB) {
            log_msg(LOG_ERROR, "Configuration error: D1_SBUS_IN → D2_USB not supported");
            log_msg(LOG_ERROR, "SBUS→USB requires protocol converter (not implemented)");
            log_msg(LOG_INFO, "Please use D2_SBUS_OUT for native SBUS output");
            // Reset to safe defaults
            config.device2.role = D2_NONE;
            config_save(&config);
        }
        // Block D1_SBUS_IN + UART Bridge roles
        if (config.device2.role == D2_UART2 || config.device3.role == D3_UART3_BRIDGE) {
            log_msg(LOG_ERROR, "Configuration error: SBUS→UART conversion not implemented");
            log_msg(LOG_INFO, "Use SBUS_OUT roles for native SBUS transmission");
            // Reset problematic devices
            if (config.device2.role == D2_UART2) config.device2.role = D2_NONE;
            if (config.device3.role == D3_UART3_BRIDGE) config.device3.role = D3_NONE;
            config_save(&config);
        }
    }

    // Auto-detect protocol optimization based on device roles
    bool hasSBUSDevice = (config.device1.role == D1_SBUS_IN ||
                         config.device2.role == D2_SBUS_IN || config.device2.role == D2_SBUS_OUT ||
                         config.device3.role == D3_SBUS_OUT);

    if (hasSBUSDevice) {
        if (config.protocolOptimization != PROTOCOL_SBUS) {
            config.protocolOptimization = PROTOCOL_SBUS;
            log_msg(LOG_INFO, "Auto-detected SBUS devices, forcing protocol optimization to SBUS");
            config_save(&config);  // Save the auto-detected setting
        }
    } else {
        if (config.protocolOptimization == PROTOCOL_SBUS) {
            config.protocolOptimization = PROTOCOL_NONE;
            log_msg(LOG_INFO, "No SBUS devices found, resetting protocol optimization to None");
            config_save(&config);  // Save the corrected setting
        }
    }

    // Update global usbMode after loading config
    usbMode = config.usb_mode;

    // Initialize devices based on configuration
    initDevices();

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
//  log_msg(LOG_INFO, "Use triple-click BOOT to enter network setup mode");
//  log_msg(LOG_INFO, "Blue LED will flash on data activity");
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

    // NOTE: registerSbusOutputs() now called in uartBridgeTask after pipeline init

    // Create FreeRTOS tasks
    createTasks();

    log_msg(LOG_INFO, "Setup complete!");
}

void loop() {
    // Process LED updates from main thread - MUST be first
    led_process_updates();

    // Process WiFi Manager in network mode
    if (bridgeMode == BRIDGE_NET) {
        wifiProcess();
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

// Common initialization for both Standalone and Network modes
void initCommonDevices() {
    // Set global USB mode based on configuration
    usbMode = config.usb_mode;

    // Initialize UART DMA - always use DMA implementation
    if (!uartBridgeSerialDMA) {
        if (config.device1.role == D1_SBUS_IN) {
            // CRITICAL: Special DMA configuration for SBUS
            UartDMA::DmaConfig dmaCfg = {
                .useEventTask = false,     // Polling mode for SBUS
                .dmaRxBufSize = 512,       // Minimal RX buffer for SBUS
                .dmaTxBufSize = 0,         // NO TX BUFFER - critical for memory saving!
                .ringBufSize = 1024,       // Small ring buffer
                .eventTaskPriority = 0,    // Not used in polling mode
                .eventQueueSize = 10       // Minimal queue
            };

            uartBridgeSerialDMA = new UartDMA(UART_NUM_1, dmaCfg);
            uartBridgeSerial = uartBridgeSerialDMA;

            log_msg(LOG_INFO, "Device1 SBUS_IN: Special DMA config (no TX, minimal buffers)");
        } else {
            // Normal DMA configuration for UART bridge
            UartDMA::DmaConfig dmaCfg = UartDMA::getDefaultDmaConfig();
            uartBridgeSerialDMA = new UartDMA(UART_NUM_1, dmaCfg);
            uartBridgeSerial = uartBridgeSerialDMA;
            log_msg(LOG_INFO, "UART DMA interface created");
        }
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
    initMainUART(uartBridgeSerial, &config, usbInterface);
}

void initStandaloneMode() {
    // Set LED mode for data flash explicitly
    led_set_mode(LED_MODE_DATA_FLASH);

    // Ensure network is not active in standalone mode
    systemState.networkActive = false;

    // Initialize common devices (UART DMA, USB, Device1)
    initCommonDevices();

    log_msg(LOG_INFO, "UART Bridge ready - transparent forwarding active");
}

void initNetworkMode() {
    // Initialize WiFi Manager
    esp_err_t ret = wifiInit();

    if (ret != ESP_OK && config.device4.role != D4_NONE) {
        // Critical error only if Device 4 is enabled
        log_msg(LOG_ERROR, "Failed to init WiFi, entering safe mode");
        systemState.wifiSafeMode = true;
        led_set_mode(LED_MODE_SAFE_MODE);
        return;
    } else if (ret != ESP_OK) {
        // Device 4 disabled - continue without WiFi
        log_msg(LOG_WARNING, "WiFi init failed, but Device 4 disabled - continuing");
    }

    // Start WiFi in appropriate mode
    if (systemState.tempForceApMode) {
        log_msg(LOG_INFO, "Starting temporary WiFi AP mode (forced by triple click)");
        wifiStartAP(config.ssid, config.password);
        led_set_mode(LED_MODE_WIFI_ON);
        systemState.tempForceApMode = false;
    } else if (config.wifi_mode == BRIDGE_WIFI_MODE_CLIENT) {
        log_msg(LOG_INFO, "Starting WiFi Client mode");
        wifiStartClient(config.wifi_client_ssid, config.wifi_client_password);
    } else {
        log_msg(LOG_INFO, "Starting WiFi AP mode");
        wifiStartAP(config.ssid, config.password);
        led_set_mode(LED_MODE_WIFI_ON);
    }

    // Set network as active
    systemState.networkActive = true;

    // Initialize common devices (UART DMA, USB, Device1)
    initCommonDevices();

    // Initialize UDP transport for Device4
    if (config.device4.role == D4_NETWORK_BRIDGE ||
        config.device4.role == D4_LOG_NETWORK ||
        config.device4.role == D4_SBUS_UDP_TX ||
        config.device4.role == D4_SBUS_UDP_RX) {

        udpTransport = new AsyncUDP();

        if (udpTransport) {
            // Create UDP RX buffer for Bridge and SBUS RX modes
            if (config.device4.role == D4_NETWORK_BRIDGE ||
                config.device4.role == D4_SBUS_UDP_RX) {

                udpRxBuffer = new CircularBuffer();
                udpRxBuffer->init(4096);

                // Setup UDP listener
                if (!udpTransport->listen(config.device4_config.port)) {
                    log_msg(LOG_ERROR, "Failed to listen on UDP port %d", config.device4_config.port);
                } else {
                    log_msg(LOG_INFO, "UDP listening on port %d with 4KB buffer", config.device4_config.port);

                    // Setup callback based on role and protocol
                    if (config.device4.role == D4_SBUS_UDP_RX || config.protocolOptimization == PROTOCOL_SBUS) {
                        // SBUS mode - filter for valid SBUS frames only
                        udpTransport->onPacket([](AsyncUDPPacket packet) {
                            if (udpRxBuffer) {
                                size_t len = packet.length();

                                if (len == 25) {
                                    // Single frame - validate
                                    if (packet.data()[0] == 0x0F) {
                                        udpRxBuffer->write(packet.data(), 25);
                                        g_deviceStats.device4.rxPackets.fetch_add(1, std::memory_order_relaxed);
                                    }
                                } else if (len == 75) {
                                    // Three frames batched (3x25) - validate all
                                    if (packet.data()[0] == 0x0F && packet.data()[25] == 0x0F && packet.data()[50] == 0x0F) {
                                        udpRxBuffer->write(packet.data(), 75);
                                        g_deviceStats.device4.rxPackets.fetch_add(3, std::memory_order_relaxed);
                                    }
                                } else if (len == 50) {
                                    // Two frames - validate both
                                    if (packet.data()[0] == 0x0F && packet.data()[25] == 0x0F) {
                                        udpRxBuffer->write(packet.data(), 50);
                                        g_deviceStats.device4.rxPackets.fetch_add(2, std::memory_order_relaxed);
                                    }
                                }
                                // Ignore other sizes in SBUS mode
                            }
                        });
                        log_msg(LOG_INFO, "UDP callback configured for SBUS protocol (filtering enabled)");
                    } else {
                        // RAW/MAVLink mode - accept all packets
                        udpTransport->onPacket([](AsyncUDPPacket packet) {
                            if (udpRxBuffer) {
                                udpRxBuffer->write(packet.data(), packet.length());
                                g_deviceStats.device4.rxPackets.fetch_add(1, std::memory_order_relaxed);
                            }
                        });
                        log_msg(LOG_INFO, "UDP callback configured for RAW/MAVLink protocol (no filtering)");
                    }
                }
            } else {
                // Logger or SBUS TX mode - TX only
                log_msg(LOG_INFO, "UDP transport created for TX only mode");
            }
        } else {
            log_msg(LOG_ERROR, "Failed to create UDP transport");
        }
    }

    // Initialize web server
    webserver_init(&config, &systemState);
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
          log_msg(LOG_INFO, "*** TRIPLE CLICK: Standalone -> Saved WiFi Mode ***");
          
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
          log_msg(LOG_INFO, "*** TRIPLE CLICK: Client Mode -> Force AP Mode ***");
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

    // UDP log mutex is now created in logging_init()
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

    // ADD: Check if we need sender task
    bool needSenderTask = false;

    // Check all possible sender configurations
    if (config.device2.role == D2_USB ||            // USB sender
        config.device2.role == D2_UART2 ||          // UART2 sender
        config.device2.role == D2_SBUS_IN ||        // SBUS input
        config.device2.role == D2_SBUS_OUT ||       // SBUS output
        config.device3.role == D3_UART3_MIRROR ||   // UART3 mirror
        config.device3.role == D3_UART3_BRIDGE ||   // UART3 bridge
        // D3_SBUS_IN removed - not supported
        config.device3.role == D3_SBUS_OUT ||       // SBUS output
        config.device4.role == D4_NETWORK_BRIDGE || // UDP bridge
        config.device4.role == D4_LOG_NETWORK) {    // UDP logger
        needSenderTask = true;
    }

    // Create sender task only if needed
    if (needSenderTask) {
        TaskHandle_t senderTaskHandle;
        xTaskCreatePinnedToCore(
            senderTask,                      // Function
            "sender_task",                   // Name
            4096,                            // Stack size
            NULL,                            // Parameter
            UART_TASK_PRIORITY - 2,          // Priority (lower than main UART task)
            &senderTaskHandle,               // Handle
            UART_TASK_CORE                   // Same core as UART task
        );

        log_msg(LOG_INFO, "Sender task created on core %d (priority %d)",
                UART_TASK_CORE, UART_TASK_PRIORITY - 2);
    } else {
        log_msg(LOG_INFO, "No senders configured, sender task not created");
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