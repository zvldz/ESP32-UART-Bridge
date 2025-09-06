#include "uart_dma.h"
#include "logging.h"
#include "defines.h"  // For RTS_PIN, CTS_PIN
#include "config.h"   // For conversion functions
#include <cstring>

// UART DMA event task runs on Core 0 (same as UART Bridge for data consistency)
#define UART_DMA_TASK_CORE 0

// Constructor with DMA configuration
UartDMA::UartDMA(uart_port_t uart, const DmaConfig& cfg) 
    : uart_num(uart)
    , DMA_RX_BUF_SIZE(cfg.dmaRxBufSize)
    , DMA_TX_BUF_SIZE(cfg.dmaTxBufSize)
    , RING_BUF_SIZE(cfg.ringBufSize)
    , uart_queue(nullptr)
    , event_task_handle(nullptr)
    , rx_mutex(nullptr)
    , tx_mutex(nullptr)
    , rx_ring_buf(nullptr)
    , rx_head(0)
    , rx_tail(0)
    , packet_timeout_flag(false)
    , overrun_flag(false)
    , dmaConfig(cfg)
    , rx_pin(-1)
    , tx_pin(-1)
    , rx_bytes_total(0)
    , tx_bytes_total(0)
    , overrun_count(0) {
    
    // Initialize as not ready
    initialized = false;
    
    // Allocate ring buffer with configured size
    rx_ring_buf = (uint8_t*)heap_caps_malloc(RING_BUF_SIZE, MALLOC_CAP_DMA);
    if (!rx_ring_buf) {
        log_msg(LOG_ERROR, "Failed to allocate DMA ring buffer of size %d", RING_BUF_SIZE);
        return;  // Critical error - cannot continue without buffer
    }
    
    // Create synchronization primitives
    rx_mutex = xSemaphoreCreateMutex();
    tx_mutex = xSemaphoreCreateMutex();
    
    if (!rx_mutex || !tx_mutex) {
        log_msg(LOG_ERROR, "Failed to create mutexes");
        // Cleanup allocated resources
        if (rx_ring_buf) {
            heap_caps_free(rx_ring_buf);
            rx_ring_buf = nullptr;
        }
        if (rx_mutex) vSemaphoreDelete(rx_mutex);
        if (tx_mutex) vSemaphoreDelete(tx_mutex);
        return;
    }
}

// Constructor with default DMA configuration
UartDMA::UartDMA(uart_port_t uart) : UartDMA(uart, getDefaultDmaConfig()) {
    // Delegating constructor
}

// Destructor
UartDMA::~UartDMA() {
    end();
    if (rx_ring_buf) {
        heap_caps_free(rx_ring_buf);
    }
    if (rx_mutex) {
        vSemaphoreDelete(rx_mutex);
    }
    if (tx_mutex) {
        vSemaphoreDelete(tx_mutex);
    }
}

