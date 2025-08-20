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

        // Use common buffer sizes for consistency
        Serial.setRxBufferSize(UsbBufferSizes::RX_BUFFER_SIZE);
        Serial.setTxBufferSize(UsbBufferSizes::TX_BUFFER_SIZE);

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
        if (!initialized) return -1;
        return Serial.read();
    }

    size_t write(uint8_t data) override {
        if (!initialized) return 0;
        return Serial.write(data);
    }

    size_t write(const uint8_t* buffer, size_t size) override {
        if (!initialized) return 0;
        return Serial.write(buffer, size);
    }

    bool connected() override {
        return initialized && Serial;
    }

    void flush() override {
        if (initialized) {
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
};

// Factory method for creating a USB Device
UsbInterface* createUsbDevice(uint32_t baudrate) {
    return new UsbDevice(baudrate);
}