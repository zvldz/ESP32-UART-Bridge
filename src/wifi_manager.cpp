#include "wifi_manager.h"
#include "logging.h"
#include "defines.h"
#include "leds.h"
#include <WiFi.h>
#include <esp_wifi.h>

// External objects
extern Config config;
extern SystemState systemState;

// Event group for network status
EventGroupHandle_t networkEventGroup = NULL;

// Internal state
static String targetSSID;
static String targetPassword;
static unsigned long lastScanTime = 0;
static unsigned long lastConnectAttempt = 0;
static bool scanInProgress = false;
static bool connectInProgress = false;

// Helper function to pause USB during WiFi initialization
static void pauseUsbForWiFiStart() {
    // External objects
    extern UsbInterface* usbInterface;
    
    if (config.device2.role == D2_USB && usbInterface) {
        log_msg("Temporarily pausing USB for WiFi startup", LOG_DEBUG);
        Serial.flush();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// WiFi event handler
void WiFiEvent(WiFiEvent_t event) {
    switch(event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            log_msg("WiFi connected to " + WiFi.SSID(), LOG_INFO);
            break;
            
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            log_msg("WiFi got IP: " + WiFi.localIP().toString(), LOG_INFO);
            systemState.wifiClientState = CLIENT_CONNECTED;
            systemState.wifiClientConnected = true;
            systemState.wifiRetryCount = 0;
            on_wifi_connected();
            break;
            
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            log_msg("WiFi disconnected", LOG_WARNING);
            
            // Check if we were previously connected (not wrong password case)
            if (systemState.wifiClientState == CLIENT_CONNECTED) {
                // We were connected - try to reconnect
                log_msg("Was connected, will attempt reconnection", LOG_INFO);
                systemState.wifiClientState = CLIENT_SCANNING;
                systemState.wifiRetryCount = 0;  // Reset retry counter
                lastScanTime = 0;  // Force immediate scan
                scanInProgress = false;
                connectInProgress = false;
            }
            // If it was wrong password, stay in that state
            else if (systemState.wifiClientState != CLIENT_WRONG_PASSWORD) {
                systemState.wifiClientState = CLIENT_IDLE;
            }
            
            systemState.wifiClientConnected = false;
            on_wifi_disconnected();
            break;
            
        case ARDUINO_EVENT_WIFI_SCAN_DONE:
            scanInProgress = false;
            int networksFound = WiFi.scanComplete();
            log_msg("WiFi scan complete, found " + String(networksFound) + " networks", LOG_DEBUG);
            
            // Check if target SSID was found
            bool targetFound = false;
            for (int i = 0; i < networksFound; i++) {
                if (WiFi.SSID(i) == targetSSID) {
                    targetFound = true;
                    break;
                }
            }
            
            if (targetFound && !connectInProgress) {
                // Network found, try to connect
                systemState.wifiClientState = CLIENT_CONNECTING;
                connectInProgress = true;
                lastConnectAttempt = millis();
                
                WiFi.begin(targetSSID.c_str(), targetPassword.c_str());
                log_msg("Attempting to connect to " + targetSSID, LOG_INFO);
            } else if (!targetFound) {
                // Network not found
                systemState.wifiClientState = CLIENT_NO_SSID;
                led_set_mode(LED_MODE_WIFI_CLIENT_SEARCHING);
            }
            
            WiFi.scanDelete();  // Free memory
            break;
    }
}

void wifi_manager_init() {
    // Create event group for network synchronization
    networkEventGroup = xEventGroupCreate();
    if (!networkEventGroup) {
        log_msg("Failed to create network event group", LOG_ERROR);
    }
    
    // Set WiFi event handler
    WiFi.onEvent(WiFiEvent);
    
    // Initialize state
    systemState.wifiClientState = CLIENT_IDLE;
    systemState.wifiClientConnected = false;
    systemState.wifiRetryCount = 0;
}

void wifi_manager_start_client(const String& ssid, const String& password) {
    pauseUsbForWiFiStart();  // Pause USB during WiFi init
    
    log_msg("Starting WiFi Client mode for SSID: " + ssid, LOG_INFO);
    
    targetSSID = ssid;
    targetPassword = password;
    
    // Configure WiFi with power limit to prevent brownout
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_5dBm);     // Limit power
    esp_wifi_set_max_tx_power(20);        // Backup power limit (critical for brownout prevention)
    WiFi.setAutoReconnect(false);  // We handle reconnection manually
    
    // Start with a scan
    systemState.wifiClientState = CLIENT_SCANNING;
    led_set_mode(LED_MODE_WIFI_CLIENT_SEARCHING);
    
    WiFi.scanNetworks(true);  // Async scan
    scanInProgress = true;
    lastScanTime = millis();
}

void wifi_manager_start_ap(const String& ssid, const String& password) {
    pauseUsbForWiFiStart();  // Pause USB during WiFi init
    
    log_msg("Starting WiFi AP mode: " + ssid, LOG_INFO);
    
    // Configure WiFi AP
    WiFi.mode(WIFI_AP);
    WiFi.setTxPower(WIFI_POWER_5dBm);
    esp_wifi_set_max_tx_power(20);        // Backup power limit (critical for brownout prevention)
    
    // Set static IP for AP mode
    IPAddress apIP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    
    WiFi.softAPConfig(apIP, gateway, subnet);
    WiFi.softAP(ssid.c_str(), password.c_str());
    
    log_msg("AP started with IP: " + WiFi.softAPIP().toString(), LOG_INFO);
}

void wifi_manager_stop() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

void wifi_manager_process() {
    // Only process in client mode
    if (systemState.wifiClientState == CLIENT_IDLE) return;
    
    unsigned long now = millis();
    
    // Handle connection timeout
    if (connectInProgress && (now - lastConnectAttempt > 10000)) {
        log_msg("Connection attempt timed out", LOG_WARNING);
        WiFi.disconnect();
        
        systemState.wifiRetryCount++;
        
        if (systemState.wifiRetryCount >= WIFI_CLIENT_MAX_RETRIES) {
            // Too many failures - wrong password
            log_msg("Max retries reached - assuming wrong password", LOG_ERROR);
            systemState.wifiClientState = CLIENT_WRONG_PASSWORD;
            led_set_mode(LED_MODE_WIFI_CLIENT_ERROR);
            connectInProgress = false;
            return;  // Stop trying
        }
        
        connectInProgress = false;
    }
    
    // Handle periodic scanning
    if (!scanInProgress && !connectInProgress && 
        systemState.wifiClientState != CLIENT_WRONG_PASSWORD &&
        systemState.wifiClientState != CLIENT_CONNECTED &&
        (now - lastScanTime > WIFI_CLIENT_SCAN_INTERVAL_MS)) {
        
        log_msg("Starting periodic WiFi scan", LOG_DEBUG);
        WiFi.scanNetworks(true);
        scanInProgress = true;
        lastScanTime = now;
    }
}

bool wifi_manager_is_connected() {
    return systemState.wifiClientConnected;
}

int wifi_manager_get_rssi() {
    if (systemState.wifiClientConnected) {
        return WiFi.RSSI();
    }
    return 0;
}

String wifi_manager_get_ip() {
    if (systemState.wifiClientConnected) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

WiFiClientState wifi_manager_get_state() {
    return systemState.wifiClientState;
}

int rssi_to_percent(int rssi) {
    if (rssi >= WIFI_RSSI_EXCELLENT) return 100;
    if (rssi <= WIFI_RSSI_POOR) return 0;
    return (rssi - WIFI_RSSI_POOR) * 100 / (WIFI_RSSI_EXCELLENT - WIFI_RSSI_POOR);
}