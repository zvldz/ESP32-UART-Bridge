#include "wifi_manager.h"
#include "logging.h"
#include "defines.h"
#include "leds.h"
#include "diagnostics.h"
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
static bool wasConnectedBefore = false;  // Track if we were ever connected
static bool targetNetworkFound = false;  // Track if target network was found in last scan
static int scanFailureCount = 0;  // Track consecutive scan failures

// Get human-readable WiFi disconnect reason
static String getWiFiDisconnectReason(uint8_t reason) {
    switch(reason) {
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
            return "Authentication failed (wrong password)";
        case WIFI_REASON_NO_AP_FOUND:
            return "Network not found";
        case WIFI_REASON_ASSOC_FAIL:
            return "Association rejected (MAC filter or connection limit)";
        case WIFI_REASON_BEACON_TIMEOUT:
            return "Lost connection (beacon timeout)";
        case WIFI_REASON_AUTH_EXPIRE:
            return "Authentication expired";
        default:
            return "Unknown reason (" + String(reason) + ")";
    }
}

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
void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch(event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            log_msg("WiFi connected to " + WiFi.SSID(), LOG_INFO);
            break;
            
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            log_msg("WiFi got IP: " + WiFi.localIP().toString(), LOG_INFO);
            
            // CRITICAL: Force disable AP immediately after connection
            WiFi.softAPdisconnect(true);
            WiFi.enableAP(false);
            esp_wifi_set_mode(WIFI_MODE_STA);
            
            systemState.wifiClientState = CLIENT_CONNECTED;
            systemState.wifiClientConnected = true;
            systemState.wifiRetryCount = 0;        // Reset retry counter
            scanFailureCount = 0;                  // Reset scan failure counter
            wasConnectedBefore = true;             // Mark successful connection
            targetNetworkFound = true;             // Network is obviously found
            connectInProgress = false;             // CRITICAL: Stop connection timeout timer
            on_wifi_connected();
            break;
            
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            {
                // Get disconnect reason from event info
                uint8_t reason = info.wifi_sta_disconnected.reason;
                String reasonStr = getWiFiDisconnectReason(reason);
                log_msg("WiFi disconnected: " + reasonStr, LOG_WARNING);
                
                // Check if this is an authentication failure
                bool isAuthError = (reason == WIFI_REASON_AUTH_FAIL || 
                                   reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                                   reason == WIFI_REASON_AUTH_EXPIRE);
                
                // Handle based on current state and history
                if (systemState.wifiClientState == CLIENT_CONNECTED) {
                    // We were connected - try to reconnect
                    log_msg("Was connected, will attempt reconnection", LOG_INFO);
                    systemState.wifiClientState = CLIENT_SCANNING;
                    systemState.wifiRetryCount = 0;  // Reset retry counter for reconnection
                    lastScanTime = 0;  // Force immediate scan
                    targetNetworkFound = false;  // Need to find network again
                    scanInProgress = false;
                    connectInProgress = false;
                }
                else if (connectInProgress && systemState.wifiClientState != CLIENT_WRONG_PASSWORD) {
                    connectInProgress = false;
                    systemState.wifiRetryCount++;
                    
                    log_msg("Connection attempt #" + String(systemState.wifiRetryCount) + 
                            " failed: " + reasonStr, LOG_DEBUG);
                    
                    // Check if we should stop trying - ONLY for authentication failures
                    if (!wasConnectedBefore && isAuthError && 
                        systemState.wifiRetryCount >= WIFI_CLIENT_MAX_RETRIES) {
                        // Authentication error on initial connection - wrong password
                        log_msg("Max authentication failures reached - wrong password", LOG_WARNING);
                        systemState.wifiClientState = CLIENT_WRONG_PASSWORD;
                        led_set_mode(LED_MODE_WIFI_CLIENT_ERROR);
                        targetNetworkFound = false;
                    }
                    else if (targetNetworkFound && systemState.wifiRetryCount < WIFI_CLIENT_MAX_RETRIES) {
                        // Network is still available, try again after a short delay
                        log_msg("Retrying connection in 500ms...", LOG_DEBUG);
                        
                        // Schedule immediate retry
                        vTaskDelay(pdMS_TO_TICKS(500));  // Brief delay for WiFi stack
                        
                        // Force STA mode to prevent automatic AP_STA switching
                        WiFi.mode(WIFI_STA);
                        
                        systemState.wifiClientState = CLIENT_CONNECTING;
                        connectInProgress = true;
                        lastConnectAttempt = millis();
                        
                        WiFi.begin(targetSSID.c_str(), targetPassword.c_str());
                        log_msg("Retry attempt #" + String(systemState.wifiRetryCount + 1) + 
                                " to " + targetSSID, LOG_INFO);
                    }
                    else {
                        // Network not found or too many retries - need to scan
                        systemState.wifiClientState = CLIENT_SCANNING;
                        lastScanTime = 0;  // Force immediate scan
                        scanInProgress = false;
                    }
                }
                
                systemState.wifiClientConnected = false;
                on_wifi_disconnected();
            }
            break;
            
        case ARDUINO_EVENT_WIFI_SCAN_DONE:
            {
                scanInProgress = false;
                int networksFound = WiFi.scanComplete();
                log_msg("WiFi scan complete, found " + String(networksFound) + " networks", LOG_DEBUG);
                
                // Check if target SSID was found
                bool foundNow = false;
                for (int i = 0; i < networksFound; i++) {
                    if (WiFi.SSID(i) == targetSSID) {
                        foundNow = true;
                        targetNetworkFound = true;  // Mark as found
                        break;
                    }
                }
                
                if (foundNow && !connectInProgress && 
                    systemState.wifiClientState != CLIENT_WRONG_PASSWORD) {
                    // Network found, try to connect
                    systemState.wifiClientState = CLIENT_CONNECTING;
                    connectInProgress = true;
                    lastConnectAttempt = millis();
                    
                    log_msg("Target network found, attempting connection #" + 
                            String(systemState.wifiRetryCount + 1), LOG_INFO);
                    
                    WiFi.begin(targetSSID.c_str(), targetPassword.c_str());
                } 
                else if (!foundNow) {
                    // Network not found
                    targetNetworkFound = false;  // Only reset when actually not found
                    systemState.wifiClientState = CLIENT_NO_SSID;
                    led_set_mode(LED_MODE_WIFI_CLIENT_SEARCHING);
                    log_msg("Target network '" + targetSSID + "' not found", LOG_DEBUG);
                }
                
                WiFi.scanDelete();  // Free memory
            }
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
    
    // Reset connection tracking
    wasConnectedBefore = false;
    
    // CRITICAL: Force STA-only mode via ESP-IDF (bypass Arduino framework)
    esp_wifi_stop();                           // Stop WiFi completely
    esp_wifi_set_mode(WIFI_MODE_NULL);         // Clear any mode
    vTaskDelay(pdMS_TO_TICKS(500));            // Let WiFi fully stop
    
    esp_wifi_set_mode(WIFI_MODE_STA);          // Set STA mode via ESP-IDF
    esp_wifi_start();                          // Restart WiFi in STA mode
    
    // Now configure Arduino layer
    WiFi.mode(WIFI_STA);                       // Sync Arduino API
    WiFi.softAPdisconnect(true);               // Ensure AP is disabled
    WiFi.enableAP(false);                      // Double-check AP disable
    
    WiFi.setTxPower(WIFI_POWER_5dBm);     // Limit power
    esp_wifi_set_max_tx_power(20);        // Backup power limit (critical for brownout prevention)
    WiFi.setAutoReconnect(false);  // We handle reconnection manually
    
    // Verify that mode is set correctly
    wifi_mode_t currentMode;
    esp_wifi_get_mode(&currentMode);
    log_msg("WiFi mode set to: " + String(currentMode) + " (1=STA, 2=AP, 3=APSTA)", LOG_DEBUG);
    
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
    // Only process in client mode, skip when connected or inactive
    if (systemState.wifiClientState == CLIENT_IDLE || 
        systemState.wifiClientState == CLIENT_WRONG_PASSWORD ||
        systemState.wifiClientState == CLIENT_CONNECTED) return;
    
    unsigned long now = millis();
    
    // Handle connection timeout
    if (connectInProgress && (now - lastConnectAttempt > 10000)) {
        log_msg("Connection attempt timed out", LOG_WARNING);
        WiFi.disconnect();
        // Let the disconnect event handler manage the retry logic
    }
    
    // Handle periodic scanning
    if (!scanInProgress && !connectInProgress && 
        systemState.wifiClientState != CLIENT_CONNECTED &&
        (now - lastScanTime > WIFI_CLIENT_SCAN_INTERVAL_MS)) {
        log_msg("Starting periodic WiFi scan", LOG_DEBUG);
        
        int scanResult = WiFi.scanNetworks(true);
        if (scanResult == WIFI_SCAN_RUNNING) {
            scanInProgress = true;
            lastScanTime = now;
            scanFailureCount = 0;  // Reset failure counter on success
        } else {
            // Scan failed to start
            scanFailureCount++;
            log_msg("WiFi scan failed to start (attempt " + String(scanFailureCount) + 
                    "), result: " + String(scanResult), LOG_WARNING);
            
            if (scanFailureCount >= 10) {
                // Too many failures - try to recover
                log_msg("Too many scan failures, attempting WiFi reset", LOG_WARNING);
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);
                vTaskDelay(pdMS_TO_TICKS(1000));
                WiFi.mode(WIFI_STA);
                scanFailureCount = 0;
                
                // If still failing after reset, reboot might be needed
                if (scanFailureCount >= 20) {
                    log_msg("WiFi subsystem unrecoverable, rebooting...", LOG_ERROR);
                    ESP.restart();
                }
            }
            
            // Retry scan after 1 second
            lastScanTime = now - (WIFI_CLIENT_SCAN_INTERVAL_MS - 1000);
        }
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