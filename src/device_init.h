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

// Initialize and log device configuration
void initDevices();

#endif // DEVICE_INIT_H