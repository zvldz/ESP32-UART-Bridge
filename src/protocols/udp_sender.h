#ifndef UDP_SENDER_H
#define UDP_SENDER_H

#include "packet_sender.h"
#include "network_functions.h"
#include <ArduinoJson.h>

class UdpSender : public PacketSender {
private:
    // Batch buffer constants
    static constexpr size_t MTU_SIZE = 1400;
    static constexpr size_t MAX_BATCH_PACKETS = 10;

    // MAVLink batch state
    uint8_t mavlinkBatchBuffer[MTU_SIZE];
    size_t mavlinkBatchSize = 0;
    size_t mavlinkBatchPackets = 0;
    uint32_t mavlinkBatchStartMs = 0;

    // RAW batch state (renamed from existing)
    uint8_t rawBatchBuffer[MTU_SIZE];  // Renamed from batchBuffer
    size_t rawBatchSize = 0;           // Renamed from batchSize
    uint32_t lastBatchTime;
    uint32_t lastStatsLog;   // For periodic statistics logging

    // Bulk mode tracking
    bool lastBulkMode = false;

    // Future compatibility flag (hardcoded for now)
    bool enableMavlinkBatching = true;  // TODO: Get from config later
    
    // === DIAGNOSTIC START === (Remove after batching validation)
    struct {
        uint32_t totalBatches = 0;
        uint32_t mavlinkPacketsInBatches = 0;
        uint32_t maxPacketsInBatch = 0;
        uint32_t bulkModeBatches = 0;
        uint32_t normalModeBatches = 0;
        uint32_t lastLogMs = 0;
    } batchDiag;
    // === DIAGNOSTIC END ===

    // Batching thresholds
    static constexpr size_t MAVLINK_BATCH_PACKETS_NORMAL = 2;
    static constexpr size_t MAVLINK_BATCH_PACKETS_BULK = 5;
    static constexpr size_t MAVLINK_BATCH_BYTES_NORMAL = 600;
    static constexpr size_t MAVLINK_BATCH_BYTES_BULK = 1200;
    static constexpr uint32_t MAVLINK_BATCH_TIMEOUT_MS_NORMAL = 5;
    static constexpr uint32_t MAVLINK_BATCH_TIMEOUT_MS_BULK = 20;
    static constexpr uint32_t RAW_BATCH_TIMEOUT_MS = 5;
    
    void flushMavlinkBatch() {
        if (mavlinkBatchSize > 0) {
            // === DIAGNOSTIC START === (Remove after batching validation)
            batchDiag.totalBatches++;
            batchDiag.mavlinkPacketsInBatches += mavlinkBatchPackets;
            if (mavlinkBatchPackets > batchDiag.maxPacketsInBatch) {
                batchDiag.maxPacketsInBatch = mavlinkBatchPackets;
            }
            if (lastBulkMode) {
                batchDiag.bulkModeBatches++;
            } else {
                batchDiag.normalModeBatches++;
            }
            
            // Log every 20 batches
            if (batchDiag.totalBatches % 20 == 0) {
                uint32_t now = millis();
                if (now - batchDiag.lastLogMs > 5000) {  // Max once per 5 sec
                    float avg = batchDiag.mavlinkPacketsInBatches / (float)batchDiag.totalBatches;
                    float efficiency = (batchDiag.mavlinkPacketsInBatches * 100.0f) / 
                                      (totalSent > 0 ? totalSent : 1);
                    
                    log_msg(LOG_DEBUG, "[UDP-BATCH] #%u: avg=%.1f max=%u eff=%.0f%% bulk=%u%%",
                            batchDiag.totalBatches, avg, batchDiag.maxPacketsInBatch,
                            efficiency,
                            (batchDiag.bulkModeBatches * 100) / batchDiag.totalBatches);
                    
                    batchDiag.lastLogMs = now;
                }
            }
            // === DIAGNOSTIC END ===
            
            sendUdpDatagram(mavlinkBatchBuffer, mavlinkBatchSize);
            totalSent += mavlinkBatchPackets;  // Count packets sent
            
            // Reset batch
            mavlinkBatchSize = 0;
            mavlinkBatchPackets = 0;
            mavlinkBatchStartMs = 0;
        }
    }
    
