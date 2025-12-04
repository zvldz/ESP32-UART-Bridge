#ifndef USB_SENDER_H
#define USB_SENDER_H

#include "packet_sender.h"
#include "../usb/usb_interface.h"
#include "../types.h"  // For g_deviceStats
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
    
    // Batch window timing
    uint32_t batchWindowStart = 0;

    // Thresholds
    static constexpr size_t BATCH_N_PACKETS = 4;
    static constexpr size_t BATCH_X_BYTES = 448;
    static constexpr uint32_t BATCH_T_MS = 5;        // Normal mode
    static constexpr uint32_t BATCH_T_MS_BULK = 20;  // Bulk mode - optimal batching
    
    // Track bulk mode transitions
    bool lastBulkMode = false;

    // USB block detection
    static constexpr uint32_t USB_BLOCKED_TIMEOUT_MS = 1000;  // Increased from 500ms
    size_t lastAvailableForWrite = 0;
    uint32_t availableNotChangedSince = 0;
    bool usbBlocked = false;
    
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

    void clearAllQueues() {
        // Clear main queue with proper memory deallocation
        while (!packetQueue.empty()) {
            packetQueue.front().packet.free();
            packetQueue.pop_front();
        }
        currentQueueBytes = 0;
        
        // Reset batch window
        batchWindowStart = 0;
    }
    
    // Helper: Send single packet - NO PARTIAL WRITE
    bool sendSinglePacket() {
        if (packetQueue.empty() || !isReady()) return false;
        
        QueuedPacket* item = &packetQueue.front();
        int avail = usbInterface->availableForWrite();
        
        // Check if entire packet fits
        if (avail < (int)item->packet.size) {
            // Packet doesn't fit - wait for next iteration
            return false;
        }
        
        // Send entire packet only
        size_t sent = usbInterface->write(item->packet.data, item->packet.size);
        
        if (sent > 0) {
            resetBackoff();
            g_deviceStats.device2.txBytes.fetch_add(sent, std::memory_order_relaxed);
            g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
            commitPackets(1);  // Remove packet from queue
            return true;
        } else {
            applyBackoff();
        }
        return false;
    }
    
