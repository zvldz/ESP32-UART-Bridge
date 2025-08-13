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
        PacketSender(20, 8192, txCounter),  // Pass TX counter to base class
        batchSize(0),
        lastBatchTime(0) {
        log_msg(LOG_INFO, "UdpSender initialized");
    }
    
    ~UdpSender() {
        // Flush any pending batch before destruction
        flushBatch();
    }
    
    void processSendQueue() override {
        uint32_t now = micros();
        
        while (!packetQueue.empty()) {
            QueuedPacket& item = packetQueue.front();
            
            // UDP doesn't support partial send - wait if packet doesn't fit
            if (item.sendOffset > 0) {
                log_msg(LOG_ERROR, "UDP: Partial send not supported!");
                // Skip this packet
                item.packet.free();
                packetQueue.pop();
                currentQueueBytes -= item.packet.size;
                continue;
            }
            
            if (item.packet.hints.keepWhole) {
                // Send as separate UDP datagram
                flushBatch();  // Flush any pending batch first
                sendUdpDatagram(item.packet.data, item.packet.size);
                totalSent++;
            } else if (item.packet.hints.canBatch) {
                // Add to batch
                if (batchSize + item.packet.size > sizeof(batchBuffer)) {
                    flushBatch();  // Batch full, flush first
                }
                
                if (item.packet.size <= sizeof(batchBuffer)) {
                    memcpy(batchBuffer + batchSize, item.packet.data, item.packet.size);
                    batchSize += item.packet.size;
                    lastBatchTime = now;
                } else {
                    // Single packet too big for batch - send alone
                    flushBatch();
                    sendUdpDatagram(item.packet.data, item.packet.size);
                    totalSent++;
                }
            } else {
                // Send immediately
                flushBatch();
                sendUdpDatagram(item.packet.data, item.packet.size);
                totalSent++;
            }
            
            // Packet processed - free and remove
            currentQueueBytes -= item.packet.size;
            item.packet.free();
            packetQueue.pop();
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