    void flushRawBatch() {
        if (rawBatchSize > 0) {
            // === DIAGNOSTIC START === (Remove after UDP stabilization)
            log_msg(LOG_DEBUG, "[UDP-DIAG] Flush RAW batch: %zu bytes", rawBatchSize);
            // === DIAGNOSTIC END ===
            
            sendUdpDatagram(rawBatchBuffer, rawBatchSize);
            totalSent++;  // RAW: one chunk = one "packet"
            rawBatchSize = 0;
        }
    }
    
    void flushAllBatches() {
        flushMavlinkBatch();
        flushRawBatch();
    }
    
    void processMavlinkPacket(QueuedPacket* item, bool bulkMode, uint32_t now) {
        // Check if batching is enabled (hardcoded true for now)
        if (!enableMavlinkBatching) {
            // Legacy mode - one packet per datagram
            flushMavlinkBatch();
            sendUdpDatagram(item->packet.data, item->packet.size);
            totalSent++;
            return;
        }
        
        // Check if packet fits in current batch
        if (mavlinkBatchSize + item->packet.size > MTU_SIZE) {
            flushMavlinkBatch();
        }
        
        // Add to batch
        memcpy(mavlinkBatchBuffer + mavlinkBatchSize, 
               item->packet.data, item->packet.size);
        mavlinkBatchSize += item->packet.size;
        mavlinkBatchPackets++;
        
        // Initialize batch window
        if (mavlinkBatchStartMs == 0) {
            mavlinkBatchStartMs = now;
        }
        
        // Decide if should flush
        bool shouldFlush = false;
        
        if (bulkMode) {
            // Bulk mode - optimize for throughput
            if (mavlinkBatchPackets >= MAVLINK_BATCH_PACKETS_BULK ||
                mavlinkBatchSize >= MAVLINK_BATCH_BYTES_BULK ||
                (now - mavlinkBatchStartMs) >= MAVLINK_BATCH_TIMEOUT_MS_BULK) {
                shouldFlush = true;
            }
        } else {
            // Normal mode - optimize for latency
            if (mavlinkBatchPackets >= MAVLINK_BATCH_PACKETS_NORMAL ||
                mavlinkBatchSize >= MAVLINK_BATCH_BYTES_NORMAL ||
                (now - mavlinkBatchStartMs) >= MAVLINK_BATCH_TIMEOUT_MS_NORMAL) {
                shouldFlush = true;
            }
        }
        
        if (shouldFlush) {
            flushMavlinkBatch();
        }
    }
    
    void processRawPacket(QueuedPacket* item, bool bulkMode, uint32_t now) {
        // RAW batching logic (existing, but renamed variables)
        if (rawBatchSize + item->packet.size > MTU_SIZE) {
            flushRawBatch();
        }
        
        if (item->packet.size <= MTU_SIZE) {
            memcpy(rawBatchBuffer + rawBatchSize, 
                   item->packet.data, item->packet.size);
            rawBatchSize += item->packet.size;
            lastBatchTime = now;
        } else {
            // Single packet too big - send alone
            flushRawBatch();
            sendUdpDatagram(item->packet.data, item->packet.size);
            totalSent++;
        }
    }
    