public:
    UsbSender(UsbInterface* usb) :
        PacketSender(USB_MAX_PACKETS, USB_MAX_BYTES),
        usbInterface(usb),
        lastSendAttempt(0),
        backoffDelay(0),
        lastStatsLog(0) {
        log_msg(LOG_DEBUG, "UsbSender initialized");
    }

    // Direct send without queue (for fast path)
    size_t sendDirect(const uint8_t* data, size_t size) override {
        if (!usbInterface) return 0;

        size_t sent = usbInterface->write(data, size);
        if (sent > 0) {
            g_deviceStats.device2.txBytes.fetch_add(sent, std::memory_order_relaxed);
            g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
        }
        return sent;
    }

    void processSendQueue(bool bulkMode = false) override {
        uint32_t now = millis();
        
        // USB block detection - only check if we have data to send in not bulk mode
        if (!bulkMode && !packetQueue.empty()) {
            size_t currentAvailable = usbInterface->availableForWrite();
            
            if (currentAvailable == lastAvailableForWrite) {
                // Value hasn't changed
                if (availableNotChangedSince == 0) {
                    availableNotChangedSince = now;
                } else if (!usbBlocked && (now - availableNotChangedSince > USB_BLOCKED_TIMEOUT_MS)) {
                    // USB is blocked - clear everything
                    usbBlocked = true;
                    clearAllQueues();
                    
                    // === USB BLOCK DIAGNOSTIC === (Remove after testing)
                    log_msg(LOG_WARNING, "[USB-DIAG] USB blocked (availableForWrite=%zu unchanged for %ums) - dropping all packets", 
                            currentAvailable, USB_BLOCKED_TIMEOUT_MS);
                    // === END DIAGNOSTIC ===
                }
            } else {
                // Value changed - USB is alive
                if (usbBlocked) {
                    // === USB BLOCK DIAGNOSTIC === (Remove after testing)  
                    log_msg(LOG_INFO, "[USB-DIAG] USB unblocked (availableForWrite: %zu -> %zu) - resuming normal operation",
                            lastAvailableForWrite, currentAvailable);
                    // === END DIAGNOSTIC ===
                    
                    usbBlocked = false;
                }
                
                availableNotChangedSince = 0;
                lastAvailableForWrite = currentAvailable;  // Always update when changed
            }
        } else {
            // No data to send - reset detection timer but keep current block state
            availableNotChangedSince = 0;
        }
        
        // If USB is blocked, don't process anything
        if (usbBlocked) {
            return;
        }
        
        // Skip if in backoff
        if (inBackoff()) return;
        
        // Track bulk mode transitions
        if (bulkMode != lastBulkMode) {
            log_msg(LOG_DEBUG, "[USB] Bulk mode %s", bulkMode ? "ON" : "OFF");
            
            // Force flush on bulk exit
            if (!bulkMode && lastBulkMode && batchWindowStart != 0) {
                batchWindowStart = now - 1000;  // Force timeout
            }
            lastBulkMode = bulkMode;
        }
        
        // Non-bulk mode - single packet
        if (!bulkMode) {
            sendSinglePacket();
            return;
        }
        
        // Bulk mode batching
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
        uint32_t batchTimeout = bulkMode ? BATCH_T_MS_BULK : BATCH_T_MS;
        
        if (batchPackets >= BATCH_N_PACKETS) {
            shouldFlush = true;  // Enough packets
        } else if (batchSize >= BATCH_X_BYTES) {
            shouldFlush = true;  // Enough bytes
        } else if (windowAge >= batchTimeout) {
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
            
            // Check if entire batch fits
            if (avail < (int)offset) {
                // Batch doesn't fit entirely
                if (windowAge >= batchTimeout) {
                    // Timeout expired - send what fits
                    size_t partialOffset = 0;
                    size_t partialPackets = 0;
                    
                    // Calculate how many packets fit
                    for (size_t i = 0; i < batchPackets; i++) {
                        size_t packetSize = packetQueue[i].packet.size;
                        if (partialOffset + packetSize <= (size_t)avail) {
                            partialOffset += packetSize;
                            partialPackets++;
                        } else {
                            break;
                        }
                    }
                    
                    // Send partial batch
                    if (partialPackets > 0) {
                        size_t sent = usbInterface->write(batchBuffer, partialOffset);
                        if (sent > 0) {
                            resetBackoff();
                            g_deviceStats.device2.txBytes.fetch_add(sent, std::memory_order_relaxed);
                            g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
                            commitPackets(partialPackets);
                        }
                    }
                    batchWindowStart = 0;  // Reset window
                }
                return;
            }
            
            // Send entire batch only
            size_t sent = usbInterface->write(batchBuffer, offset);
            
            if (sent > 0) {
                resetBackoff();
                g_deviceStats.device2.txBytes.fetch_add(sent, std::memory_order_relaxed);
                g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
                commitPackets(batchPackets);  // Commit all packets
                
                
                batchWindowStart = 0;  // Reset window after sending
            } else {
                applyBackoff();
            }
        } else {
            // NON-BLOCKING: Just return, don't wait
            return;
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
    
    bool enqueue(const ParsedPacket& packet) override {
        // When USB is blocked, keep only 1 packet for testing
        if (usbBlocked) {
            if (packetQueue.empty()) {
                // Accept this packet as test packet
                // Continue with normal enqueue logic below
            } else {
                // Already have a test packet - drop new ones
                // Don't count as dropped - this is expected when USB is dead
                return false;
            }
        }

        // Call parent enqueue implementation
        return PacketSender::enqueue(packet);
    }
};

#endif // USB_SENDER_H