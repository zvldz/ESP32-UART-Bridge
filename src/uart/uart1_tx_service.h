#pragma once
#include "../circular_buffer.h"
#include "../uart/uart_interface.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class Uart1TxService {
private:
    CircularBuffer* txRing;
    UartInterface* uart;
    SemaphoreHandle_t ringMutex;
    
    // Statistics
    uint32_t totalBytes;
    uint32_t droppedBytes;
    uint32_t writeErrors;
    
    // Configuration
    size_t maxWritePerCall;  // Limit per processTxQueue() call
    
public:
    Uart1TxService();
    ~Uart1TxService();
    
    bool init(UartInterface* uartInterface, size_t ringSize = UART1_TX_RING_SIZE);
    
    // Thread-safe enqueue (can be called from any context)
    bool enqueue(const uint8_t* data, size_t len);
    
    // Process TX queue (called from uartBridgeTask only)
    void processTxQueue();
    
    // Statistics
    size_t getQueuedBytes() const { return txRing ? txRing->available() : 0; }
    uint32_t getDroppedBytes() const { return droppedBytes; }
    
    // Get singleton instance
    static Uart1TxService* getInstance();
};