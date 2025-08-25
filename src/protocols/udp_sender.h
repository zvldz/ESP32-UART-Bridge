#ifndef UDP_SENDER_H
#define UDP_SENDER_H

#include "packet_sender.h"
#include "network_functions.h"
#include <ArduinoJson.h>
#include <AsyncUDP.h>
#include "../wifi/wifi_manager.h"

class UdpSender : public PacketSender {
private:
    
    // Direct UDP transport
    AsyncUDP* udpTransport;  // Passed from outside
    IPAddress targetIP;      // Parsed target IP
    bool isBroadcast;        // Broadcast flag
    // Batch buffer constants
    static constexpr size_t MTU_SIZE = 1400;
    static constexpr size_t MAX_BATCH_PACKETS = 10;

    // Atomic packet batch state (for packets with keepWhole=true)
    // Used for: MAVLink telemetry, line-based logs, other atomic packets
    uint8_t atomicBatchBuffer[MTU_SIZE];
    size_t atomicBatchSize = 0;
    size_t atomicBatchPackets = 0;
    uint32_t atomicBatchStartMs = 0;

    // RAW batch state (renamed from existing)
    uint8_t rawBatchBuffer[MTU_SIZE];  // Renamed from batchBuffer
    size_t rawBatchSize = 0;           // Renamed from batchSize
    uint32_t lastBatchTime;
    uint32_t lastStatsLog;   // For periodic statistics logging

    // Bulk mode tracking
    bool lastBulkMode = false;

    // Batching control from config (for legacy GCS compatibility)
    bool enableAtomicBatching = true;  // Set via setBatchingEnabled()
    
    // === DIAGNOSTIC START === (Remove after batching validation)
    struct {
        uint32_t totalBatches = 0;
        uint32_t atomicPacketsInBatches = 0;
        uint32_t maxPacketsInBatch = 0;
        uint32_t bulkModeBatches = 0;
        uint32_t normalModeBatches = 0;
        uint32_t lastLogMs = 0;
    } batchDiag;
    // === DIAGNOSTIC END ===

    // Batching thresholds
    static constexpr size_t ATOMIC_BATCH_PACKETS_NORMAL = 2;
    static constexpr size_t ATOMIC_BATCH_PACKETS_BULK = 5;
    static constexpr size_t ATOMIC_BATCH_BYTES_NORMAL = 600;
    static constexpr size_t ATOMIC_BATCH_BYTES_BULK = 1200;
    static constexpr uint32_t ATOMIC_BATCH_TIMEOUT_MS_NORMAL = 5;
    static constexpr uint32_t ATOMIC_BATCH_TIMEOUT_MS_BULK = 20;
    static constexpr uint32_t RAW_BATCH_TIMEOUT_MS = 5;
    
    void flushAtomicBatch() {
        if (atomicBatchSize > 0) {
            // === DIAGNOSTIC START === (Remove after batching validation)
            batchDiag.totalBatches++;
            batchDiag.atomicPacketsInBatches += atomicBatchPackets;
            if (atomicBatchPackets > batchDiag.maxPacketsInBatch) {
                batchDiag.maxPacketsInBatch = atomicBatchPackets;
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
                    float avg = batchDiag.atomicPacketsInBatches / (float)batchDiag.totalBatches;
                    float efficiency = (batchDiag.atomicPacketsInBatches * 100.0f) / 
                                      (totalSent > 0 ? totalSent : 1);
                    
                    log_msg(LOG_DEBUG, "[UDP-BATCH] #%u: avg=%.1f max=%u eff=%.0f%% bulk=%u%%",
                            batchDiag.totalBatches, avg, batchDiag.maxPacketsInBatch,
                            efficiency,
                            (batchDiag.bulkModeBatches * 100) / batchDiag.totalBatches);
                    
                    batchDiag.lastLogMs = now;
                }
            }
            // === DIAGNOSTIC END ===
            
            sendUdpDatagram(atomicBatchBuffer, atomicBatchSize);
            totalSent += atomicBatchPackets;  // Count packets sent
            
            // Reset batch
            atomicBatchSize = 0;
            atomicBatchPackets = 0;
            atomicBatchStartMs = 0;
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
        flushAtomicBatch();
        flushRawBatch();
    }
    
