#ifndef USB_SENDER_H
#define USB_SENDER_H

#include "packet_sender.h"
#include "../usb/usb_interface.h"
#include <cstring>   // for memcpy
#include <algorithm> // for min

class UsbSender : public PacketSender {
private:
    UsbInterface* usbInterface;
    uint32_t lastSendAttempt;
    uint32_t backoffDelay;
    uint32_t lastStatsLog;
    
    // Batch buffer (keep existing)
    static constexpr size_t BATCH_BUFFER_SIZE = 2048;
    static constexpr size_t MAX_BATCH_PACKETS = 8;
    uint8_t batchBuffer[BATCH_BUFFER_SIZE];
    
    // NEW: Pending batch for partial writes
    struct {
        uint8_t data[BATCH_BUFFER_SIZE];
        size_t totalSize = 0;
        size_t sentOffset = 0;
        size_t packetCount = 0;
        
        bool active() const { return totalSize > 0; }
        void clear() { totalSize = sentOffset = packetCount = 0; }
        
        void set(const uint8_t* src, size_t size, size_t offset, size_t count) {
            memcpy(data, src, size);
            totalSize = size;
            sentOffset = offset;
            packetCount = count;
        }
    } pending;
    
    // NEW: Batch window timing
    uint32_t batchWindowStart = 0;
    
    // NEW: Thresholds
    static constexpr size_t BATCH_N_PACKETS = 4;
    static constexpr size_t BATCH_X_BYTES = 448;
    static constexpr uint32_t BATCH_T_MS = 5;
    
    // NEW: Track bulk mode transitions
    bool lastBulkMode = false;
    
    // Helper: Apply backoff
    void applyBackoff(uint32_t delayUs = 1000) {
        lastSendAttempt = micros();
        backoffDelay = (backoffDelay == 0) ? delayUs : std::min((uint32_t)(backoffDelay * 2), (uint32_t)5000);
    }
    
    // Helper: Reset backoff
    void resetBackoff() {
        backoffDelay = 0;
    }
    
    // Helper: Check if in backoff
    bool inBackoff() const {
        return backoffDelay > 0 && (micros() - lastSendAttempt) < backoffDelay;
    }
    
    // Helper: Update TX counter
    void updateTxCounter(size_t bytes) {
        if (globalTxBytesCounter) {
            *globalTxBytesCounter += bytes;
        }
    }
    
    // Helper: Commit packets from queue (after successful send)
    void commitPackets(size_t count) {
        for (size_t i = 0; i < count; i++) {
            if (packetQueue.empty()) break;
            totalSent++;
            currentQueueBytes -= packetQueue.front().packet.size;
            packetQueue.front().packet.free();
            packetQueue.pop_front();  // Note: pop_front() for deque
        }
    }
    
    // Helper: Flush pending batch
    bool flushPending() {
        if (!pending.active()) return true;
        
        size_t remaining = pending.totalSize - pending.sentOffset;
        int avail = usbInterface->availableForWrite();
        
        if (avail <= 0) {
            applyBackoff();
            return false;
        }
        
        size_t toSend = std::min(remaining, (size_t)avail);
        size_t sent = usbInterface->write(
            pending.data + pending.sentOffset, toSend);
        
        if (sent > 0) {
            pending.sentOffset += sent;
            resetBackoff();
            updateTxCounter(sent);
            
            // Check if fully sent
            if (pending.sentOffset >= pending.totalSize) {
                // NOW safe to remove packets from queue
                commitPackets(pending.packetCount);
                pending.clear();
                return true;
            }
        } else {
            applyBackoff();
        }
        return false;  // Still pending
    }
    
    // Helper: Send single packet (handles partial)
    bool sendSinglePacket() {
        if (packetQueue.empty() || !isReady()) return false;
        
        QueuedPacket* item = &packetQueue.front();
        int avail = usbInterface->availableForWrite();
        
        if (avail <= 0) {
            applyBackoff();
            return false;
        }
        
        size_t remaining = item->packet.size - item->sendOffset;
        size_t toSend = std::min(remaining, (size_t)avail);
        size_t sent = usbInterface->write(
            item->packet.data + item->sendOffset, toSend);
        
        if (sent > 0) {
            item->sendOffset += sent;
            resetBackoff();
            updateTxCounter(sent);
            
            if (item->sendOffset >= item->packet.size) {
                // Fully sent
                commitPackets(1);
                return true;
            }
        } else {
            applyBackoff();
        }
        return false;
    }
    
public:
    UsbSender(UsbInterface* usb, unsigned long* txCounter = nullptr) : 
        PacketSender(128, 24576, txCounter),  // Return to original sizes
        usbInterface(usb),
        lastSendAttempt(0),
        backoffDelay(0),
        lastStatsLog(0) {
        log_msg(LOG_DEBUG, "UsbSender initialized");
    }
    