    void checkBatchTimeouts(bool bulkMode, uint32_t now) {
        // Check MAVLink batch timeout
        if (mavlinkBatchSize > 0) {
            uint32_t timeout = bulkMode ? MAVLINK_BATCH_TIMEOUT_MS_BULK : MAVLINK_BATCH_TIMEOUT_MS_NORMAL;
            if ((now - mavlinkBatchStartMs) >= timeout) {
                // === DIAGNOSTIC START === (Remove after UDP stabilization)
                log_msg(LOG_DEBUG, "[UDP-DIAG] MAVLink batch timeout: %ums (%s mode)", 
                        now - mavlinkBatchStartMs, bulkMode ? "BULK" : "NORMAL");
                // === DIAGNOSTIC END ===
                flushMavlinkBatch();
            }
        }
        
        // Check RAW batch timeout
        if (rawBatchSize > 0) {
            if ((now - lastBatchTime) >= RAW_BATCH_TIMEOUT_MS) {
                // === DIAGNOSTIC START === (Remove after UDP stabilization)
                log_msg(LOG_DEBUG, "[UDP-DIAG] RAW batch timeout: %ums", 
                        now - lastBatchTime);
                // === DIAGNOSTIC END ===
                flushRawBatch();
            }
        }
    }
    
public:
    UdpSender(unsigned long* txCounter = nullptr) : 
        PacketSender(20, 8192, txCounter),  // Return to original sizes
        rawBatchSize(0),
        lastBatchTime(0),
        lastStatsLog(0) {
        log_msg(LOG_DEBUG, "UdpSender initialized");
    }
    
    ~UdpSender() {
        // Flush any pending batches before destruction
        flushAllBatches();
    }
    
    void processSendQueue(bool bulkMode = false) override {
        uint32_t now = millis();
        
        // === DIAGNOSTIC START === (Remove after batching validation)
        static uint32_t bulkStartMs = 0;
        if (bulkMode != lastBulkMode) {
            if (bulkMode) {
                bulkStartMs = now;
                log_msg(LOG_DEBUG, "[UDP] Bulk mode ON (queue=%zu)", packetQueue.size());
            } else {
                log_msg(LOG_DEBUG, "[UDP] Bulk mode OFF after %ums", now - bulkStartMs);
            }
            
            // Force flush on mode change
            if (!bulkMode && lastBulkMode) {
                flushAllBatches();
            }
            lastBulkMode = bulkMode;
        }
        // === DIAGNOSTIC END ===
        
        // Process queue
        while (!packetQueue.empty()) {
            QueuedPacket* item = &packetQueue.front();
            
            
            // Route packet based on protocol type (using keepWhole flag)
            if (item->packet.hints.keepWhole) {
                // MAVLink packet
                processMavlinkPacket(item, bulkMode, now);
            } else {
                // RAW data packet
                processRawPacket(item, bulkMode, now);
            }
            
            // Remove from queue
            currentQueueBytes -= item->packet.size;
            item->packet.free();
            packetQueue.pop_front();
        }
        
        // Check timeouts for batches
        checkBatchTimeouts(bulkMode, now);
    }
    
    bool isReady() const override {
        return true;  // UDP is always ready (connectionless)
    }
    
    const char* getName() const override {
        return "UDP";
    }
    
    // Get batching statistics for display
    void getBatchingStats(JsonObject& stats) {
        // === DIAGNOSTIC START === (Remove after batching validation)
        stats["batching"] = enableMavlinkBatching;  // Always show batching status
        
        if (batchDiag.totalBatches > 0) {
            stats["totalBatches"] = batchDiag.totalBatches;
            float avg = batchDiag.mavlinkPacketsInBatches / (float)batchDiag.totalBatches;
            stats["avgPacketsPerBatch"] = serialized(String(avg, 1));
            stats["maxPacketsInBatch"] = batchDiag.maxPacketsInBatch;
            
            float efficiency = (batchDiag.mavlinkPacketsInBatches * 100.0f) / 
                              (totalSent > 0 ? totalSent : 1);
            stats["batchEfficiency"] = String(efficiency, 0) + "%";
        } else {
            // No batches yet - show waiting status
            stats["totalBatches"] = 0;
            stats["avgPacketsPerBatch"] = "0.0";
            stats["maxPacketsInBatch"] = 0;
            stats["batchEfficiency"] = "0%";
        }
        // === DIAGNOSTIC END ===
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