#ifndef DEVICE4_TASK_H
#define DEVICE4_TASK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <AsyncUDP.h>
#include "defines.h"

// Task function
void device4Task(void* parameter);

// Statistics update function
void updateDevice4Stats();

// Log buffer
extern uint8_t device4LogBuffer[DEVICE4_LOG_BUFFER_SIZE];
extern int device4LogHead;
extern int device4LogTail;
extern SemaphoreHandle_t device4LogMutex;

// Bridge buffers (only used in Bridge mode)
extern uint8_t device4BridgeTxBuffer[DEVICE4_BRIDGE_BUFFER_SIZE];
extern uint8_t device4BridgeRxBuffer[DEVICE4_BRIDGE_BUFFER_SIZE];
extern int device4BridgeTxHead;
extern int device4BridgeTxTail;
extern int device4BridgeRxHead;
extern int device4BridgeRxTail;
extern SemaphoreHandle_t device4BridgeMutex;

// Global statistics
extern unsigned long globalDevice4TxBytes;
extern unsigned long globalDevice4TxPackets;
extern unsigned long globalDevice4RxBytes;
extern unsigned long globalDevice4RxPackets;

// AsyncUDP instance
extern AsyncUDP* device4UDP;

#endif // DEVICE4_TASK_H