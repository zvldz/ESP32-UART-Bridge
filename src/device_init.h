#ifndef DEVICE_INIT_H
#define DEVICE_INIT_H

#include <stdint.h>
#include "types.h"  // Need full definitions of Config and UartStats

// Forward declarations for classes only
class UartInterface;
class UsbInterface;

// Initialize main UART bridge (Device 1)
void initMainUART(UartInterface* serial, Config* config, UsbInterface* usb);

// Device initialization functions
void initDevice2UART();
void initDevice3(uint8_t role);

// SBUS initialization functions
void initDevice2SBUS();
void initDevice3SBUS();

// Bluetooth SPP initialization (MiniKit only)
#if defined(BOARD_MINIKIT_ESP32)
void initDevice5Bluetooth();
#endif

// Initialize and log device configuration
void initDevices();

// Helper to check if any SBUS device is configured
bool hasSbusDevice();

// Register SBUS outputs after UART interfaces are created
void registerSbusOutputs();

#endif // DEVICE_INIT_H