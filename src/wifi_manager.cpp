// ESP-IDF headers instead of Arduino
#include "wifi_manager.h"
#include "logging.h"
#include "defines.h"
#include "leds.h"
#include "diagnostics.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "mdns.h"
#include <string.h>
#include <DNSServer.h>

// External objects
extern Config config;
extern SystemState systemState;

// ESP-IDF global variables
static esp_netif_t* sta_netif = NULL;
static esp_netif_t* ap_netif = NULL;
static bool wifi_initialized = false;

// Event group for network status
EventGroupHandle_t networkEventGroup = NULL;

// Error counters for bootloop protection
RTC_DATA_ATTR int wifiInitFailCount = 0;

// DNS server for captive portal (moved from web_interface.cpp)
DNSServer* dnsServer = nullptr;

// Internal state for client mode
static String targetSSID;
static String targetPassword;
static bool mdnsInitNeeded = false;
static String clientIP;

// Helper function to create valid mDNS hostname with MAC suffix
String generateMdnsHostname() {
    String hostname = config.device_name;
    hostname.toLowerCase();
    hostname.replace(" ", "-");
    hostname.replace("_", "-");
    // Ensure it starts with a letter and contains only valid characters
    for (int i = 0; i < hostname.length(); i++) {
        char c = hostname.charAt(i);
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
            hostname.setCharAt(i, '-');
        }
    }
    
    // Add last 2 bytes of MAC address for uniqueness
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    char macSuffix[6];
    sprintf(macSuffix, "-%02x%02x", mac[4], mac[5]);
    hostname += String(macSuffix);
    
    return hostname;
}

// Initialize mDNS safely outside of event handler
static void initMdnsService() {
    if (!mdnsInitNeeded) return;
    
    
    esp_err_t mdns_ret = mdns_init();
    if (mdns_ret == ESP_OK) {
        String hostname = generateMdnsHostname();
        
        // Follow ESP-IDF documentation pattern exactly
        esp_err_t hostname_ret = mdns_hostname_set(hostname.c_str());
        esp_err_t instance_ret = mdns_instance_name_set(config.device_name.c_str());
        esp_err_t service_ret = mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
        
        log_msg(LOG_INFO, "mDNS initialized: %s.local (%s)", hostname.c_str(), config.device_name.c_str());
        
        if (hostname_ret != ESP_OK) log_msg(LOG_WARNING, "mDNS hostname set failed: %s", esp_err_to_name(hostname_ret));
        if (instance_ret != ESP_OK) log_msg(LOG_WARNING, "mDNS instance set failed: %s", esp_err_to_name(instance_ret));
        if (service_ret != ESP_OK) log_msg(LOG_WARNING, "mDNS service add failed: %s", esp_err_to_name(service_ret));
    } else {
        log_msg(LOG_WARNING, "Failed to initialize mDNS: %s", esp_err_to_name(mdns_ret));
    }
    
    mdnsInitNeeded = false;  // Reset flag
}
static unsigned long lastScanTime = 0;
static unsigned long lastConnectAttempt = 0;
static bool scanInProgress = false;
static bool connectInProgress = false;
static bool wasConnectedBefore = false;
static bool targetNetworkFound = false;
static int scanFailureCount = 0;

