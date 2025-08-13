#ifndef DEVICE3_TASK_H
#define DEVICE3_TASK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../uart/uart_interface.h"
#include "defines.h"

// Task function
void device3Task(void* parameter);

// External interfaces (defined in device_init.cpp)
extern UartInterface* device3Serial;

// Buffers for Device 3 operations
extern uint8_t device3TxBuffer[DEVICE3_UART_BUFFER_SIZE];
extern uint8_t device3RxBuffer[DEVICE3_UART_BUFFER_SIZE];
extern int device3TxHead;
extern int device3TxTail;
extern int device3RxHead;
extern int device3RxTail;

// Mutex for Device 3 buffer access
extern SemaphoreHandle_t device3Mutex;

// Global statistics
extern unsigned long globalDevice3TxBytes;
extern unsigned long globalDevice3RxBytes;

#endif // DEVICE3_TASK_H