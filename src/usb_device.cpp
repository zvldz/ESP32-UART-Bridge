#include "logging.h"
#include "usb_interface.h"
#include "defines.h"
#include "types.h"

// Concrete implementation for USB device (CDC over USB)
class UsbDevice : public UsbInterface {
public:
    UsbDevice(uint32_t baudrate) : baudrate(baudrate), initialized(false) {}

    void init() override {
        Serial.begin(baudrate);

        // Increase RX/TX buffer for better performance
        Serial.setRxBufferSize(1024);
        Serial.setTxBufferSize(1024);

        // Wait for USB connection (maximum 2 seconds)
        unsigned long startTime = millis();
        while (!Serial && (millis() - startTime < 2000)) {
            delay(10);
        }

        // Add stabilization delay only if USB is connected
        if (Serial) {
            vTaskDelay(pdMS_TO_TICKS(500));  // Allow USB detection time
            log_msg(LOG_INFO, "USB Device: connected at %lu baud", baudrate);
        } else {
            log_msg(LOG_INFO, "USB Device: no connection detected, continuing...");
        }

        // Reset behavioral detection state
        everHadFreeSpace = false;
        consecutiveFullBuffer = 0;
        
        initialized = true;
    }

    int available() override {
        if (!initialized) return 0;
        return Serial.available();
    }

    int availableForWrite() override {
        if (!initialized) return 0;
        return Serial.availableForWrite();
    }

    int read() override {
        if (!connected()) return -1;
        return Serial.read();
    }

    size_t write(uint8_t data) override {
        uint8_t buf[1] = {data};
        return write(buf, 1);
    }

    size_t write(const uint8_t* buffer, size_t size) override {
        if (!initialized || size == 0) return 0;
        
        // For first attempts, always try even if we haven't seen free space yet
        if (!everHadFreeSpace && consecutiveFullBuffer < FIRST_ATTEMPT_THRESHOLD) {
            // Give initial data a chance to determine port state
        } else if (!connected()) {
            // After initial attempts, use connected() to quickly reject if port is closed
            return 0;
        }
        
        int availableSpace = Serial.availableForWrite();
        
        if (availableSpace == 0) {
            // Buffer is full
            consecutiveFullBuffer++;
            
            if (consecutiveFullBuffer >= ASSUME_CLOSED_THRESHOLD) {
                // Port is likely closed - honestly report we cannot write
                // This allows caller to drop old data and prepare fresh data
                return 0;
            }
            
            // Still hoping port will open - honestly report no bytes written
            return 0;
            
        } else {
            // We have space - port is definitely open!
            everHadFreeSpace = true;
            consecutiveFullBuffer = 0;  // Reset counter
            
            // Write what we can
            size_t toWrite = (availableSpace < size) ? availableSpace : size;
            return Serial.write(buffer, toWrite);
        }
    }

    bool connected() override {
        if (!initialized || !Serial) return false;
        
        // Consider connected if:
        // 1. We ever had free space (port was opened at least once)
        // 2. Buffer is not stuck full (port is still being read)
        return everHadFreeSpace && (consecutiveFullBuffer < ASSUME_CLOSED_THRESHOLD);
    }

    void flush() override {
        if (connected()) {
            Serial.flush();
        }
    }

    void end() override {
        if (initialized) {
            Serial.end();
        }
        initialized = false;
    }

private:
    uint32_t baudrate;
    bool initialized;
    
    // Behavioral port detection state
    bool everHadFreeSpace = false;           // Ever had space in buffer since init
    uint32_t consecutiveFullBuffer = 0;      // Counter for full buffer conditions
    
    // Thresholds for real-time critical data
    static constexpr uint32_t ASSUME_CLOSED_THRESHOLD = 20;  // ~20-200ms depending on call rate
    static constexpr uint32_t FIRST_ATTEMPT_THRESHOLD = 5;   // Give first data a chance
};

// Factory method for creating a USB Device
UsbInterface* createUsbDevice(uint32_t baudrate) {
    return new UsbDevice(baudrate);
}