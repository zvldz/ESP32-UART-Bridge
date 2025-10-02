#ifndef PACKET_SENDER_H
#define PACKET_SENDER_H

#include "protocol_types.h"
#include "../types.h"
#include "../logging.h"
#include <deque>
#include <Arduino.h>

// PacketSender buffer size constants
static constexpr size_t DEFAULT_MAX_PACKETS = 20;
static constexpr size_t DEFAULT_MAX_BYTES = 8192;
static constexpr size_t USB_MAX_PACKETS = 128; 
static constexpr size_t USB_MAX_BYTES = 24576;

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
    
public:
    PacketSender(size_t maxPackets = DEFAULT_MAX_PACKETS, size_t maxBytes = DEFAULT_MAX_BYTES) : 
        maxQueuePackets(maxPackets),
        maxQueueBytes(maxBytes),
        currentQueueBytes(0),
        totalSent(0), 
        totalDropped(0),
        maxQueueDepth(0) {}
    
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
        
        ParsedPacket finalCopy = packet.duplicate();
        
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
    
    // Direct send without queue (for fast path protocols like SBUS)
    // Must be implemented by all senders
    // Returns number of bytes sent (may be 0 on error)
    virtual size_t sendDirect(const uint8_t* data, size_t size) = 0;

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