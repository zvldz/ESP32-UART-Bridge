#ifndef USB_INTERFACE_H
#define USB_INTERFACE_H

#include <Arduino.h>

// Abstract interface for USB communication
class UsbInterface {
public:
    virtual ~UsbInterface() {}
    virtual void init() = 0;
    virtual int available() = 0;
    virtual int read() = 0;
    virtual size_t write(uint8_t data) = 0;
    virtual size_t write(const uint8_t* buffer, size_t size) = 0;
    virtual bool connected() = 0;
};

#endif // USB_INTERFACE_H