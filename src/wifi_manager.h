#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "types.h"
#include <WiFi.h>

// WiFi manager interface
void wifi_manager_init();
void wifi_manager_start_client(const String& ssid, const String& password);
void wifi_manager_start_ap(const String& ssid, const String& password);
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

#endif // WIFI_MANAGER_H