// Static event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                log_msg(LOG_DEBUG, "WiFi STA started");
                esp_wifi_connect();
                break;
                
            case WIFI_EVENT_STA_CONNECTED:
                {
                    wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*)event_data;
                    log_msg(LOG_INFO, "WiFi connected to %s", (char*)event->ssid);
                }
                break;
                
            case WIFI_EVENT_STA_DISCONNECTED:
                {
                    wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*)event_data;
                    String reasonStr = "Disconnect reason: " + String(event->reason);
                    log_msg(LOG_WARNING, "WiFi disconnected: Disconnect reason: %d", event->reason);
                    
                    // Check if this is an authentication failure
                    bool isAuthError = (event->reason == WIFI_REASON_AUTH_FAIL || 
                                       event->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                                       event->reason == WIFI_REASON_AUTH_EXPIRE);
                    
                    // Handle based on current state and history
                    if (systemState.wifiClientState == CLIENT_CONNECTED) {
                        // We were connected - try to reconnect
                        log_msg(LOG_INFO, "Was connected, will attempt reconnection");
                        systemState.wifiClientState = CLIENT_SCANNING;
                        systemState.wifiRetryCount = 0;
                        lastScanTime = 0;
                        targetNetworkFound = false;
                        scanInProgress = false;
                        connectInProgress = false;
                    }
                    else if (connectInProgress && systemState.wifiClientState != CLIENT_WRONG_PASSWORD) {
                        connectInProgress = false;
                        systemState.wifiRetryCount++;
                        
                        log_msg(LOG_DEBUG, "Connection attempt #%d failed", systemState.wifiRetryCount);
                        
                        // Check if we should stop trying - ONLY for authentication failures
                        if (!wasConnectedBefore && isAuthError && 
                            systemState.wifiRetryCount >= WIFI_CLIENT_MAX_RETRIES) {
                            // Authentication error on initial connection - wrong password
                            log_msg(LOG_WARNING, "Max authentication failures reached - wrong password");
                            systemState.wifiClientState = CLIENT_WRONG_PASSWORD;
                            led_set_mode(LED_MODE_WIFI_CLIENT_ERROR);
                            targetNetworkFound = false;
                        }
                        else if (targetNetworkFound && systemState.wifiRetryCount < WIFI_CLIENT_MAX_RETRIES) {
                            // Network is still available, try again after a short delay
                            log_msg(LOG_DEBUG, "Retrying connection in 500ms...");
                            
                            vTaskDelay(pdMS_TO_TICKS(500));
                            
                            systemState.wifiClientState = CLIENT_CONNECTING;
                            connectInProgress = true;
                            lastConnectAttempt = millis();
                            
                            esp_wifi_connect();
                            log_msg(LOG_INFO, "Retry attempt #%d to %s", systemState.wifiRetryCount + 1, targetSSID.c_str());
                        }
                        else {
                            // Network not found or too many retries - need to scan
                            systemState.wifiClientState = CLIENT_SCANNING;
                            lastScanTime = 0;
                            scanInProgress = false;
                        }
                    }
                    
                    systemState.wifiClientConnected = false;
                    xEventGroupClearBits(networkEventGroup, NETWORK_CONNECTED_BIT);
                    
                    // Clean up mDNS when disconnected
                    mdns_free();
                    log_msg(LOG_DEBUG, "mDNS freed on disconnect");
                }
                break;
                
            case WIFI_EVENT_SCAN_DONE:
                {
                    scanInProgress = false;
                    uint16_t networksFound = 0;
                    esp_wifi_scan_get_ap_num(&networksFound);
                    log_msg(LOG_DEBUG, "WiFi scan complete, found %d networks", networksFound);
                    
                    // Check if target SSID was found
                    bool foundNow = false;
                    if (networksFound > 0) {
                        wifi_ap_record_t* ap_records = (wifi_ap_record_t*)malloc(networksFound * sizeof(wifi_ap_record_t));
                        if (ap_records) {
                            esp_wifi_scan_get_ap_records(&networksFound, ap_records);
                            
                            for (int i = 0; i < networksFound; i++) {
                                if (strcmp((char*)ap_records[i].ssid, targetSSID.c_str()) == 0) {
                                    foundNow = true;
                                    targetNetworkFound = true;
                                    break;
                                }
                            }
                            
                            free(ap_records);
                        }
                    }
                    
                    if (foundNow && !connectInProgress && 
                        systemState.wifiClientState != CLIENT_WRONG_PASSWORD) {
                        // Network found, try to connect
                        systemState.wifiClientState = CLIENT_CONNECTING;
                        connectInProgress = true;
                        lastConnectAttempt = millis();
                        
                        log_msg(LOG_INFO, "Target network found, attempting connection #%d", systemState.wifiRetryCount + 1);
                        
                        esp_wifi_connect();
                    } 
                    else if (!foundNow) {
                        // Network not found
                        targetNetworkFound = false;
                        systemState.wifiClientState = CLIENT_NO_SSID;
                        led_set_mode(LED_MODE_WIFI_CLIENT_SEARCHING);
                        log_msg(LOG_DEBUG, "Target network '%s' not found", targetSSID.c_str());
                    }
                }
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
                {
                    ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
                    char ip_str[16];
                    sprintf(ip_str, IPSTR, IP2STR(&event->ip_info.ip));
                    log_msg(LOG_INFO, "WiFi got IP: %s", ip_str);
                    
                    systemState.wifiClientState = CLIENT_CONNECTED;
                    systemState.wifiClientConnected = true;
                    systemState.wifiRetryCount = 0;
                    scanFailureCount = 0;
                    wasConnectedBefore = true;
                    targetNetworkFound = true;
                    connectInProgress = false;
                    
                    // Set flag and IP to initialize mDNS outside of event handler  
                    mdnsInitNeeded = true;
                    clientIP = String(ip_str);
                    systemState.wifiClientConnected = true;
                    
                    // Set network connected bit
                    xEventGroupSetBits(networkEventGroup, NETWORK_CONNECTED_BIT);
                    led_set_mode(LED_MODE_WIFI_CLIENT_CONNECTED);
                }
                break;
        }
    }
}

