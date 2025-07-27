#include "device3_task.h"
#include "config.h"
#include "logging.h"
#include "leds.h"
#include "types.h"
#include "diagnostics.h"
#include "uart_dma.h"

// External objects
extern Config config;
extern UartStats uartStats;

// Device 3 UART interface pointer defined in device_init.cpp
// declared as extern in device3_task.h

// Shared buffers for Device 3 operations
uint8_t device3TxBuffer[DEVICE3_UART_BUFFER_SIZE];
uint8_t device3RxBuffer[DEVICE3_UART_BUFFER_SIZE];
// Buffer indices protected by mutex - no need for volatile
int device3TxHead = 0;
int device3TxTail = 0;
int device3RxHead = 0;
int device3RxTail = 0;

// Mutex for Device 3 buffer access
SemaphoreHandle_t device3Mutex = nullptr;

// Device 3 statistics (unified approach like Device 4)
unsigned long globalDevice3TxBytes = 0;
unsigned long globalDevice3RxBytes = 0;

void updateDevice3Stats() {
    enterStatsCritical();
    uartStats.device3TxBytes = globalDevice3TxBytes;
    uartStats.device3RxBytes = globalDevice3RxBytes;
    exitStatsCritical();
}

// Device 3 Task - runs on Core 0 with UART task
void device3Task(void* parameter) {
  // Wait for system initialization
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  log_msg("Device 3 task started on core " + String(xPortGetCoreID()), LOG_INFO);
  
  
  // Rate limiting for LED notifications
  unsigned long lastDevice3TxLed = 0;
  unsigned long lastDevice3RxLed = 0;
  const unsigned long LED_NOTIFY_INTERVAL = 10;
  
  // Buffer for batch operations
  const int UART_BLOCK_SIZE = 64;
  uint8_t readBuffer[UART_BLOCK_SIZE];
  
  while (1) {
    // Poll DMA events for Device 3
    if (device3Serial) {
      static_cast<UartDMA*>(device3Serial)->pollEvents();
    }
    
    if (config.device3.role == D3_UART3_MIRROR) {
      // Mirror mode - copy data from buffer to Device 3 TX
      if (xSemaphoreTake(device3Mutex, 0) == pdTRUE) {
        // Process in blocks for efficiency
        int bytesToWrite = 0;
        uint8_t writeBuffer[UART_BLOCK_SIZE];
        
        while (device3TxHead != device3TxTail && bytesToWrite < UART_BLOCK_SIZE) {
          writeBuffer[bytesToWrite++] = device3TxBuffer[device3TxTail];
          device3TxTail = (device3TxTail + 1) % DEVICE3_UART_BUFFER_SIZE;
        }
        
        xSemaphoreGive(device3Mutex);
        
        // Write collected data
        if (bytesToWrite > 0 && device3Serial) {
          int written = device3Serial->write(writeBuffer, bytesToWrite);
          enterStatsCritical();
          globalDevice3TxBytes += written;
          exitStatsCritical();
          
          // LED notification with rate limiting
          if (millis() - lastDevice3TxLed > LED_NOTIFY_INTERVAL) {
            led_notify_device3_tx();
            lastDevice3TxLed = millis();
          }
        }
      }
    }
    else if (config.device3.role == D3_UART3_BRIDGE) {
      // Bridge mode - bidirectional data transfer
      
      // Device 3 RX -> Buffer (to be sent to Device 1)
      if (device3Serial && device3Serial->available()) {
        if (xSemaphoreTake(device3Mutex, 0) == pdTRUE) {
          // Read in blocks for efficiency
          int toRead = min(device3Serial->available(), UART_BLOCK_SIZE);
          toRead = min(toRead, (int)sizeof(readBuffer));
          
          int actuallyRead = 0;
          for (int i = 0; i < toRead; i++) {
            int byte = device3Serial->read();
            if (byte >= 0) {
              readBuffer[actuallyRead++] = (uint8_t)byte;
            }
          }
          
          // Store in circular buffer
          for (int i = 0; i < actuallyRead; i++) {
            int nextHead = (device3RxHead + 1) % DEVICE3_UART_BUFFER_SIZE;
            if (nextHead != device3RxTail) {  // Buffer not full
              device3RxBuffer[device3RxHead] = readBuffer[i];
              device3RxHead = nextHead;
              enterStatsCritical();
              globalDevice3RxBytes++;
              exitStatsCritical();
            } else {
              // Buffer full
              log_msg("Device 3 RX buffer full, dropping data", LOG_WARNING);
              break;
            }
          }
          
          xSemaphoreGive(device3Mutex);
          
          // LED notification with rate limiting
          if (actuallyRead > 0 && millis() - lastDevice3RxLed > LED_NOTIFY_INTERVAL) {
            led_notify_device3_rx();
            lastDevice3RxLed = millis();
          }
        }
      }
      
      // Buffer -> Device 3 TX (from Device 1)
      if (xSemaphoreTake(device3Mutex, 0) == pdTRUE) {
        // Process in blocks for efficiency
        int bytesToWrite = 0;
        uint8_t writeBuffer[UART_BLOCK_SIZE];
        
        while (device3TxHead != device3TxTail && bytesToWrite < UART_BLOCK_SIZE) {
          writeBuffer[bytesToWrite++] = device3TxBuffer[device3TxTail];
          device3TxTail = (device3TxTail + 1) % DEVICE3_UART_BUFFER_SIZE;
        }
        
        xSemaphoreGive(device3Mutex);
        
        // Write collected data
        if (bytesToWrite > 0 && device3Serial) {
          int written = device3Serial->write(writeBuffer, bytesToWrite);
          enterStatsCritical();
          globalDevice3TxBytes += written;
          exitStatsCritical();
          
          // LED notification
          if (millis() - lastDevice3TxLed > LED_NOTIFY_INTERVAL) {
            led_notify_device3_tx();
            lastDevice3TxLed = millis();
          }
        }
      }
    }
    
    
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}