// Initialize UART with full configuration
void UartDMA::begin(const UartConfig& config, int8_t rxPin, int8_t txPin) {
    // Check if constructor succeeded
    if (!rx_ring_buf || !rx_mutex || !tx_mutex) {
        log_msg(LOG_ERROR, "UartDMA not properly initialized, cannot begin");
        return;
    }
    
    // Save configuration
    uartConfig = config;
    rx_pin = rxPin;
    tx_pin = txPin;
    
    // Debug output using stored configuration
    log_msg(LOG_DEBUG, "DMA UART config: %u baud, %s%c%s", 
            config.baudrate,
            word_length_to_string(config.databits),
            parity_to_string(config.parity)[0],  // First char only
            stop_bits_to_string(config.stopbits));
    
    // Configure UART parameters from passed configuration
    uart_config_t uart_config = {
        .baud_rate = (int)config.baudrate,
        .data_bits = config.databits,
        .parity = config.parity,
        .stop_bits = config.stopbits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,  // Will be set below if needed
        .rx_flow_ctrl_thresh = 122,             // Default threshold
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    // Enable flow control ONLY for UART1 when requested
    if (uart_num == UART_NUM_1 && config.flowcontrol) {
        uart_config.flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS;
        uart_config.rx_flow_ctrl_thresh = 100;  // ~78% of 128-byte FIFO
        flowControlEnabled = true;  // IMPORTANT: Set flag for status reporting
        log_msg(LOG_INFO, "UART1: Hardware flow control ENABLED (RTS=%d, CTS=%d)", 
                RTS_PIN, CTS_PIN);
    } else {
        flowControlEnabled = false;  // IMPORTANT: Clear flag when disabled
    }
    
    // Install UART driver with DMA using configured buffer sizes
    esp_err_t err = uart_driver_install(uart_num, DMA_RX_BUF_SIZE, DMA_TX_BUF_SIZE, 
                                        dmaConfig.eventQueueSize, &uart_queue, ESP_INTR_FLAG_IRAM);
    if (err != ESP_OK) {
        log_msg(LOG_ERROR, "UART driver install failed: %d", err);
        return;
    }
    
    // Configure UART parameters
    err = uart_param_config(uart_num, &uart_config);
    if (err != ESP_OK) {
        log_msg(LOG_ERROR, "UART param config failed: %d", err);
        uart_driver_delete(uart_num);
        return;
    }
    
    // Set pins - include RTS/CTS only for UART1 with flow control
    if (uart_num == UART_NUM_1 && flowControlEnabled) {  // Use the flag we just set
        err = uart_set_pin(uart_num, tx_pin, rx_pin, RTS_PIN, CTS_PIN);
    } else {
        err = uart_set_pin(uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    
    if (err != ESP_OK) {
        log_msg(LOG_ERROR, "UART set pin failed: %d", err);
        uart_driver_delete(uart_num);
        return;
    }
    
    // Set RX FIFO "full" interrupt threshold to 100 bytes.
    // This affects when UART_DATA event is generated due to FIFO fullness.
    // Note: This is NOT the same as rx_flow_ctrl_thresh (RTS hardware flow control).
    uart_set_rx_full_threshold(uart_num, 100);

    // Configure RX timeout (important for packet detection)
    // Timeout in UART symbols (bits). For 8N1, one symbol = 10 bits
    // At 115200 baud: 10 bits = ~87us per symbol
    // 23 symbols = ~2ms timeout
    uart_set_rx_timeout(uart_num, 23);
    
    // Enable RX interrupt
    uart_enable_rx_intr(uart_num);
    
    // Create event handling task only if configured
    if (dmaConfig.useEventTask) {
        if (xTaskCreatePinnedToCore(
            uartEventTask,
            "uart_dma_event",
            4096,        // Stack size
            this,
            dmaConfig.eventTaskPriority,
            &event_task_handle,
            UART_DMA_TASK_CORE
        ) != pdPASS) {
            log_msg(LOG_ERROR, "Failed to create UART event task");
            uart_driver_delete(uart_num);
            return;
        }
        log_msg(LOG_DEBUG, "UART DMA event task created with priority %d", dmaConfig.eventTaskPriority);
    } else {
        log_msg(LOG_DEBUG, "UART DMA initialized in polling mode (no event task)");
    }
    
    // Mark as successfully initialized
    initialized = true;
    
    log_msg(LOG_INFO, "UART DMA initialized: %u baud, pins RX=%d TX=%d%s, mode=%s",
            config.baudrate, rx_pin, tx_pin,
            (config.flowcontrol && uart_num == UART_NUM_1 ? " with flow control" : ""),
            dmaConfig.useEventTask ? "event" : "polling");
}

// Simplified begin - uses default UART configuration
void UartDMA::begin(uint32_t baudrate, int8_t rxPin, int8_t txPin) {
    // Create default UART configuration
    UartConfig config = {
        .baudrate = baudrate,
        .databits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stopbits = UART_STOP_BITS_1,
        .flowcontrol = false
    };
    
    begin(config, rxPin, txPin);
}

// Event handling task
void UartDMA::uartEventTask(void* pvParameters) {
    UartDMA* uart = (UartDMA*)pvParameters;
    uart_event_t event;
    uint8_t* dtmp = (uint8_t*)heap_caps_malloc(uart->DMA_RX_BUF_SIZE, MALLOC_CAP_DMA);
    
    if (!dtmp) {
        log_msg(LOG_ERROR, "Failed to allocate DMA event buffer");
        vTaskDelete(nullptr);
        return;
    }
    
    while (true) {
        // Wait for UART event
        if (xQueueReceive(uart->uart_queue, &event, portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA: {
                    // Read data from DMA buffer
                    int len = uart_read_bytes(uart->uart_num, dtmp, event.size, 0);
                    if (len > 0) {
                        uart->processRxData(dtmp, len);
                    }
                    break;
                }
                
                case UART_FIFO_OVF:
                    log_msg(LOG_WARNING, "UART FIFO overflow");
                    

                    uart->overrun_flag = true;
                    uart->overrun_count = uart->overrun_count + 1;
                    uart_flush_input(uart->uart_num);
                    xQueueReset(uart->uart_queue);
                    break;
                    
                case UART_BUFFER_FULL:
                    log_msg(LOG_WARNING, "UART ring buffer full");
                    uart->overrun_flag = true;
                    uart->overrun_count = uart->overrun_count + 1;
                    uart_flush_input(uart->uart_num);
                    xQueueReset(uart->uart_queue);
                    break;
                    
                case UART_BREAK: {
                    static uint32_t breakCount = 0;
                    static uint32_t lastBreakReport = 0;
    
                    breakCount++;
                    uint32_t now = millis();
    
                    // Report first and then every 10 seconds
                    if (breakCount == 1 || (now - lastBreakReport) > 10000) {
                        log_msg(LOG_DEBUG, "UART break detected (total: %u)", breakCount);
                        lastBreakReport = now;
                    }
                    break;
                }

                case UART_PARITY_ERR:
                    log_msg(LOG_WARNING, "UART parity error");
                    break;
                    
                case UART_FRAME_ERR:
                    log_msg(LOG_WARNING, "UART frame error");
                    break;
                    
                case UART_DATA_BREAK:
                    // RX timeout - indicates packet boundary
                    uart->packet_timeout_flag = true;
                    break;
                    
                default:
                    log_msg(LOG_DEBUG, "UART event type: %d", event.type);
                    break;
            }
        }
    }
    
    heap_caps_free(dtmp);
    vTaskDelete(nullptr);
}

// Poll events for non-event-task mode
void UartDMA::pollEvents() {
    if (!initialized || dmaConfig.useEventTask) {
        return;  // Only poll if initialized and not using event task
    }
    
    // Static buffer to avoid allocation on each call
    static uint8_t* poll_buffer = nullptr;
    if (!poll_buffer) {
        poll_buffer = (uint8_t*)heap_caps_malloc(DMA_RX_BUF_SIZE, MALLOC_CAP_DMA);
        if (!poll_buffer) {
            log_msg(LOG_ERROR, "Failed to allocate poll buffer");
            return;
        }
    }
    
    // Process all pending events without blocking
    uart_event_t event;
    while (xQueueReceive(uart_queue, &event, 0) == pdTRUE) {
        switch (event.type) {
            case UART_DATA: {
                // Read available data from UART
                size_t buffered_len = 0;
                uart_get_buffered_data_len(uart_num, &buffered_len);
                if (buffered_len > 0) {
                    int len = uart_read_bytes(uart_num, poll_buffer, buffered_len, 0);
                    if (len > 0) {
                        processRxData(poll_buffer, len);
                    }
                }
                break;
            }
            
            case UART_FIFO_OVF:
            case UART_BUFFER_FULL:
                overrun_flag = true;
                overrun_count = overrun_count + 1;
                uart_flush_input(uart_num);
                break;
                
            case UART_DATA_BREAK:
                // RX timeout - indicates packet boundary
                packet_timeout_flag = true;
                break;
                
            default:
                // Other events handled same as event task
                break;
        }
    }
    
    // Also check if there's data available even without event
    size_t buffered_len = 0;
    uart_get_buffered_data_len(uart_num, &buffered_len);
    if (buffered_len > 0) {
        int len = uart_read_bytes(uart_num, poll_buffer, 
                                 (buffered_len > DMA_RX_BUF_SIZE) ? DMA_RX_BUF_SIZE : buffered_len, 0);
        if (len > 0) {
            processRxData(poll_buffer, len);
        }
    }
}

// Process received data into ring buffer
void UartDMA::processRxData(uint8_t* data, size_t len) {
    if (xSemaphoreTake(rx_mutex, portMAX_DELAY) == pdTRUE) {
        rx_bytes_total = rx_bytes_total + len;
        
        for (size_t i = 0; i < len; i++) {
            size_t next_head = (rx_head + 1) % RING_BUF_SIZE;
            
            if (next_head == rx_tail) {
                // Buffer overflow
                overrun_flag = true;
                overrun_count = overrun_count + 1;
                log_msg(LOG_WARNING, "UART RX ring buffer overflow");
                break;
            }
            
            rx_ring_buf[rx_head] = data[i];
            rx_head = next_head;
        }
        
        xSemaphoreGive(rx_mutex);
    }
}

// Get number of bytes available in ring buffer
size_t UartDMA::getRxBytesAvailable() const {
    size_t head = rx_head;
    size_t tail = rx_tail;
    
    if (head >= tail) {
        return head - tail;
    } else {
        return RING_BUF_SIZE - tail + head;
    }
}

// Check how many bytes available to read
int UartDMA::available() {
    if (!initialized) return 0;
    
    if (xSemaphoreTake(rx_mutex, 0) == pdTRUE) {
        int avail = getRxBytesAvailable();
        xSemaphoreGive(rx_mutex);
        return avail;
    }
    return 0;
}

// Check available space in TX buffer
int UartDMA::availableForWrite() {
    if (!initialized) return 0;
    
    size_t free = 0;
    uart_get_tx_buffer_free_size(uart_num, &free);
    return free;
}

// Read single byte
int UartDMA::read() {
    if (!initialized) return -1;
    
    if (xSemaphoreTake(rx_mutex, portMAX_DELAY) == pdTRUE) {
        if (rx_head == rx_tail) {
            xSemaphoreGive(rx_mutex);
            return -1;  // No data
        }
        
        uint8_t byte = rx_ring_buf[rx_tail];
        rx_tail = (rx_tail + 1) % RING_BUF_SIZE;
        
        xSemaphoreGive(rx_mutex);
        return byte;
    }
    return -1;
}

// Batch read - takes mutex once, reads all available bytes
size_t UartDMA::readBytes(uint8_t* buffer, size_t maxLen) {
    if (!initialized || maxLen == 0) return 0;
    
    // Non-blocking mutex - return 0 if busy
    if (xSemaphoreTake(rx_mutex, 0) != pdTRUE) {
        return 0;
    }
    
    size_t avail = getRxBytesAvailable();
    size_t toRead = min(avail, maxLen);
    
    if (toRead > 0) {
        // Efficient copy with wrap handling
        if (rx_tail + toRead <= RING_BUF_SIZE) {
            // No wrap - single memcpy
            memcpy(buffer, &rx_ring_buf[rx_tail], toRead);
            rx_tail = (rx_tail + toRead) % RING_BUF_SIZE;
        } else {
            // Handle wrap - two memcpy operations
            size_t firstPart = RING_BUF_SIZE - rx_tail;
            memcpy(buffer, &rx_ring_buf[rx_tail], firstPart);
            memcpy(buffer + firstPart, &rx_ring_buf[0], toRead - firstPart);
            rx_tail = toRead - firstPart;
        }
    }
    
    xSemaphoreGive(rx_mutex);
    return toRead;
}

// Write single byte
size_t UartDMA::write(uint8_t data) {
    if (!initialized) return 0;
    return write(&data, 1);
}

// Write buffer
size_t UartDMA::write(const uint8_t* buffer, size_t size) {
    if (!initialized) return 0;
    
    if (xSemaphoreTake(tx_mutex, portMAX_DELAY) == pdTRUE) {
        int written = uart_write_bytes(uart_num, buffer, size);
        if (written > 0) {
            tx_bytes_total = tx_bytes_total + written;
        }
        xSemaphoreGive(tx_mutex);
        return written > 0 ? written : 0;
    }
    return 0;
}

// Flush TX buffer
void UartDMA::flush() {
    if (!initialized) return;
    
    if (xSemaphoreTake(tx_mutex, portMAX_DELAY) == pdTRUE) {
        uart_wait_tx_done(uart_num, portMAX_DELAY);
        xSemaphoreGive(tx_mutex);
    }
}

// Close UART
void UartDMA::end() {
    initialized = false;
    
    // Stop event task if it exists
    if (event_task_handle) {
        vTaskDelete(event_task_handle);
        event_task_handle = nullptr;
    }
    
    // Uninstall driver
    uart_driver_delete(uart_num);
    
    // Reset state
    rx_head = 0;
    rx_tail = 0;
    packet_timeout_flag = false;
    overrun_flag = false;
}

// Set RX buffer size (no-op for DMA as size is fixed at creation)
void UartDMA::setRxBufferSize(size_t size) {
    log_msg(LOG_DEBUG, "UART DMA RX buffer size is fixed at %d (configured at creation)", RING_BUF_SIZE);
}

// Check if packet timeout occurred
bool UartDMA::hasPacketTimeout() {
    return packet_timeout_flag.exchange(false);
}

// Check if overrun occurred
bool UartDMA::hasOverrun() {
    return overrun_flag.exchange(false);
}

// Get flow control status string
String UartDMA::getFlowControlStatus() const {
    if (uart_num != UART_NUM_1) {
        return "Not supported";
    }
    
    if (!flowControlEnabled) {
        return "Disabled";
    }
    
    // ESP-IDF handles flow control transparently
    // If enabled, it's active
    return "Enabled (Active)";
}