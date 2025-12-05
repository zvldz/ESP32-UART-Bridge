#ifndef UDP_SENDER_H
#define UDP_SENDER_H

#include "packet_sender.h"
#include "protocol_types.h"  // For DataFormat::FORMAT_SBUS
#include <ArduinoJson.h>
#include <AsyncUDP.h>
#include "../types.h"  // For g_deviceStats
#include "../wifi/wifi_manager.h"  // For wifiIsReady()

class UdpSender : public PacketSender {
private:

    // Direct UDP transport
    AsyncUDP* udpTransport;  // Passed from outside

    // Multiple target IPs support (manual or auto-broadcast from scheduler)
    static constexpr size_t MAX_UDP_TARGETS = 4;
    IPAddress targetIPs[MAX_UDP_TARGETS];
    uint8_t targetCount = 0;

    // Batch buffer constants
    static constexpr size_t MTU_SIZE = 1400;
    static constexpr size_t MAX_BATCH_PACKETS = 10;

    // Atomic packet batch state (for packets with keepWhole=true)
    // Used for: MAVLink telemetry, line-based logs, other atomic packets
    uint8_t atomicBatchBuffer[MTU_SIZE];
    size_t atomicBatchSize = 0;
    size_t atomicBatchPackets = 0;
    uint32_t atomicBatchStartMs = 0;

    // RAW batch state
    uint8_t rawBatchBuffer[MTU_SIZE];
    size_t rawBatchSize = 0;
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

    // Batching thresholds for MAVLink/Logs (slow path)
    static constexpr size_t ATOMIC_BATCH_PACKETS_NORMAL = 2;
    static constexpr size_t ATOMIC_BATCH_PACKETS_BULK = 5;
    static constexpr size_t ATOMIC_BATCH_BYTES_NORMAL = 600;
    static constexpr size_t ATOMIC_BATCH_BYTES_BULK = 1200;
    static constexpr uint32_t ATOMIC_BATCH_TIMEOUT_MS_NORMAL = 5;
    static constexpr uint32_t ATOMIC_BATCH_TIMEOUT_MS_BULK = 20;
    static constexpr uint32_t RAW_BATCH_TIMEOUT_MS = 5;

    // Batching thresholds for SBUS (fast path via sendDirect)
    static constexpr size_t SBUS_BATCH_FRAMES = 3;
    static constexpr uint32_t SBUS_BATCH_STALE_MS = 50;  // Discard stale batch
    
    void flushBatch() {
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
        flushBatch();
        flushRawBatch();
    }
    
    void processAtomicPacket(QueuedPacket* item, bool bulkMode, uint32_t now) {
        // Check if batching is enabled (configurable via setBatchingEnabled)
        if (!enableAtomicBatching) {
            // Legacy mode - one packet per datagram
            flushBatch();
            sendUdpDatagram(item->packet.data, item->packet.size);
            totalSent++;
            return;
        }

        // Check if packet fits in current batch
        if (atomicBatchSize + item->packet.size > MTU_SIZE) {
            flushBatch();
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
            flushBatch();
        }
    }
    
