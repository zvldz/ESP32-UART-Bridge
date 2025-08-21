#ifndef PACKET_SENDER_H
#define PACKET_SENDER_H

#include "protocol_types.h"
#include "../types.h"
#include "../logging.h"
#include <deque>
#include <Arduino.h>

// Queued packet with send progress tracking
struct QueuedPacket {
    ParsedPacket packet;
    size_t sendOffset;      // For UART partial send support (UDP/USB ignore this)
    uint32_t enqueueTime;   // When packet was queued
    
    QueuedPacket() : sendOffset(0), enqueueTime(0) {}
    QueuedPacket(const ParsedPacket& p) : packet(p), sendOffset(0) {
        enqueueTime = micros();
    }
};

class PacketSender {
protected:
    // Single FIFO queue
    std::deque<QueuedPacket> packetQueue;
    
    // Queue limits
    size_t maxQueuePackets;    // Max number of packets
    size_t maxQueueBytes;      // Max total bytes in queue
    size_t currentQueueBytes;  // Current total bytes
    
    // Statistics
    uint32_t totalSent;
    uint32_t totalDropped;
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
        maxQueueDepth(0),
        globalTxBytesCounter(txCounter) {}
    
    virtual ~PacketSender() {
        // Clean queue - CRITICAL: free all packets
        while (!packetQueue.empty()) {
            packetQueue.front().packet.free();
            packetQueue.pop_front();
        }
        currentQueueBytes = 0;
    }
    
    // Check if sender will accept packet
    bool willAccept(size_t size) const {
        return packetQueue.size() < maxQueuePackets && 
               currentQueueBytes + size <= maxQueueBytes;
    }
    
    // Add packet to queue
    virtual bool enqueue(const ParsedPacket& packet) {
        // Check if we can accept
        if (!willAccept(packet.size)) {
            // Try to drop oldest packet to make room
            if (!dropOldestPacket()) {
                totalDropped++;
                return false;
            }
        }
        
        // === DIAGNOSTIC START === (Remove after MAVLink stabilization)
        // Track enqueue time
        ParsedPacket copy = packet;
        copy.enqueueTimeMicros = micros();
        
        // Check parsing latency
        uint32_t parseLatency = copy.enqueueTimeMicros - copy.parseTimeMicros;
        if (parseLatency > 5000) {  // > 5ms
            log_msg(LOG_WARNING, "[DIAG] High parse->enq latency: %ums for seq=%u msgid=%u",
                    parseLatency/1000, copy.seqNum, copy.mavlinkMsgId);
        }
        
        // Make copy and enqueue (use modified copy with diagnostic data)
        ParsedPacket finalCopy = copy.duplicate();
        // === DIAGNOSTIC END ===
        // === ORIGINAL: ParsedPacket copy = packet.duplicate();
        
        if (!finalCopy.data) {
            log_msg(LOG_ERROR, "%s: Failed to duplicate packet", getName());
            totalDropped++;
            return false;
        }
        
        // Enqueue
        QueuedPacket qp(finalCopy);
        packetQueue.push_back(qp);
        currentQueueBytes += finalCopy.size;
        
        // Update max depth
        if (packetQueue.size() > maxQueueDepth) {
            maxQueueDepth = packetQueue.size();
        }
        
        return true;
    }
    
    // Process queue and send packets (MUST handle partial send!)
    // @param bulkMode - true if parser detected bulk transfer
    virtual void processSendQueue(bool bulkMode = false) = 0;
    
    // Check if device is ready
    virtual bool isReady() const = 0;
    
    // Get device name for logging
    virtual const char* getName() const = 0;
    
    // Get statistics
    uint32_t getSentCount() const { return totalSent; }
    uint32_t getDroppedCount() const { return totalDropped; }
    size_t getQueueDepth() const { return packetQueue.size(); }
    size_t getQueueBytes() const { return currentQueueBytes; }
    size_t getMaxQueueDepth() const { return maxQueueDepth; }
    
    
    void getDetailedStats(char* buffer, size_t bufSize) {
        snprintf(buffer, bufSize,
                "%s: Sent=%u Dropped=%u Queue=%zu/%zu bytes=%zu/%zu",
                getName(), totalSent, totalDropped,
                packetQueue.size(), maxQueuePackets,
                currentQueueBytes, maxQueueBytes);
    }
    
protected:
    // Drop oldest packet if queue full
    bool dropOldestPacket() {
        if (!packetQueue.empty()) {
            currentQueueBytes -= packetQueue.front().packet.size;
            packetQueue.front().packet.free();
            packetQueue.pop_front();
            totalDropped++;
            return true;
        }
        return false;
    }
};

#endif // PACKET_SENDER_H