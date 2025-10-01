#include "uart1_tx_service.h"
#include "../logging.h"
#include "../device_stats.h"

// Singleton instance
static Uart1TxService* s_instance = nullptr;

Uart1TxService* Uart1TxService::getInstance() {
    if (!s_instance) {
        s_instance = new Uart1TxService();
    }
    return s_instance;
}

Uart1TxService::Uart1TxService() : 
    txRing(nullptr), uart(nullptr), ringMutex(nullptr),
    totalBytes(0), droppedBytes(0), writeErrors(0),
    maxWritePerCall(1024) {
}

Uart1TxService::~Uart1TxService() {
    if (txRing) {
        delete txRing;
        txRing = nullptr;
    }
    if (ringMutex) {
        vSemaphoreDelete(ringMutex);
        ringMutex = nullptr;
    }
}

bool Uart1TxService::init(UartInterface* uartInterface, size_t ringSize) {
    // Get config reference
    extern Config config;

    // Don't create TX resources for SBUS_IN role
    if (config.device1.role == D1_SBUS_IN) {
        uart = uartInterface;  // Store interface but don't create ring
        log_msg(LOG_INFO, "UART1 TX service disabled for SBUS_IN role (saving ~4KB)");
        return true;
    }

    // Normal initialization continues
    uart = uartInterface;

    // Create TX ring buffer
    txRing = new CircularBuffer();
    txRing->init(ringSize);
    log_msg(LOG_INFO, "UART1 TX ring initialized: %zu bytes", ringSize);

    // Create mutex for thread-safety
    ringMutex = xSemaphoreCreateMutex();
    if (!ringMutex) {
        log_msg(LOG_ERROR, "Failed to create UART1 TX mutex");
        return false;
    }

    log_msg(LOG_INFO, "UART1 TX service initialized: %zu byte ring", ringSize);
    return true;
}

bool Uart1TxService::enqueue(const uint8_t* data, size_t len) {
    if (!txRing || !ringMutex) return false;
    
    // Mutex needed: can be called from UDP callback (lwIP task on core 0)
    xSemaphoreTake(ringMutex, portMAX_DELAY);
    
    // FIFO with eviction if full
    size_t free = txRing->freeSpace();
    if (free < len) {
        size_t toDrop = len - free;
        txRing->consume(toDrop);  // Drop oldest data
        droppedBytes += toDrop;
        log_msg(LOG_DEBUG, "UART1 TX: Dropped %zu old bytes", toDrop);
    }
    
    size_t written = txRing->write(data, len);
    
    xSemaphoreGive(ringMutex);
    
    return written == len;
}

void Uart1TxService::processTxQueue() {
    if (!txRing) return;  // Skip if no TX ring (SBUS_IN mode)
    if (!uart || !ringMutex) return;
    
    // Quick check without mutex
    if (txRing->available() == 0) return;
    if (uart->availableForWrite() == 0) return;
    
    // === TEMPORARY DIAGNOSTIC BLOCK START ===
    // Temporary diagnostic for TX queue monitoring
    static uint32_t lastLog = 0;
    if (millis() - lastLog > 1000) {
        size_t available = txRing->available();
        size_t canWrite = uart->availableForWrite();
        log_msg(LOG_DEBUG, "TX Queue: ring=%zu canWrite=%zu", available, canWrite);
        lastLog = millis();
    }
    // === TEMPORARY DIAGNOSTIC BLOCK END ===
    
    // Short mutex only for reading segments (ring can be modified by enqueue)
    xSemaphoreTake(ringMutex, portMAX_DELAY);
    
    size_t totalWritten = 0;
    while (txRing->available() > 0 && totalWritten < maxWritePerCall) {
        size_t canWrite = uart->availableForWrite();
        if (canWrite == 0) break;
        
        // Get read segments (handle wrap-around)
        auto segments = txRing->getReadSegments();
        
        // Write first segment
        if (segments.first.size > 0) {
            size_t toWrite = min(segments.first.size, 
                                min(canWrite, maxWritePerCall - totalWritten));
            size_t written = uart->write(segments.first.data, toWrite);
            
            if (written > 0) {
                txRing->consume(written);
                totalWritten += written;
                totalBytes += written;
                g_deviceStats.device1.txBytes.fetch_add(written, std::memory_order_relaxed);
            } else {
                writeErrors++;
                break;
            }
        }
        
        // Write second segment if any
        if (segments.second.size > 0 && totalWritten < maxWritePerCall) {
            size_t canWrite = uart->availableForWrite();
            if (canWrite > 0) {
                size_t toWrite = min(segments.second.size,
                                   min(canWrite, maxWritePerCall - totalWritten));
                size_t written = uart->write(segments.second.data, toWrite);
                
                if (written > 0) {
                    txRing->consume(written);
                    totalWritten += written;
                    totalBytes += written;
                    g_deviceStats.device1.txBytes.fetch_add(written, std::memory_order_relaxed);
                }
            }
        }
    }
    
    xSemaphoreGive(ringMutex);
}