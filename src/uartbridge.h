#ifndef UARTBRIDGE_H
#define UARTBRIDGE_H

#include "types.h"
#include "usb_interface.h"
#include "uart_interface.h"

// UART bridge task function
void uartBridgeTask(void* parameter);  // FreeRTOS task function

// Device 3 task
void device3Task(void* parameter);     // FreeRTOS task for Device 3 operations

// External device serial interfaces (defined in uartbridge.cpp)
extern UartInterface* device2Serial;
extern UartInterface* device3Serial;

#endif // UARTBRIDGE_H