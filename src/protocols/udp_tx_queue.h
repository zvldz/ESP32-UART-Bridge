#ifndef UDP_TX_QUEUE_H
#define UDP_TX_QUEUE_H

#include <Arduino.h>
#include <string.h>

/**
 * Single Producer Single Consumer (SPSC) Queue for UDP packets
 * Producer: UdpSender on Core 0
 * Consumer: device4Task on Core 1
 * 
 * CRITICAL: Only one producer and one consumer allowed!
 * Uses memory barriers for inter-core synchronization on ESP32
 */
class UdpTxQueue {
private:
    struct Packet {
        uint16_t size;
        uint8_t data[1500];  // MTU size
    };
    
    static constexpr uint32_t SLOTS = 16;  // 4 slots, can increase to 8 if needed
    Packet slots[SLOTS];
    
    // Separate cache lines to avoid false sharing
    volatile uint32_t head = 0;  // Written by Core 0 (producer - UdpSender)
    volatile uint32_t tail = 0;  // Written by Core 1 (consumer - device4Task)
    
public:
    // Producer side (Core 0 - UdpSender)
    bool enqueue(const uint8_t* data, size_t size) {
        if (size > sizeof(Packet::data)) {
            return false;  // Packet too large
        }
        
        uint32_t next = (head + 1) % SLOTS;
        if (next == tail) {
            return false;  // Queue full
        }
        
        // Copy data to slot
        slots[head].size = (uint16_t)size;
        memcpy(slots[head].data, data, size);
        
        // Memory barrier for inter-core synchronization
        __sync_synchronize();  // Release semantics
        
        head = next;
        return true;
    }
    
    // Consumer side (Core 1 - device4Task)
    size_t dequeue(uint8_t* buffer, size_t maxSize) {
        if (head == tail) {
            return 0;  // Empty
        }
        
        uint16_t size = slots[tail].size;
        if (size > maxSize) {
            size = (uint16_t)maxSize;
        }
        
        memcpy(buffer, slots[tail].data, size);
        
        __sync_synchronize();  // Acquire semantics
        
        tail = (tail + 1) % SLOTS;
        return size;
    }
    
    bool isEmpty() const { return head == tail; }
    bool isFull() const { return ((head + 1) % SLOTS) == tail; }
    uint32_t getCount() const {
        uint32_t h = head;
        uint32_t t = tail;
        return (h >= t) ? (h - t) : (SLOTS - t + h);
    }
};

#endif // UDP_TX_QUEUE_H