esp_err_t wifi_manager_init() {
    // Check safe mode (Device 4)
    if (config.device4.role != D4_NONE && wifiInitFailCount >= 3) {
        log_msg(LOG_WARNING, "WiFi in safe mode after 3 failures");
        systemState.wifiSafeMode = true;
        return ESP_FAIL;
    }
    
    if (wifi_initialized) {
        return ESP_OK;
    }
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    // Check memory
    if (ESP.getFreeHeap() < 30000) {
        log_msg(LOG_ERROR, "Not enough heap for WiFi: %d", ESP.getFreeHeap());
        wifiInitFailCount++;
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize TCP/IP
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        log_msg(LOG_ERROR, "Failed to init netif: %s", esp_err_to_name(ret));
        wifiInitFailCount++;
        return ret;
    }
    
    // Event loop
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        log_msg(LOG_ERROR, "Failed to create event loop: %s", esp_err_to_name(ret));
        wifiInitFailCount++;
        return ret;
    }
    
    // Create network interfaces
    sta_netif = esp_netif_create_default_wifi_sta();
    ap_netif = esp_netif_create_default_wifi_ap();
    
    if (!sta_netif || !ap_netif) {
        log_msg(LOG_ERROR, "Failed to create netif interfaces");
        wifiInitFailCount++;
        return ESP_ERR_NO_MEM;
    }
    
    // WiFi configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    
    if (ret != ESP_OK) {
        log_msg(LOG_ERROR, "Failed to init WiFi: %s", esp_err_to_name(ret));
        wifiInitFailCount++;
        return ret;
    }
    
    // Register event handlers
    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        log_msg(LOG_ERROR, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        wifiInitFailCount++;
        return ret;
    }
    
    ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        log_msg(LOG_ERROR, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        wifiInitFailCount++;
        return ret;
    }
    
    // Create event group for network synchronization
    networkEventGroup = xEventGroupCreate();
    if (!networkEventGroup) {
        log_msg(LOG_ERROR, "Failed to create network event group");
        wifiInitFailCount++;
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize client state
    systemState.wifiClientState = CLIENT_IDLE;
    systemState.wifiClientConnected = false;
    systemState.wifiRetryCount = 0;
    
    wifi_initialized = true;
    wifiInitFailCount = 0;  // Success - reset counter
    
    log_msg(LOG_INFO, "WiFi Manager initialized successfully");
    return ESP_OK;
}

