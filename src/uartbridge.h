#ifndef UARTBRIDGE_H
#define UARTBRIDGE_H

#include "types.h"
#include "usb_interface.h"
#include "uart_interface.h"

// UART bridge interface - always uses UartInterface
void uartbridge_init(UartInterface* serial, Config* config, UartStats* stats, UsbInterface* usb);
void uartBridgeTask(void* parameter);  // FreeRTOS task function

// Device 3 task
void device3Task(void* parameter);     // FreeRTOS task for Device 3 operations

// Flow control functions
void detectFlowControl();
String getFlowControlStatus();

// Device 2 functions
void initDevice2UART();

// Device 3 functions
void initDevice3(uint8_t role);  // Initialize based on role (mirror/bridge)

#endif // UARTBRIDGE_H