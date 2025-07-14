#include "usb_interface.h"
#include "logging.h"
#include <Arduino.h>

// USB Device implementation using Arduino Serial
class UsbDevice : public UsbInterface {
private:
    uint32_t baudrate;
    bool initialized;

public:
    UsbDevice(uint32_t baud) : baudrate(baud), initialized(false) {}
    
    void init() override {
        Serial.begin(baudrate);
        
        // Increase RX buffer for better performance at high speeds
        if (baudrate >= 115200) {
            Serial.setRxBufferSize(1024);
        }
        
        // Wait for USB connection (maximum 2 seconds)
        unsigned long startTime = millis();
        while (!Serial && (millis() - startTime < 2000)) {
            delay(10);
        }
        
        // Add stabilization delay only if USB is connected
        if (Serial) {
            vTaskDelay(pdMS_TO_TICKS(500));  // Allow USB detection time
            log_msg("USB Device: connected at " + String(baudrate) + " baud");
        } else {
            log_msg("USB Device: no connection detected, continuing...");
        }
        
        initialized = true;
    }
    
    int available() override {
        return initialized ? Serial.available() : 0;
    }
    
    int availableForWrite() override {
        return initialized ? Serial.availableForWrite() : 0;
    }
    
    int read() override {
        return initialized ? Serial.read() : -1;
    }
    
    size_t write(uint8_t data) override {
        return initialized ? Serial.write(data) : 0;
    }
    
    size_t write(const uint8_t* buffer, size_t size) override {
        return initialized ? Serial.write(buffer, size) : 0;
    }
    
    bool connected() override {
        return initialized && Serial;
    }
    
    void flush() override {
        if (initialized) Serial.flush();
    }
    
    void end() override {
        if (initialized) {
            Serial.end();
            initialized = false;
        }
    }
};

// Factory function
UsbInterface* createUsbDevice(uint32_t baudrate) {
    return new UsbDevice(baudrate);
}