    void processSendQueue(bool bulkMode = false) override {
        // Skip if in backoff
        if (inBackoff()) return;
        
        uint32_t now = millis();
        
        // Track bulk mode transitions
        if (bulkMode != lastBulkMode) {
            log_msg(LOG_DEBUG, "[USB] Bulk mode %s", bulkMode ? "ON" : "OFF");
            
            // Force flush on bulk exit
            if (!bulkMode && lastBulkMode && batchWindowStart != 0) {
                batchWindowStart = now - 1000;  // Force timeout
            }
            lastBulkMode = bulkMode;
        }
        
        // STEP 1: Always flush pending first
        if (pending.active()) {
            if (!flushPending()) return;
            // Pending done, continue to maybe batch more
        }
        
        // STEP 2: Handle partial head packet
        if (!packetQueue.empty() && packetQueue.front().sendOffset > 0) {
            sendSinglePacket();
            return;  // Don't batch until head is clear
        }
        
        // STEP 3: Non-bulk mode - single packet
        if (!bulkMode) {
            sendSinglePacket();
            return;
        }
        
        // STEP 4: Bulk mode batching
        if (packetQueue.empty() || !isReady()) {
            batchWindowStart = 0;
            return;
        }
        
        // Initialize window on first packet
        if (batchWindowStart == 0) {
            batchWindowStart = now;
        }
        
        // Plan batch - scan queue directly with deque
        size_t batchPackets = 0;
        size_t batchSize = 0;
        
        // Direct iteration - much cleaner with deque!
        size_t maxScan = std::min(packetQueue.size(), (size_t)MAX_BATCH_PACKETS);
        for (size_t i = 0; i < maxScan; i++) {
            const QueuedPacket& item = packetQueue[i];
            
            // Skip partially sent packets
            if (item.sendOffset > 0) break;
            
            // Check if fits
            if (batchSize + item.packet.size > BATCH_BUFFER_SIZE) break;
            
            batchSize += item.packet.size;
            batchPackets++;
            
            // Stop if we have enough
            if (batchPackets >= BATCH_N_PACKETS || 
                batchSize >= BATCH_X_BYTES) {
                break;
            }
        }
        
        // Decide if should flush
        bool shouldFlush = false;
        uint32_t windowAge = now - batchWindowStart;
        
        if (batchPackets >= BATCH_N_PACKETS) {
            shouldFlush = true;  // Enough packets
        } else if (batchSize >= BATCH_X_BYTES) {
            shouldFlush = true;  // Enough bytes
        } else if (windowAge >= BATCH_T_MS) {
            shouldFlush = true;  // Timeout
        } else if (batchPackets == packetQueue.size() && batchPackets > 0) {
            shouldFlush = true;  // No more coming
        }
        
        // Send batch if ready
        if (shouldFlush && batchPackets > 0) {
            int avail = usbInterface->availableForWrite();
            if (avail <= 0) {
                applyBackoff();
                return;
            }
            
            // Build batch by copying data
            size_t offset = 0;
            for (size_t i = 0; i < batchPackets; i++) {
                const QueuedPacket& item = packetQueue[i];
                memcpy(batchBuffer + offset, item.packet.data, item.packet.size);
                offset += item.packet.size;
            }
            
            // Attempt send
            size_t toSend = std::min(offset, (size_t)avail);
            size_t sent = usbInterface->write(batchBuffer, toSend);
            
            if (sent > 0) {
                resetBackoff();
                updateTxCounter(sent);
                
                if (sent < offset) {
                    // Partial - save to pending
                    pending.set(batchBuffer, offset, sent, batchPackets);
                    log_msg(LOG_DEBUG, "[USB] Partial batch: %zu/%zu bytes",
                            sent, offset);
                } else {
                    // Full send - commit packets
                    commitPackets(batchPackets);
                    
                    // Log batch success (sample)
                    static uint32_t batchCount = 0;
                    if (++batchCount % 20 == 0) {
                        log_msg(LOG_DEBUG, "[USB] Batch #%u: %zu packets, %zu bytes",
                                batchCount, batchPackets, offset);
                    }
                }
                batchWindowStart = 0;  // Reset window
            } else {
                applyBackoff();
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