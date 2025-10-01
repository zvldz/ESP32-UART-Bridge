#ifndef CONFIG_H
#define CONFIG_H

#include "types.h"

// Current configuration version
#define CURRENT_CONFIG_VERSION 9  // Increased from 8 to 9 for MAVLink routing

// Configuration management interface
void config_init(Config* config);
void config_load(Config* config);
void config_save(Config* config);
void config_migrate(Config* config);
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