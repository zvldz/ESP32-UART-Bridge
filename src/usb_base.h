#ifndef USB_BASE_H
#define USB_BASE_H

#include "usb_interface.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <atomic>

// Base class for USB implementations with common ring buffer logic
class UsbBase : public UsbInterface {
protected:
    // Common buffer sizes
    static constexpr size_t RX_BUFFER_SIZE = 1024;
    static constexpr size_t TX_BUFFER_SIZE = 1024;
    
    // Ring buffers
    uint8_t rx_buffer[RX_BUFFER_SIZE];
    uint8_t tx_buffer[TX_BUFFER_SIZE];
    volatile size_t rx_head;
    volatile size_t rx_tail;
    volatile size_t tx_head;
    volatile size_t tx_tail;
    
    // Synchronization
    SemaphoreHandle_t rx_mutex;
    SemaphoreHandle_t tx_mutex;
    
    // State
    uint32_t baudrate;
    bool initialized;
    std::atomic<bool> is_connected;
    
    // Task handle
    TaskHandle_t task_handle;
    
public:
    UsbBase(uint32_t baud) : baudrate(baud), initialized(false), 
                             rx_head(0), rx_tail(0), tx_head(0), tx_tail(0),
                             task_handle(nullptr), is_connected(false) {
        rx_mutex = xSemaphoreCreateMutex();
        tx_mutex = xSemaphoreCreateMutex();
    }
    
    virtual ~UsbBase() {
        if (rx_mutex) vSemaphoreDelete(rx_mutex);
        if (tx_mutex) vSemaphoreDelete(tx_mutex);
    }
    
    // Common implementations
    int available() override {
        if (!initialized) return 0;
        
        xSemaphoreTake(rx_mutex, portMAX_DELAY);
        size_t count = (rx_head >= rx_tail) ? 
                       (rx_head - rx_tail) : 
                       (RX_BUFFER_SIZE - rx_tail + rx_head);
        xSemaphoreGive(rx_mutex);
        
        return count;
    }
    
    int availableForWrite() override {
        if (!initialized) return 0;
        
        xSemaphoreTake(tx_mutex, portMAX_DELAY);
        size_t space = (tx_tail > tx_head) ? 
                       (tx_tail - tx_head - 1) : 
                       (TX_BUFFER_SIZE - tx_head + tx_tail - 1);
        xSemaphoreGive(tx_mutex);
        
        return space;
    }
    
    int read() override {
        if (!initialized || available() == 0) return -1;
        
        xSemaphoreTake(rx_mutex, portMAX_DELAY);
        uint8_t data = rx_buffer[rx_tail];
        rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;
        xSemaphoreGive(rx_mutex);
        
        return data;
    }
    
    size_t write(uint8_t data) override {
        if (!initialized || availableForWrite() == 0) return 0;
        
        xSemaphoreTake(tx_mutex, portMAX_DELAY);
        tx_buffer[tx_head] = data;
        tx_head = (tx_head + 1) % TX_BUFFER_SIZE;
        xSemaphoreGive(tx_mutex);
        
        return 1;
    }
    
    size_t write(const uint8_t* buffer, size_t size) override {
        if (!initialized) return 0;
        
        size_t written = 0;
        for (size_t i = 0; i < size; i++) {
            if (write(buffer[i]) == 0) break;
            written++;
        }
        
        return written;
    }
    
    bool connected() override {
        return initialized && is_connected;
    }
    
    void flush() override {
        if (!initialized) return;
        
        // Wait for TX buffer to empty
        while (tx_head != tx_tail) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        
        // Derived class should implement hardware-specific flush
        flushHardware();
    }
    
protected:
    // Helper methods for derived classes
    
    // Add data to RX buffer (called from task)
    bool addToRxBuffer(const uint8_t* data, size_t len) {
        xSemaphoreTake(rx_mutex, portMAX_DELAY);
        bool success = true;
        
        for (size_t i = 0; i < len; i++) {
            size_t next_head = (rx_head + 1) % RX_BUFFER_SIZE;
            if (next_head != rx_tail) {
                rx_buffer[rx_head] = data[i];
                rx_head = next_head;
            } else {
                success = false;
                break;
            }
        }
        
        xSemaphoreGive(rx_mutex);
        return success;
    }
    
    // Get data from TX buffer (called from task)
    size_t getFromTxBuffer(uint8_t* data, size_t maxLen) {
        xSemaphoreTake(tx_mutex, portMAX_DELAY);
        size_t count = 0;
        
        while (tx_tail != tx_head && count < maxLen) {
            data[count++] = tx_buffer[tx_tail];
            tx_tail = (tx_tail + 1) % TX_BUFFER_SIZE;
        }
        
        xSemaphoreGive(tx_mutex);
        return count;
    }
    
    // Abstract methods for derived classes
    virtual void flushHardware() = 0;
};

#endif // USB_BASE_H