    void processRawPacket(QueuedPacket* item, bool bulkMode, uint32_t now) {
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
                flushBatch();
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
    // Direct send without queue (for SBUS fast path)
    size_t sendDirect(const uint8_t* data, size_t size) override {
        if (!udpTransport || !wifiIsReady()) return 0;

        uint32_t now = millis();

        // Check if batch is stale (no new frames for 50ms) - discard it
        if (atomicBatchSize > 0 && (now - atomicBatchStartMs) >= SBUS_BATCH_STALE_MS) {
            // Stale batch - discard without sending (SBUS is state-based)
            log_msg(LOG_WARNING, "[SBUS-UDP] Discarded stale batch: %zu frames, %zu bytes, age %lums",
                atomicBatchPackets, atomicBatchSize, (now - atomicBatchStartMs));
            atomicBatchSize = 0;
            atomicBatchPackets = 0;
            atomicBatchStartMs = 0;
        }

        // Check if batch buffer overflows
        if (atomicBatchSize + size > MTU_SIZE) {
            // Flush current batch (incomplete - MTU overflow)
            flushBatch();
        }

        // Add to batch
        memcpy(atomicBatchBuffer + atomicBatchSize, data, size);
        atomicBatchSize += size;
        atomicBatchPackets++;

        if (atomicBatchStartMs == 0) {
            atomicBatchStartMs = now;
        }

        // Flush if reached SBUS batch threshold
        if (atomicBatchPackets >= SBUS_BATCH_FRAMES) {
            flushBatch();
        }

        return size;
    }

    // Configuration method
    void setBatchingEnabled(bool enabled) {
        enableAtomicBatching = enabled;
        log_msg(LOG_INFO, "UDP batching %s", enabled ? "enabled" : "disabled");
    }

    // Parse comma-separated IP list into targetIPs array
    void parseTargetIPs(const char* ipList) {
        targetCount = 0;
        if (!ipList || ipList[0] == '\0') return;

        char buffer[96];
        strncpy(buffer, ipList, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';

        char* token = strtok(buffer, ",");
        while (token && targetCount < MAX_UDP_TARGETS) {
            // Skip leading spaces
            while (*token == ' ') token++;
            // Remove trailing spaces
            char* end = token + strlen(token) - 1;
            while (end > token && *end == ' ') *end-- = '\0';

            if (strlen(token) > 0) {
                if (targetIPs[targetCount].fromString(token)) {
                    log_msg(LOG_DEBUG, "UDP target[%d]: %s", targetCount, token);
                    targetCount++;
                }
            }
            token = strtok(nullptr, ",");
        }
    }

    explicit UdpSender(AsyncUDP* udp) :
        PacketSender(DEFAULT_MAX_PACKETS, DEFAULT_MAX_BYTES),
        udpTransport(udp),
        rawBatchSize(0),
        lastBatchTime(0),
        lastStatsLog(0) {

        extern Config config;
        if (config.device4_config.auto_broadcast) {
            // Scheduler will set broadcast IP, no need to parse manual targets
            log_msg(LOG_INFO, "UdpSender: auto-broadcast mode");
        } else {
            parseTargetIPs(config.device4_config.target_ip);
            log_msg(LOG_INFO, "UdpSender: %d target(s)", targetCount);
        }
    }

    // Set broadcast IP directly (called by scheduler for auto-broadcast mode)
    void setBroadcastIP(const IPAddress& ip) {
        targetIPs[0] = ip;
        targetCount = 1;
    }

    // Reload targets from config (called by scheduler when IP changes)
    void reloadTargets() {
        extern Config config;
        parseTargetIPs(config.device4_config.target_ip);
        log_msg(LOG_DEBUG, "UDP targets reloaded: %d", targetCount);
    }
    
    ~UdpSender() {
        // Flush any pending batches before destruction
        flushAllBatches();
    }
    
    void processSendQueue(bool bulkMode = false) override {
        // Check WiFi readiness for data transmission (works for both Client and AP modes)
        if (!wifiIsReady()) {
            // Clear queue silently (not counted as drops)
            while (!packetQueue.empty()) {
                currentQueueBytes -= packetQueue.front().packet.size;
                packetQueue.front().packet.free();
                packetQueue.pop_front();
            }

            // Reset batch buffers for slow path only (MAVLink/Log)
            // NOTE: Do NOT reset atomicBatchPackets here - it's used by sendDirect() fast path
            atomicBatchSize = 0;
            rawBatchSize = 0;
            atomicBatchStartMs = 0;

            return;
        }
        
        // Existing processing code continues here...
        uint32_t now = millis();

        // Flush batches on bulk mode transition (functional, not diagnostic)
        if (bulkMode != lastBulkMode) {
            // === DIAGNOSTIC START === (Remove after batching validation)
            static uint32_t bulkStartMs = 0;
            if (bulkMode) {
                bulkStartMs = now;
                log_msg(LOG_DEBUG, "[UDP] Bulk mode ON (queue=%zu)", packetQueue.size());
            } else {
                log_msg(LOG_DEBUG, "[UDP] Bulk mode OFF after %ums", now - bulkStartMs);
            }
            // === DIAGNOSTIC END ===

            // Force flush on bulk mode exit
            if (!bulkMode) {
                flushAllBatches();
            }
            lastBulkMode = bulkMode;
        }

        // Process queue
        while (!packetQueue.empty()) {
            QueuedPacket* item = &packetQueue.front();
            
            // === TEMPORARY DIAGNOSTIC START ===
            // Log SBUS frames being sent via UDP
            if (item->packet.format == DataFormat::FORMAT_SBUS) {
                static uint32_t sbusUdpCount = 0;
                if (++sbusUdpCount % 100 == 0) {
                    log_msg(LOG_DEBUG, "UDP: Sent %u SBUS frames", sbusUdpCount);
                }
            }
            // === TEMPORARY DIAGNOSTIC END ===
            
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
    
    // Get batching statistics for web interface display
    void getBatchingStats(JsonObject& stats) {
        stats["batching"] = enableAtomicBatching;

        if (batchDiag.totalBatches > 0) {
            stats["totalBatches"] = batchDiag.totalBatches;
            float avg = batchDiag.atomicPacketsInBatches / (float)batchDiag.totalBatches;
            stats["avgPacketsPerBatch"] = serialized(String(avg, 1));
            stats["maxPacketsInBatch"] = batchDiag.maxPacketsInBatch;

            float efficiency = (batchDiag.atomicPacketsInBatches * 100.0f) /
                              (totalSent > 0 ? totalSent : 1);
            char effBuf[16];
            snprintf(effBuf, sizeof(effBuf), "%.0f%%", efficiency);
            stats["batchEfficiency"] = effBuf;
        } else {
            stats["totalBatches"] = 0;
            stats["avgPacketsPerBatch"] = "0.0";
            stats["maxPacketsInBatch"] = 0;
            stats["batchEfficiency"] = "0%";
        }
    }
    
    void sendUdpDatagram(uint8_t* data, size_t size) {
        if (!udpTransport || size == 0 || targetCount == 0) return;

        extern Config config;
        uint16_t port = config.device4_config.port;
        size_t totalSentBytes = 0;
        uint8_t sentCount = 0;

        // Send to all configured targets (manual IPs or broadcast from scheduler)
        for (uint8_t i = 0; i < targetCount; i++) {
            size_t sent = udpTransport->writeTo(data, size, targetIPs[i], port);
            if (sent > 0) {
                totalSentBytes += sent;
                sentCount++;
            }
        }

        if (sentCount == 0) {
            static uint32_t lastFailLog = 0;
            if (millis() - lastFailLog > 5000) {
                log_msg(LOG_WARNING, "[UDP] Send failed (no targets or all failed)");
                lastFailLog = millis();
            }
        } else {
            g_deviceStats.device4.txBytes.fetch_add(totalSentBytes, std::memory_order_relaxed);
            g_deviceStats.device4.txPackets.fetch_add(sentCount, std::memory_order_relaxed);
            g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
        }
    }
};

#endif // UDP_SENDER_H