#ifndef UART_DMA_H
#define UART_DMA_H

#include "uart_interface.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <atomic>

class UartDMA : public UartInterface {
public:
    // DMA-specific configuration structure
    struct DmaConfig {
        bool useEventTask = true;          // Create event task for interrupt-driven operation
        size_t dmaRxBufSize = 8192;       // DMA RX buffer size
        size_t dmaTxBufSize = 8192;       // DMA TX buffer size  
        size_t ringBufSize = 16384;       // Application ring buffer size
        uint8_t eventTaskPriority = (configMAX_PRIORITIES - 1);   // Priority for event task (if used) (20)
        size_t eventQueueSize = 30;       // UART event queue size
    };
    
    // Static method to get default DMA configuration
    static DmaConfig getDefaultDmaConfig() {
        return DmaConfig();
    }

private:
    // Buffer sizes from configuration
    const size_t DMA_RX_BUF_SIZE;
    const size_t DMA_TX_BUF_SIZE;
    const size_t RING_BUF_SIZE;
    
    static constexpr uint32_t RX_TIMEOUT_MS = 2;     // ~23 symbols at 115200
    
    uart_port_t uart_num;
    QueueHandle_t uart_queue;
    TaskHandle_t event_task_handle;
    SemaphoreHandle_t rx_mutex;
    SemaphoreHandle_t tx_mutex;
    
    // Ring buffer implementation
    uint8_t* rx_ring_buf;
    volatile size_t rx_head;
    volatile size_t rx_tail;
    std::atomic<bool> packet_timeout_flag;
    std::atomic<bool> overrun_flag;
    
    // Configuration storage
    DmaConfig dmaConfig;     // DMA-specific configuration
    UartConfig uartConfig;   // UART parameters configuration
    int8_t rx_pin;
    int8_t tx_pin;
    
    // Initialization status
    bool initialized;
    
    // Statistics
    volatile uint32_t rx_bytes_total;
    volatile uint32_t tx_bytes_total;
    volatile uint32_t overrun_count;
    
    // Private methods
    void processRxData(uint8_t* data, size_t len);
    size_t getRxBytesAvailable() const;
    static void uartEventTask(void* pvParameters);
    
public:
    explicit UartDMA(uart_port_t uart, const DmaConfig& cfg);
    explicit UartDMA(uart_port_t uart);  // Uses default DMA config
    ~UartDMA();
    
    // UartInterface implementation - full configuration
    void begin(const UartConfig& config, int8_t rxPin, int8_t txPin) override;
    
    // Simplified begin (for compatibility)
    void begin(uint32_t baudrate, int8_t rxPin, int8_t txPin) override;
    
    int available() override;
    int availableForWrite() override;
    int read() override;
    size_t write(uint8_t data) override;
    size_t write(const uint8_t* buffer, size_t size) override;
    void flush() override;
    void end() override;
    void setRxBufferSize(size_t size) override;
    
    // Extended interface
    bool hasPacketTimeout() override;
    bool hasOverrun() override;
    size_t getRxBufferSize() override { return RING_BUF_SIZE; }
    
    // Status check
    bool isInitialized() const override { return initialized; }
    
    // Statistics
    uint32_t getRxBytesTotal() const { return rx_bytes_total; }
    uint32_t getTxBytesTotal() const { return tx_bytes_total; }
    uint32_t getOverrunCount() const { return overrun_count; }
    
    // Poll events for non-event-task mode
    // Checks UART queue and processes any pending events
    void pollEvents();
};

#endif // UART_DMA_H