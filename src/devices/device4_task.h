#ifndef DEVICE4_TASK_H
#define DEVICE4_TASK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <AsyncUDP.h>
#include "defines.h"

// Task function
void device4Task(void* parameter);

// Log buffer
extern uint8_t device4LogBuffer[DEVICE4_LOG_BUFFER_SIZE];
extern int device4LogHead;
extern int device4LogTail;
extern SemaphoreHandle_t device4LogMutex;

// REMOVED: Bridge buffers - now using Pipeline + UdpTxQueue

// Global statistics
extern unsigned long globalDevice4TxBytes;
extern unsigned long globalDevice4TxPackets;
extern unsigned long globalDevice4RxBytes;
extern unsigned long globalDevice4RxPackets;

// Device 1 TX statistics for UDP→UART forwarding
extern unsigned long device1TxBytesFromDevice4;

// AsyncUDP instance
extern AsyncUDP* device4UDP;

#endif // DEVICE4_TASK_H