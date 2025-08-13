#ifndef PACKET_SENDER_H
#define PACKET_SENDER_H

#include "protocol_types.h"
#include "../types.h"
#include "../logging.h"
#include <queue>
#include <Arduino.h>

// Queued packet with send progress tracking
struct QueuedPacket {
    ParsedPacket packet;
    size_t sendOffset;      // How much already sent (for partial send)
    uint32_t enqueueTime;   // When packet was queued
    
    QueuedPacket() : sendOffset(0), enqueueTime(0) {}
    QueuedPacket(const ParsedPacket& p) : packet(p), sendOffset(0) {
        enqueueTime = micros();
    }
};

class PacketSender {
protected:
    std::queue<QueuedPacket> packetQueue;
    
    // Queue limits
    size_t maxQueuePackets;    // Max number of packets
    size_t maxQueueBytes;      // Max total bytes in queue
    size_t currentQueueBytes;  // Current total bytes
    
    // Statistics
    uint32_t totalSent;
    uint32_t totalDropped;
    uint32_t dropBulk;         // Dropped BULK priority
    uint32_t dropNormal;       // Dropped NORMAL priority
    uint32_t maxQueueDepth;    // Max queue depth seen
    unsigned long* globalTxBytesCounter;  // Pointer to global TX counter
    
public:
    PacketSender(size_t maxPackets = 20, size_t maxBytes = 8192, 
                 unsigned long* txCounter = nullptr) : 
        maxQueuePackets(maxPackets),
        maxQueueBytes(maxBytes),
        currentQueueBytes(0),
        totalSent(0), 
        totalDropped(0),
        dropBulk(0),
        dropNormal(0),
        maxQueueDepth(0),
        globalTxBytesCounter(txCounter) {}
    
    virtual ~PacketSender() {
        // Clean queue - CRITICAL: free all packets
        while (!packetQueue.empty()) {
            packetQueue.front().packet.free();
            packetQueue.pop();
        }
        currentQueueBytes = 0;
    }
    
    // Check if sender will accept packet
    bool willAccept(size_t size) const {
        if (packetQueue.size() >= maxQueuePackets) return false;
        if (currentQueueBytes + size > maxQueueBytes) return false;
        return true;
    }
    
    // Add packet to queue (makes copy)
    virtual bool enqueue(const ParsedPacket& packet) {
        // Check limits
        if (!willAccept(packet.size)) {
            // Try to make space by dropping lower priority
            if (packet.priority == PRIORITY_CRITICAL) {
                // Critical packet - try to drop BULK first
                if (!dropLowestPriority()) {
                    // Couldn't make space
                    totalDropped++;
                    log_msg(LOG_WARNING, "%s: Dropped CRITICAL packet (queue full)", getName());
                    return false;
                }
            } else {
                // Non-critical - just drop it
                totalDropped++;
                if (packet.priority == PRIORITY_BULK) dropBulk++;
                else if (packet.priority == PRIORITY_NORMAL) dropNormal++;
                return false;
            }
        }
        
        // Make copy and enqueue
        ParsedPacket copy = packet.duplicate();
        if (!copy.data) {
            // Memory allocation failed
            log_msg(LOG_ERROR, "%s: Failed to duplicate packet", getName());
            totalDropped++;
            return false;
        }
        
        packetQueue.push(QueuedPacket(copy));
        currentQueueBytes += copy.size;
        
        // Update max depth
        if (packetQueue.size() > maxQueueDepth) {
            maxQueueDepth = packetQueue.size();
        }
        
        return true;
    }
    
    // Process queue and send packets (MUST handle partial send!)
    virtual void processSendQueue() = 0;
    
    // Check if device is ready
    virtual bool isReady() const = 0;
    
    // Get device name for logging
    virtual const char* getName() const = 0;
    
    // Get statistics
    uint32_t getSentCount() const { return totalSent; }
    uint32_t getDroppedCount() const { return totalDropped; }
    size_t getQueueDepth() const { return packetQueue.size(); }
    size_t getQueueBytes() const { return currentQueueBytes; }
    
    // Get detailed drop statistics
    uint32_t getDroppedBulk() const { return dropBulk; }
    uint32_t getDroppedNormal() const { return dropNormal; }
    uint32_t getDroppedCritical() const { return totalDropped - dropBulk - dropNormal; }
    size_t getMaxQueueDepth() const { return maxQueueDepth; }
    
    // Get total bytes sent estimate (for display)
    uint32_t getTotalBytesSent() const { 
        // Calculate from packets sent * average size
        // This is internal counter, different from global device counter
        return totalSent * 100;  // Approximate
    }
    
    void getDetailedStats(char* buffer, size_t bufSize) {
        snprintf(buffer, bufSize,
                "%s: Sent=%u Dropped=%u (B:%u/N:%u) Queue=%zu/%zu MaxQ=%u",
                getName(), totalSent, totalDropped, dropBulk, dropNormal,
                packetQueue.size(), currentQueueBytes, maxQueueDepth);
    }
    
protected:
    // Try to drop lowest priority packet to make space
    bool dropLowestPriority() {
        // Find and drop first BULK packet
        std::queue<QueuedPacket> tempQueue;
        bool dropped = false;
        
        while (!packetQueue.empty()) {
            QueuedPacket& item = packetQueue.front();
            
            if (!dropped && item.packet.priority == PRIORITY_BULK && item.sendOffset == 0) {
                // Drop this packet (only if not partially sent!)
                currentQueueBytes -= item.packet.size;
                item.packet.free();
                totalDropped++;
                dropBulk++;
                dropped = true;
            } else {
                tempQueue.push(item);
            }
            packetQueue.pop();
        }
        
        // Restore queue
        packetQueue = tempQueue;
        return dropped;
    }
};

#endif // PACKET_SENDER_H