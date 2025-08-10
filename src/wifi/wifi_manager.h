#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "types.h"
#include "esp_err.h"
#include <DNSServer.h>

// WiFi manager interface with ESP-IDF error handling
esp_err_t wifi_manager_init();
esp_err_t wifi_manager_start_client(const String& ssid, const String& password);
esp_err_t wifi_manager_start_ap(const String& ssid, const String& password);
void wifi_manager_stop();
void wifi_manager_process();  // Called from main loop

// Status functions
bool wifi_manager_is_connected();
int wifi_manager_get_rssi();
String wifi_manager_get_ip();
WiFiClientState wifi_manager_get_state();

// Utility functions
int rssi_to_percent(int rssi);

// Event callbacks (implemented in main.cpp)
extern void on_wifi_connected();
extern void on_wifi_disconnected();

// Network event group for task synchronization
extern EventGroupHandle_t networkEventGroup;
#define NETWORK_CONNECTED_BIT BIT0

// DNS Server (перенесен из web_interface.cpp для доступа из scheduler_tasks.cpp)
extern DNSServer* dnsServer;

#endif // WIFI_MANAGER_H