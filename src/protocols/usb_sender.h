#ifndef USB_SENDER_H
#define USB_SENDER_H

#include "packet_sender.h"
#include "../usb/usb_interface.h"

class UsbSender : public PacketSender {
private:
    UsbInterface* usbInterface;
    uint32_t lastSendAttempt;
    uint32_t backoffDelay;  // Exponential backoff for USB congestion
    uint32_t lastStatsLog;   // For periodic statistics logging
    
    // Micro-batching for bulk mode
    static constexpr size_t BATCH_BUFFER_SIZE = 2048;
    static constexpr size_t MAX_BATCH_PACKETS = 8;
    uint8_t batchBuffer[BATCH_BUFFER_SIZE];
    bool bulkModeActive = false;  // Will be updated from parser
    
public:
    UsbSender(UsbInterface* usb, unsigned long* txCounter = nullptr) : 
        PacketSender(30, 10240, txCounter),  // Return to original sizes
        usbInterface(usb),
        lastSendAttempt(0),
        backoffDelay(0),
        lastStatsLog(0) {
        log_msg(LOG_DEBUG, "UsbSender initialized");
    }
    
    void processSendQueue() override {
        uint32_t now = millis();
        
        // === DIAGNOSTIC START === (Remove after MAVLink stabilization)
        static uint32_t diagSentCount = 0;
        static uint32_t diagHighLatencyCount = 0;
        static uint32_t lastDiagReportMs = 0;
        // === DIAGNOSTIC END ===
        
        // Backoff if USB was congested
        if (backoffDelay > 0) {
            uint32_t nowMicros = micros();
            if (nowMicros - lastSendAttempt < backoffDelay) {
                return;  // Still in backoff period
            }
            backoffDelay = 0;  // Reset backoff
        }
        
        // Check if bulk mode is active using urgentFlush hint as indicator
        if (!packetQueue.empty()) {
            bulkModeActive = packetQueue.front().packet.hints.urgentFlush;
        }
        
        // MICRO-BATCHING for bulk mode
        if (bulkModeActive && !packetQueue.empty() && isReady()) {
            size_t batchSize = 0;
            int batchedPackets = 0;
            
            // Collect multiple packets into batch buffer
            while (!packetQueue.empty() && 
                   batchedPackets < MAX_BATCH_PACKETS && 
                   batchSize < BATCH_BUFFER_SIZE - 280) {  // Leave room for max packet
                
                QueuedPacket* item = &packetQueue.front();
                
                // Skip partially sent packets
                if (item->sendOffset > 0) {
                    break;  // Handle normally below
                }
                
                // Check if packet fits
                if (batchSize + item->packet.size > BATCH_BUFFER_SIZE) {
                    break;  // Batch full
                }
                
                // Add to batch
                memcpy(batchBuffer + batchSize, item->packet.data, item->packet.size);
                batchSize += item->packet.size;
                batchedPackets++;
                
                // Free packet and remove from queue
                totalSent++;
                currentQueueBytes -= item->packet.size;
                item->packet.free();
                packetQueue.pop();
            }
            
            // Send batch if collected
            if (batchSize > 0) {
                size_t sent = usbInterface->write(batchBuffer, batchSize);
                
                if (globalTxBytesCounter) {
                    *globalTxBytesCounter += sent;
                }
                
                // Log micro-batch event (temporary)
                static uint32_t batchCount = 0;
                if (++batchCount % 10 == 0) {
                    log_msg(LOG_DEBUG, "USB micro-batch #%u: %d packets, %zu bytes",
                            batchCount, batchedPackets, batchSize);
                }
                
                return;  // Exit after batch send
            }
        }
        
        // Normal single-packet processing (for non-bulk mode or partially sent packets)
        while (isReady() && !packetQueue.empty()) {
            QueuedPacket* item = &packetQueue.front();
            
            // === DIAGNOSTIC START === (Remove after MAVLink stabilization)
            uint32_t nowMicros = micros();
            uint32_t totalLatency = nowMicros - item->packet.parseTimeMicros;
            uint32_t queueLatency = nowMicros - item->packet.enqueueTimeMicros;
            
            if (totalLatency > 15000) {  // > 15ms total
                log_msg(LOG_WARNING, "[DIAG] High total latency: %ums (queue=%ums) seq=%u msgid=%u",
                        totalLatency/1000, queueLatency/1000, 
                        item->packet.seqNum, item->packet.mavlinkMsgId);
                diagHighLatencyCount++;
            }
            
            diagSentCount++;
            
            // Report every second
            uint32_t nowMs = millis();
            if (nowMs - lastDiagReportMs > 1000) {
                log_msg(LOG_INFO, "[DIAG] USB: sent=%u high_latency=%u queue_depth=%zu",
                        diagSentCount, diagHighLatencyCount, packetQueue.size());
                lastDiagReportMs = nowMs;
                diagSentCount = 0;
                diagHighLatencyCount = 0;
            }
            // === DIAGNOSTIC END ===
            
            // Check available space (handle negative values)
            int space_raw = usbInterface->availableForWrite();
            size_t space = (space_raw > 0) ? (size_t)space_raw : 0;
            
            if (space == 0) {
                // === DIAGNOSTIC START === (Remove after MAVLink stabilization)
                if (item->packet.hints.urgentFlush) {
                    static uint32_t urgentFlushCount = 0;
                    if (++urgentFlushCount % 10 == 0) {
                        log_msg(LOG_DEBUG, "[DIAG] Urgent flush #%u blocked for MAVFtp", urgentFlushCount);
                    }
                    // Shorter backoff for urgent packets
                    lastSendAttempt = micros();
                    backoffDelay = 500U;  // Shorter delay for MAVFtp
                } else {
                    // === DIAGNOSTIC END ===
                    // USB full - apply exponential backoff
                    lastSendAttempt = micros();
                    backoffDelay = (backoffDelay == 0) ? 1000U : (backoffDelay * 2);
                    if (backoffDelay > 5000U) backoffDelay = 5000U;
                // === DIAGNOSTIC START === (Remove after MAVLink stabilization)
                }
                // === DIAGNOSTIC END ===
                break;  // Can't send now
            }
            
            // Calculate how much to send (partial send support)
            size_t remaining = item->packet.size - item->sendOffset;
            size_t toSend = min(remaining, space);
            
            if (toSend == 0) {
                break;  // Can't make progress
            }
            
            // Send what we can
            size_t sent = usbInterface->write(
                item->packet.data + item->sendOffset, 
                toSend
            );
            
            if (sent > 0) {
                item->sendOffset += sent;
                backoffDelay = 0;  // Reset backoff on successful send
                
                // Update global TX counter
                if (globalTxBytesCounter) {
                    *globalTxBytesCounter += sent;
                }
                
                // Check if packet fully sent
                if (item->sendOffset >= item->packet.size) {
                    totalSent++;
                    currentQueueBytes -= item->packet.size;
                    item->packet.free();
                    packetQueue.pop();
                }
            } else {
                // Send failed - apply backoff
                lastSendAttempt = micros();
                backoffDelay = (backoffDelay == 0) ? 1000U : (backoffDelay * 2);
                if (backoffDelay > 5000U) backoffDelay = 5000U;
                break;
            }
        }
    }
    
    bool isReady() const override {
        if (!usbInterface) return false;
        int space = usbInterface->availableForWrite();
        return space > 0;
    }
    
    const char* getName() const override {
        return "USB";
    }
};

#endif // USB_SENDER_H