    void processAtomicPacket(QueuedPacket* item, bool bulkMode, uint32_t now) {
        // Check if batching is enabled (hardcoded true for now)
        if (!enableAtomicBatching) {
            // Legacy mode - one packet per datagram
            flushAtomicBatch();
            sendUdpDatagram(item->packet.data, item->packet.size);
            totalSent++;
            return;
        }
        
        // Check if packet fits in current batch
        if (atomicBatchSize + item->packet.size > MTU_SIZE) {
            flushAtomicBatch();
        }
        
        // Add to batch
        memcpy(atomicBatchBuffer + atomicBatchSize, 
               item->packet.data, item->packet.size);
        atomicBatchSize += item->packet.size;
        atomicBatchPackets++;
        
        // Initialize batch window
        if (atomicBatchStartMs == 0) {
            atomicBatchStartMs = now;
        }
        
        // Decide if should flush
        bool shouldFlush = false;
        
        if (bulkMode) {
            // Bulk mode - optimize for throughput
            if (atomicBatchPackets >= ATOMIC_BATCH_PACKETS_BULK ||
                atomicBatchSize >= ATOMIC_BATCH_BYTES_BULK ||
                (now - atomicBatchStartMs) >= ATOMIC_BATCH_TIMEOUT_MS_BULK) {
                shouldFlush = true;
            }
        } else {
            // Normal mode - optimize for latency
            if (atomicBatchPackets >= ATOMIC_BATCH_PACKETS_NORMAL ||
                atomicBatchSize >= ATOMIC_BATCH_BYTES_NORMAL ||
                (now - atomicBatchStartMs) >= ATOMIC_BATCH_TIMEOUT_MS_NORMAL) {
                shouldFlush = true;
            }
        }
        
        if (shouldFlush) {
            flushAtomicBatch();
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
        // Check atomic packet batch timeout
        if (atomicBatchSize > 0) {
            uint32_t timeout = bulkMode ? ATOMIC_BATCH_TIMEOUT_MS_BULK : ATOMIC_BATCH_TIMEOUT_MS_NORMAL;
            if ((now - atomicBatchStartMs) >= timeout) {
                flushAtomicBatch();
            }
        }
        
        // Check RAW batch timeout
        if (rawBatchSize > 0) {
            if ((now - lastBatchTime) >= RAW_BATCH_TIMEOUT_MS) {
                flushRawBatch();
            }
        }
    }
    
public:
    // Static UDP statistics
    inline static unsigned long udpTxBytes = 0;
    inline static unsigned long udpTxPackets = 0;
    inline static unsigned long udpRxBytes = 0;
    inline static unsigned long udpRxPackets = 0;
    inline static unsigned long device1TxBytesFromDevice4 = 0;

    // Configuration method
    void setBatchingEnabled(bool enabled) { 
        enableAtomicBatching = enabled; 
        log_msg(LOG_INFO, "UDP batching %s", enabled ? "enabled" : "disabled");
    }
    
    UdpSender(AsyncUDP* udp, unsigned long* txCounter = nullptr) : 
        PacketSender(20, 8192, txCounter),
        udpTransport(udp),
        rawBatchSize(0),
        lastBatchTime(0),
        lastStatsLog(0) {
        
        // Parse target IP
        extern Config config;
        isBroadcast = (strcmp(config.device4_config.target_ip, "192.168.4.255") == 0) ||
                      (strstr(config.device4_config.target_ip, ".255") != nullptr);
        
        if (!isBroadcast) {
            targetIP.fromString(config.device4_config.target_ip);
        }
        
        log_msg(LOG_DEBUG, "UdpSender initialized");
    }
    
    ~UdpSender() {
        // Flush any pending batches before destruction
        flushAllBatches();
    }
    
    void processSendQueue(bool bulkMode = false) override {
        // Check WiFi readiness for data transmission (works for both Client and AP modes)
        extern Config config;
        if (!wifi_manager_is_ready_for_data()) {
            // Clear queue silently (not counted as drops)
            while (!packetQueue.empty()) {
                currentQueueBytes -= packetQueue.front().packet.size;
                packetQueue.front().packet.free();
                packetQueue.pop_front();
            }
            
            // Reset batch buffers to avoid stale data
            atomicBatchSize = 0;
            atomicBatchPackets = 0;
            atomicBatchStartMs = 0;
            rawBatchSize = 0;
            
            return;
        }
        
        // Existing processing code continues here...
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
                // Atomic packet (MAVLink, logs, etc)
                processAtomicPacket(item, bulkMode, now);
            } else {
                // RAW data packet (can be fragmented)
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
        stats["batching"] = enableAtomicBatching;  // Always show batching status
        
        if (batchDiag.totalBatches > 0) {
            stats["totalBatches"] = batchDiag.totalBatches;
            float avg = batchDiag.atomicPacketsInBatches / (float)batchDiag.totalBatches;
            stats["avgPacketsPerBatch"] = serialized(String(avg, 1));
            stats["maxPacketsInBatch"] = batchDiag.maxPacketsInBatch;
            
            float efficiency = (batchDiag.atomicPacketsInBatches * 100.0f) / 
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
    
    // Update RX stats from callback
    static void updateRxStats(size_t bytes) {
        udpRxBytes += bytes;
        udpRxPackets++;
        device1TxBytesFromDevice4 += bytes;
    }
    
    void sendUdpDatagram(uint8_t* data, size_t size) {
        // Early exit checks
        if (!udpTransport || size == 0) return;
        
        extern Config config;
        size_t sent = 0;
        
        if (isBroadcast) {
            sent = udpTransport->broadcastTo(data, size, config.device4_config.port);
        } else {
            sent = udpTransport->writeTo(data, size, targetIP, config.device4_config.port);
        }
        
        if (sent == 0) {
            // Rate-limited warning for UDP failures
            static uint32_t lastFailLog = 0;
            if (millis() - lastFailLog > 5000) {
                log_msg(LOG_WARNING, "[UDP] Send failed");
                lastFailLog = millis();
            }
        } else {
            // Update static counters
            udpTxBytes += sent;
            udpTxPackets++;
        }
        
        if (globalTxBytesCounter) {
            // Update global TX counter (no protection needed - same core)
            *globalTxBytesCounter += sent;
        }
    }
};

#endif // UDP_SENDER_H