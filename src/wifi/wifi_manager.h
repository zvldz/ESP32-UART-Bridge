#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "types.h"
#include "esp_err.h"
#include <DNSServer.h>

// WiFi Manager constants
#define WIFI_RECONNECT_DELAY_MS     500   // Delay between reconnection attempts
#define WIFI_MDNS_SERVICE_PORT      80    // HTTP service port for mDNS
#define WIFI_MAC_SUFFIX_BUFFER_SIZE 6     // Buffer size for MAC address suffix
#define WIFI_MIN_HEAP_BYTES         30000 // Minimum heap required for WiFi init
#define WIFI_SSID_MAX_LEN           32    // Maximum SSID length
#define WIFI_PASSWORD_MAX_LEN       64    // Maximum password length
#define WIFI_CONNECT_TIMEOUT_MS     10000 // Connection attempt timeout

// WiFi Client Mode constants
#define WIFI_CLIENT_RETRY_INTERVAL_MS  10000  // Retry every 10 seconds
#define WIFI_CLIENT_SCAN_INTERVAL_MS   15000  // Scan for networks every 15 seconds
#define WIFI_CLIENT_MAX_RETRIES        5      // Max password attempts before error state

// RSSI thresholds for percentage calculation
#define WIFI_RSSI_EXCELLENT -30  // 100% signal
#define WIFI_RSSI_POOR      -90  // 0% signal

// WiFi manager interface with ESP-IDF error handling
esp_err_t wifiInit();
esp_err_t wifiStartClient(const String& ssid, const String& password);
esp_err_t wifiStartAP(const String& ssid, const String& password);
void wifiProcess();  // Called from main loop

// Status functions
bool wifiIsReady();  // Universal check for data transmission
int wifiGetRSSI();
String wifiGetIP();

// Utility functions
int rssiToPercent(int rssi);

// Event callbacks (implemented in main.cpp)
extern void on_wifi_connected();
extern void on_wifi_disconnected();

// Network event group for task synchronization
extern EventGroupHandle_t networkEventGroup;
#define NETWORK_CONNECTED_BIT BIT0

// DNS Server
extern DNSServer* dnsServer;

#endif // WIFI_MANAGER_H