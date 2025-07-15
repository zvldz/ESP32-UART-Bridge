#ifndef USB_INTERFACE_H
#define USB_INTERFACE_H

#include <Arduino.h>
#include "types.h"

// Global USB mode variable
extern UsbMode usbMode;

// Abstract interface for USB communication
class UsbInterface {
public:
    virtual ~UsbInterface() {}
    virtual void init() = 0;
    virtual int available() = 0;
    virtual int availableForWrite() = 0;
    virtual int read() = 0;
    virtual size_t write(uint8_t data) = 0;
    virtual size_t write(const uint8_t* buffer, size_t size) = 0;
    virtual bool connected() = 0;
    virtual void flush() = 0;
    virtual void end() = 0;
};

// Factory functions
UsbInterface* createUsbDevice(uint32_t baudrate);
UsbInterface* createUsbHost(uint32_t baudrate);
UsbInterface* createUsbAuto(uint32_t baudrate);

#endif // USB_INTERFACE_H