#ifndef UART_INTERFACE_H
#define UART_INTERFACE_H

#include <stddef.h>
#include <stdint.h>
#include <Arduino.h>
#include "driver/uart.h"

// UART configuration structure for all implementations
struct UartConfig {
    uint32_t baudrate;
    uart_word_length_t databits;
    uart_parity_t parity;
    uart_stop_bits_t stopbits;
    bool flowcontrol;
};

class UartInterface {
public:
    virtual ~UartInterface() {}
    
    // Core UART functions - with full configuration
    virtual void begin(const UartConfig& config, int8_t rxPin, int8_t txPin) = 0;
    
    // Legacy begin for gradual migration
    virtual void begin(uint32_t baudrate, int8_t rxPin, int8_t txPin) {
        // Default configuration
        UartConfig cfg = {
            .baudrate = baudrate,
            .databits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stopbits = UART_STOP_BITS_1,
            .flowcontrol = false
        };
        begin(cfg, rxPin, txPin);
    }
    
    virtual int available() = 0;
    virtual int availableForWrite() = 0;
    virtual int read() = 0;
    virtual size_t write(uint8_t data) = 0;
    virtual size_t write(const uint8_t* buffer, size_t size) = 0;
    virtual void flush() = 0;
    virtual void end() = 0;
    virtual void setRxBufferSize(size_t size) = 0;
    
    // Extended interface for DMA features
    virtual bool hasPacketTimeout() { return false; }  // For adaptive buffering
    virtual bool hasOverrun() { return false; }        // For error detection
    virtual size_t getRxBufferSize() { return 0; }     // For diagnostics
    
    // Get flow control status string for web interface
    virtual String getFlowControlStatus() const {
        return "Not supported";
    }
    
    // Status checks
    virtual bool isInitialized() const { return true; }      // For error handling
};

#endif // UART_INTERFACE_H