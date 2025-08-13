#ifndef PACKET_MEMORY_POOL_H
#define PACKET_MEMORY_POOL_H

#include "protocol_types.h"
#include "../logging.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Simple slab allocator for packet buffers
template<size_t BlockSize, size_t BlockCount>
class MemoryPool {
private:
    struct Block {
        uint8_t data[BlockSize];
        bool inUse;
    };
    
    Block blocks[BlockCount];
    SemaphoreHandle_t mutex;
    uint32_t allocCount;
    uint32_t freeCount;
    uint32_t failCount;
    
public:
    MemoryPool() : allocCount(0), freeCount(0), failCount(0) {
        mutex = xSemaphoreCreateMutex();
        for (size_t i = 0; i < BlockCount; i++) {
            blocks[i].inUse = false;
        }
    }
    
    uint8_t* allocate() {
        xSemaphoreTake(mutex, portMAX_DELAY);
        
        for (size_t i = 0; i < BlockCount; i++) {
            if (!blocks[i].inUse) {
                blocks[i].inUse = true;
                allocCount++;
                xSemaphoreGive(mutex);
                return blocks[i].data;
            }
        }
        
        failCount++;
        xSemaphoreGive(mutex);
        return nullptr;  // Pool exhausted
    }
    
    void deallocate(uint8_t* ptr) {
        if (!ptr) return;
        
        xSemaphoreTake(mutex, portMAX_DELAY);
        
        for (size_t i = 0; i < BlockCount; i++) {
            if (blocks[i].data == ptr) {
                blocks[i].inUse = false;
                freeCount++;
                xSemaphoreGive(mutex);
                return;
            }
        }
        
        // Not from this pool - error!
        log_msg(LOG_ERROR, "Pool: Invalid deallocation attempt!");
        xSemaphoreGive(mutex);
    }
    
    size_t getBlockSize() const { return BlockSize; }
    uint32_t getAllocCount() const { return allocCount; }
    uint32_t getFreeCount() const { return freeCount; }
    uint32_t getFailCount() const { return failCount; }
};

// Global packet memory pool manager
class PacketMemoryPool {
private:
    // Pools for different packet sizes
    MemoryPool<64, 30> smallPool;     // Control packets (30 blocks)
    MemoryPool<128, 20> mediumPool;   // For RAW chunks (120-240 divided in half)
    MemoryPool<288, 20> mavlinkPool;  // MAVLink v2 max (20 blocks)
    MemoryPool<512, 10> rawPool;      // RAW chunks (10 blocks)
    
    // Private constructor for singleton
    PacketMemoryPool() = default;
    
public:
    static PacketMemoryPool* getInstance() {
        static PacketMemoryPool instance;  // Thread-safe in C++11+
        return &instance;
    }
    
    uint8_t* allocate(size_t size, size_t& allocatedSize) {
        uint8_t* ptr = nullptr;
        
        if (size <= 64) {
            ptr = smallPool.allocate();
            allocatedSize = 64;
        } else if (size <= 128) {
            ptr = mediumPool.allocate();
            allocatedSize = 128;            
        } else if (size <= 288) {
            ptr = mavlinkPool.allocate();
            allocatedSize = 288;
        } else if (size <= 512) {
            ptr = rawPool.allocate();
            allocatedSize = 512;
        } else {
            // Too big for pools - fall back to heap (with warning)
            log_msg(LOG_WARNING, "Pool: Size %zu too big, using heap", size);
            ptr = new uint8_t[size];
            allocatedSize = size;
        }
        
        if (!ptr) {
            // Pool exhausted - fall back to heap
            log_msg(LOG_WARNING, "Pool exhausted for size %zu, using heap", size);
            ptr = new uint8_t[size];
            allocatedSize = size;
        }
        
        return ptr;
    }
    
    void deallocate(uint8_t* ptr, size_t allocatedSize) {
        if (!ptr) return;
        
        if (allocatedSize == 64) {
            smallPool.deallocate(ptr);
        } else if (allocatedSize == 128) {
            mediumPool.deallocate(ptr);
        } else if (allocatedSize == 288) {
            mavlinkPool.deallocate(ptr);
        } else if (allocatedSize == 512) {
            rawPool.deallocate(ptr);
        } else {
            // Was allocated from heap
            delete[] ptr;
        }
    }
    
    void getStats(char* buffer, size_t bufSize) {
        snprintf(buffer, bufSize,
                "Pool Stats:\n"
                "  Small(64B): alloc=%u free=%u fail=%u\n"
                "  MAVLink(288B): alloc=%u free=%u fail=%u\n"
                "  RAW(512B): alloc=%u free=%u fail=%u\n",
                smallPool.getAllocCount(), smallPool.getFreeCount(), smallPool.getFailCount(),
                mavlinkPool.getAllocCount(), mavlinkPool.getFreeCount(), mavlinkPool.getFailCount(),
                rawPool.getAllocCount(), rawPool.getFreeCount(), rawPool.getFailCount());
    }
};

// Implementation of ParsedPacket methods
inline ParsedPacket ParsedPacket::duplicate() const {
    ParsedPacket copy = *this;
    
    // Allocate from pool
    copy.pool = PacketMemoryPool::getInstance();
    copy.data = copy.pool->allocate(size, copy.allocSize);
    
    if (copy.data) {
        memcpy(copy.data, data, size);
    } else {
        // Allocation failed
        copy.size = 0;
        copy.allocSize = 0;
    }
    
    return copy;
}

inline void ParsedPacket::free() {
    if (data) {
        if (pool) {
            pool->deallocate(data, allocSize);
        } else {
            // Was allocated directly
            delete[] data;
        }
        data = nullptr;
        size = 0;
        allocSize = 0;
    }
}

#endif // PACKET_MEMORY_POOL_H