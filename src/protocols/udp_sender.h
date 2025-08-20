#ifndef UDP_SENDER_H
#define UDP_SENDER_H

#include "packet_sender.h"
#include "network_functions.h"

class UdpSender : public PacketSender {
private:
    // Buffer for batching non-whole packets
    uint8_t batchBuffer[1400];  // MTU safe size
    size_t batchSize;
    uint32_t lastBatchTime;
    uint32_t lastStatsLog;   // For periodic statistics logging
    static constexpr uint32_t BATCH_TIMEOUT_US = 5000;  // 5ms
    
    void flushBatch() {
        if (batchSize > 0) {
            // Send UDP datagram
            sendUdpDatagram(batchBuffer, batchSize);
            batchSize = 0;
            totalSent++;  // Count batch as one send
        }
    }
    
public:
    UdpSender(unsigned long* txCounter = nullptr) : 
        PacketSender(20, 8192, txCounter),  // Return to original sizes
        batchSize(0),
        lastBatchTime(0),
        lastStatsLog(0) {
        log_msg(LOG_DEBUG, "UdpSender initialized");
    }
    
    ~UdpSender() {
        // Flush any pending batch before destruction
        flushBatch();
    }
    
    void processSendQueue(bool bulkMode = false) override {
        // UDP sender may use bulkMode for optimizations
        uint32_t now = micros();
        
        // Process packets from FIFO queue
        while (!packetQueue.empty()) {
            QueuedPacket* item = &packetQueue.front();
            
            // UDP doesn't support partial send - skip malformed packets
            if (item->sendOffset > 0) {
                log_msg(LOG_ERROR, "UDP: Partial send not supported!");
                currentQueueBytes -= item->packet.size;
                item->packet.free();
                packetQueue.pop_front();
                continue;
            }
            
            // KEEP BATCHING LOGIC - this was added after priorities!
            if (item->packet.hints.keepWhole) {
                // Send as separate UDP datagram (MAVLink packets)
                flushBatch();  // Flush any pending batch first
                sendUdpDatagram(item->packet.data, item->packet.size);
                totalSent++;
            } else if (item->packet.hints.canBatch) {
                // Add to batch (non-MAVLink data)
                if (batchSize + item->packet.size > sizeof(batchBuffer)) {
                    flushBatch();  // Batch full, flush first
                }
                
                if (item->packet.size <= sizeof(batchBuffer)) {
                    memcpy(batchBuffer + batchSize, item->packet.data, item->packet.size);
                    batchSize += item->packet.size;
                    lastBatchTime = now;
                } else {
                    // Single packet too big for batch - send alone
                    flushBatch();
                    sendUdpDatagram(item->packet.data, item->packet.size);
                    totalSent++;
                }
            } else {
                // Send immediately
                flushBatch();
                sendUdpDatagram(item->packet.data, item->packet.size);
                totalSent++;
            }
            
            // Packet processed - remove from queue
            currentQueueBytes -= item->packet.size;
            item->packet.free();
            packetQueue.pop_front();
        }
        
        // Check batch timeout
        if (batchSize > 0 && (now - lastBatchTime) > BATCH_TIMEOUT_US) {
            flushBatch();
        }
    }
    
    bool isReady() const override {
        return true;  // UDP is always ready (connectionless)
    }
    
    const char* getName() const override {
        return "UDP";
    }
    
private:
    void sendUdpDatagram(uint8_t* data, size_t size) {
        // Actual UDP send implementation
        // This depends on your network stack
        addToDevice4BridgeTx(data, size);
        
        // Update global TX counter
        if (globalTxBytesCounter) {
            *globalTxBytesCounter += size;
        }
    }
};

#endif // UDP_SENDER_H