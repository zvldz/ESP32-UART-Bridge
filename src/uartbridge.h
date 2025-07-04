#ifndef UARTBRIDGE_H
#define UARTBRIDGE_H

#include "types.h"
#include <HardwareSerial.h>

// UART bridge interface
void uartbridge_init(HardwareSerial* serial, Config* config, UartStats* stats);
void uartBridgeTask(void* parameter);  // FreeRTOS task function

// Flow control functions
void detectFlowControl();     // TODO: Enhanced Flow Control diagnostics
String getFlowControlStatus();

// Thread-safe statistics update
void updateSharedStats(unsigned long uartToUsb, unsigned long usbToUart, unsigned long activity);

// Statistics reset
void resetStatistics(UartStats* stats);

#endif // UARTBRIDGE_H