esp_err_t wifi_manager_start_client(const String& ssid, const String& password) {
    if (!wifi_initialized) {
        log_msg(LOG_ERROR, "WiFi not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    log_msg(LOG_INFO, "Starting WiFi Client mode for SSID: %s", ssid.c_str());
    
    targetSSID = ssid;
    targetPassword = password;
    
    // Reset connection tracking
    wasConnectedBefore = false;
    
    // Convert String to char*
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, ssid.c_str(), 32);
    strncpy((char*)wifi_config.sta.password, password.c_str(), 64);
    
    // CRITICAL: Set STA mode only
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // CRITICAL: Set DHCP hostname BEFORE starting WiFi
    String hostname = generateMdnsHostname();
    esp_err_t hostname_ret = esp_netif_set_hostname(sta_netif, hostname.c_str());
    log_msg(LOG_INFO, "DHCP hostname set to: %s (%s)", hostname.c_str(), esp_err_to_name(hostname_ret));
    
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Set power parameters
    esp_wifi_set_max_tx_power(20);  // ~5dBm
    
    // Start with a scan
    systemState.wifiClientState = CLIENT_SCANNING;
    led_set_mode(LED_MODE_WIFI_CLIENT_SEARCHING);
    
    esp_err_t scan_ret = esp_wifi_scan_start(NULL, false);
    if (scan_ret == ESP_OK) {
        scanInProgress = true;
        lastScanTime = millis();
        log_msg(LOG_DEBUG, "Initial WiFi scan started");
    } else {
        log_msg(LOG_WARNING, "Failed to start initial scan: %s", esp_err_to_name(scan_ret));
    }
    
    return ESP_OK;
}

esp_err_t wifi_manager_start_ap(const String& ssid, const String& password) {
    if (!wifi_initialized) {
        log_msg(LOG_ERROR, "WiFi not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    log_msg(LOG_INFO, "Starting WiFi AP mode: %s", ssid.c_str());
    
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.ap.ssid, ssid.c_str(), 32);
    strncpy((char*)wifi_config.ap.password, password.c_str(), 64);
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.channel = 1;
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Set power parameters
    esp_wifi_set_max_tx_power(20);  // ~5dBm
    
    // Initialize DNS server for captive portal
    // IMPORTANT: DNS remains on Arduino framework (DNSServer)
    // Only moving creation location from web_interface.cpp to wifi_manager.cpp
    if (!dnsServer) {
        dnsServer = new DNSServer();
        dnsServer->start(53, "*", IPAddress(192, 168, 4, 1));
        log_msg(LOG_INFO, "DNS Server started for captive portal (Arduino DNSServer)");
    }
    
    led_set_mode(LED_MODE_WIFI_ON);
    
    return ESP_OK;
}

void wifi_manager_stop() {
    if (!wifi_initialized) return;
    
    log_msg(LOG_INFO, "Stopping WiFi Manager");
    
    // Stop DNS server
    if (dnsServer) {
        dnsServer->stop();
        delete dnsServer;
        dnsServer = nullptr;
        log_msg(LOG_DEBUG, "DNS Server stopped");
    }
    
    // Stop WiFi
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_NULL);
    
    // Reset state
    systemState.wifiClientConnected = false;
    systemState.wifiClientState = CLIENT_IDLE;
    
    if (networkEventGroup) {
        xEventGroupClearBits(networkEventGroup, NETWORK_CONNECTED_BIT);
    }
}

void wifi_manager_process() {
    // Initialize mDNS if needed (safely outside event handler)
    initMdnsService();
    
    // Only process in client mode, skip when connected or inactive
    if (systemState.wifiClientState == CLIENT_IDLE || 
        systemState.wifiClientState == CLIENT_WRONG_PASSWORD ||
        systemState.wifiClientState == CLIENT_CONNECTED) return;
    
    unsigned long now = millis();
    
    // Handle connection timeout
    if (connectInProgress && (now - lastConnectAttempt > 10000)) {
        log_msg(LOG_WARNING, "Connection attempt timed out");
        esp_wifi_disconnect();
        // Let the disconnect event handler manage the retry logic
    }
    
    // Handle periodic scanning
    if (!scanInProgress && !connectInProgress && 
        systemState.wifiClientState != CLIENT_CONNECTED &&
        (now - lastScanTime > WIFI_CLIENT_SCAN_INTERVAL_MS)) {
        log_msg(LOG_DEBUG, "Starting periodic WiFi scan");
        
        esp_err_t scan_ret = esp_wifi_scan_start(NULL, false);
        if (scan_ret == ESP_OK) {
            scanInProgress = true;
            lastScanTime = now;
            scanFailureCount = 0;
        } else {
            // Scan failed to start
            scanFailureCount++;
            log_msg(LOG_WARNING, "WiFi scan failed to start (attempt %d), error: %s", scanFailureCount, esp_err_to_name(scan_ret));
            
            if (scanFailureCount >= 10) {
                // Too many failures - try to recover
                log_msg(LOG_WARNING, "Too many scan failures, attempting WiFi reset");
                esp_wifi_stop();
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_wifi_start();
                scanFailureCount = 0;
                
                // If still failing after reset, reboot might be needed
                if (scanFailureCount >= 20) {
                    log_msg(LOG_ERROR, "WiFi subsystem unrecoverable, rebooting...");
                    ESP.restart();
                }
            }
            
            // Retry scan after 1 second
            lastScanTime = now - (WIFI_CLIENT_SCAN_INTERVAL_MS - 1000);
        }
    }
    
    // Process DNS server if active
    if (dnsServer) {
        dnsServer->processNextRequest();
    }
}

bool wifi_manager_is_connected() {
    return systemState.wifiClientConnected;
}

String wifi_manager_get_ip() {
    if (!wifi_initialized) return "0.0.0.0";
    
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
        char ip_str[16];
        sprintf(ip_str, IPSTR, IP2STR(&ip_info.ip));
        return String(ip_str);
    }
    return "0.0.0.0";
}

int wifi_manager_get_rssi() {
    if (!wifi_initialized) return 0;
    
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        return ap.rssi;
    }
    return 0;
}

WiFiClientState wifi_manager_get_state() {
    return systemState.wifiClientState;
}

int rssi_to_percent(int rssi) {
    if (rssi >= WIFI_RSSI_EXCELLENT) return 100;
    if (rssi <= WIFI_RSSI_POOR) return 0;
    return (rssi - WIFI_RSSI_POOR) * 100 / (WIFI_RSSI_EXCELLENT - WIFI_RSSI_POOR);
}