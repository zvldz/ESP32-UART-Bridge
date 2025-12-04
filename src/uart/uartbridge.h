#ifndef UARTBRIDGE_H
#define UARTBRIDGE_H

#include "types.h"
#include "../usb/usb_interface.h"
#include "uart_interface.h"

// Forward declarations
class ProtocolPipeline;

// Task functions
void uartBridgeTask(void* parameter);  // FreeRTOS task function
void senderTask(void* parameter);      // Sender task function

// Device 3 task
void device3Task(void* parameter);     // FreeRTOS task for Device 3 operations

// UDP log buffer and mutex
extern SemaphoreHandle_t udpLogMutex;

// External device serial interfaces (defined in uartbridge.cpp)
extern UartInterface* device2Serial;
extern UartInterface* device3Serial;

// Get protocol pipeline instance
ProtocolPipeline* getProtocolPipeline();

#endif // UARTBRIDGE_H