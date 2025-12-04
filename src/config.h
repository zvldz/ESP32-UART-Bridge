#ifndef CONFIG_H
#define CONFIG_H

#include "types.h"

// Current configuration version
#define CURRENT_CONFIG_VERSION 10  // Increased from 9 to 10 for multi-WiFi networks

// Default values
#define DEFAULT_WIFI_TX_POWER   20      // 5dBm (20 * 0.25dBm)
#define MAX_WIFI_NETWORKS       5       // Maximum WiFi networks for Client mode

// Configuration management interface
void config_init(Config* config);
void config_load(Config* config);
void config_save(Config* config);
void config_migrate(Config* config);
void config_reset_wifi(Config* config);  // Reset WiFi settings to defaults (AP mode)
bool config_load_from_json(Config* config, const String& jsonString);
String config_to_json(Config* config);
void config_to_json_stream(Print& output, const Config* config);

// Helper functions for string conversion
const char* parity_to_string(uart_parity_t parity);
uart_parity_t string_to_parity(const char* str);
const char* word_length_to_string(uart_word_length_t length);
uart_word_length_t string_to_word_length(uint8_t bits);
const char* stop_bits_to_string(uart_stop_bits_t bits);
uart_stop_bits_t string_to_stop_bits(uint8_t bits);

#endif // CONFIG_H