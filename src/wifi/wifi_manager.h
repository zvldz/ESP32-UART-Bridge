#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "types.h"
#include "esp_err.h"
#include <DNSServer.h>

// WiFi manager interface with ESP-IDF error handling
esp_err_t wifiInit();
esp_err_t wifiStartClient(const String& ssid, const String& password);
esp_err_t wifiStartAP(const String& ssid, const String& password);
void wifiStop();
void wifiProcess();  // Called from main loop

// Status functions
bool wifiIsReady();  // Universal check for data transmission
int wifiGetRSSI();
String wifiGetIP();
WiFiClientState wifiGetState();

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