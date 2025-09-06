#pragma once

#include <memory>
#include "protocol_parser.h"
#include "packet_memory_pool.h"
#include "protocol_stats.h"
#include "mavlink_include.h"      // Unified MAVLink header inclusion point
#include "../logging.h"

class MavlinkParser : public ProtocolParser {
private:
    PacketMemoryPool* memPool;
    bool routingEnabled = false;  // Cache routing flag
    
    // Target extraction methods
    uint8_t extractTargetSystem(mavlink_message_t* msg);
    uint8_t extractTargetComponent(mavlink_message_t* msg);
    
    // pymavlink structures
    mavlink_message_t rxMessage;        // Current message being parsed
    mavlink_status_t rxStatus;          // Parser status
    uint8_t rxChannel;                  // Channel ID (usually 0)
    
    // Bulk mode detector with decay counter
    class BulkModeDetector {
    private:
        uint32_t recentPackets = 0;      // Counter with decay
        uint32_t lastDecayMs = 0;        // Last decay timestamp
        bool bulkActive = false;         // Current bulk mode state
        uint32_t bulkStartMs = 0;        // When bulk started
        uint32_t bulkEndMs = 0;          // When bulk ended
        
        // Thresholds with hysteresis
        static constexpr uint32_t BULK_ON_THRESHOLD = 20;   // Turn on at 20
        static constexpr uint32_t BULK_OFF_THRESHOLD = 5;   // Turn off at 5
        static constexpr uint32_t DECAY_INTERVAL_MS = 100; // Decay every 100ms
        static constexpr uint32_t PACKET_INCREMENT = 2;     // Add 2 per packet
        
    public:
        BulkModeDetector() : lastDecayMs(millis()) {}
        
        // Call for each relevant packet
        void onPacket(uint32_t msgId) {
            // Count only FTP and PARAM packets
            if (msgId == MAVLINK_MSG_ID_FILE_TRANSFER_PROTOCOL ||
                msgId == MAVLINK_MSG_ID_PARAM_VALUE ||
                msgId == MAVLINK_MSG_ID_PARAM_REQUEST_READ ||
                msgId == MAVLINK_MSG_ID_PARAM_REQUEST_LIST) {
                
                // Increase counter (saturate at 50 to prevent overflow)
                recentPackets = min((uint32_t)50, recentPackets + PACKET_INCREMENT);
                
                // Check activation threshold
                if (!bulkActive && recentPackets >= BULK_ON_THRESHOLD) {
                    bulkActive = true;
                    bulkStartMs = millis();
                    log_msg(LOG_INFO, "Bulk mode ON (counter=%u)", recentPackets);
                }
            }
            
            // Always update decay (for any packet)
            update();
        }
        
        // Update decay counter
        void update() {
            uint32_t now = millis();
            
            // Decay counter every 100ms
            while (now - lastDecayMs >= DECAY_INTERVAL_MS) {
                if (recentPackets > 0) {
                    recentPackets--;
                }
                lastDecayMs += DECAY_INTERVAL_MS;
                
                // Check deactivation threshold
                if (bulkActive && recentPackets < BULK_OFF_THRESHOLD) {
                    bulkActive = false;
                    bulkEndMs = now;
                    uint32_t duration = (bulkEndMs - bulkStartMs) / 1000;
                    log_msg(LOG_INFO, "Bulk mode OFF (counter=%u, duration=%us)", 
                            recentPackets, duration);
                }
            }
        }
        
        bool isActive() const { return bulkActive; }
        uint32_t getCounter() const { return recentPackets; }
        
        void reset() {
            recentPackets = 0;
            bulkActive = false;
            lastDecayMs = millis();
            bulkStartMs = 0;
            bulkEndMs = 0;
        }
    };
    
    BulkModeDetector bulkDetector;  // Instance of bulk detector
    
    // === DIAGNOSTIC START === (Remove after MAVLink stabilization)
    struct DiagnosticCounters {
        uint32_t totalParsed = 0;
        uint32_t highLatencyWarnings = 0;
        uint32_t lastReportTimeMs = 0;
    } diagCounters;
    // === DIAGNOSTIC END ===

public:
    MavlinkParser() : rxChannel(0) {
        memPool = PacketMemoryPool::getInstance();
        
        // Initialize pymavlink status
        memset(&rxStatus, 0, sizeof(rxStatus));
        memset(&rxMessage, 0, sizeof(rxMessage));
        
        log_msg(LOG_DEBUG, "pymav: Parser initialized");
    }
    
    virtual ~MavlinkParser() = default;
    
    // Main parse method - simplified byte-wise parsing
    ParseResult parse(CircularBuffer* buffer, uint32_t currentTime) override {
        ParseResult result;
        
        size_t available = buffer->available();
        if (available == 0) {
            return result;
        }
        
        // Get view - up to 296 bytes (MAVLink max + margin)
        const size_t MAVLINK_MAX_FRAME = 296;
        size_t needed = min(available, MAVLINK_MAX_FRAME);
        auto view = buffer->getContiguousForParser(needed);
        
        if (!view.ptr || view.safeLen == 0) {
            return result;
        }
        
        // Temporary storage for packets
        const size_t maxPackets = 10;
        std::unique_ptr<ParsedPacket[]> tempPackets(new ParsedPacket[maxPackets]);
        size_t packetCount = 0;
        
        // === EXPERIMENTAL CONSUME FIX START ===
        // CRITICAL CHANGE: Track last complete packet position
        size_t lastCompletePos = 0;  // Position after last complete packet
        // === EXPERIMENTAL CONSUME FIX END ===
        
        // Feed each byte to pymavlink parser (STANDARD APPROACH)
        for (size_t i = 0; i < view.safeLen; i++) {
            uint8_t byte = view.ptr[i];
            
            // Standard pymavlink byte-wise parsing
            uint8_t parseResult = mavlink_parse_char(
                rxChannel,
                byte,
                &rxMessage,
                &rxStatus
            );
            
            // === EXPERIMENTAL CONSUME FIX START ===
            // Handle different parsing results
            if (parseResult == MAVLINK_FRAMING_OK) {
                // Complete valid packet received
                lastCompletePos = i + 1;  // Mark position AFTER this packet
                if (stats) stats->onPacketDetected(0, currentTime);  // Will be updated with real size later
            }
            else if (parseResult == MAVLINK_FRAMING_BAD_CRC || 
                     parseResult == MAVLINK_FRAMING_BAD_SIGNATURE) {
                // Bad CRC - packet is complete but invalid, still need to consume it
                lastCompletePos = i + 1;  // Skip bad packet
                if (stats) stats->onDetectionError();
                
                // Log for diagnostics during testing
                static uint32_t badCrcCount = 0;
                if (++badCrcCount <= 10) {  // Limit log spam
                    log_msg(LOG_WARNING, "[MAVLink] Bad CRC #%u at pos %zu", 
                            badCrcCount, i);
                }
            }
            // MAVLINK_FRAMING_INCOMPLETE - continue parsing, don't update lastCompletePos
            // === EXPERIMENTAL CONSUME FIX END ===
            
            if (parseResult == MAVLINK_FRAMING_OK) {
                // === TEMPORARY DIAGNOSTIC BLOCK START ===
                // Log sequence gaps (rate limited)
                static uint8_t lastSeq[256] = {0};  // Per sysid
                static uint32_t gapCount = 0;
                static uint32_t lastGapLog = 0;

                uint8_t sysid = rxMessage.sysid;
                uint8_t seq = rxMessage.seq;
                uint8_t expected = lastSeq[sysid] + 1;

                if (lastSeq[sysid] != 0 && seq != expected) {
                    gapCount++;
                    uint32_t now = millis();
                    
                    // Log first 10 gaps or every 5 seconds
                    if (gapCount <= 10 || (now - lastGapLog > 5000)) {
                        log_msg(LOG_WARNING, "[SEQ] Gap #%u: sysid=%d jumped %d->%d (lost=%d)", 
                                gapCount, sysid, lastSeq[sysid], seq, 
                                (seq > expected) ? (seq - expected) : (256 + seq - expected));
                        lastGapLog = now;
                    }
                }
                lastSeq[sysid] = seq;
                // === TEMPORARY DIAGNOSTIC BLOCK END ===
                
                // Complete message received
                bulkDetector.onPacket(rxMessage.msgid);
                
                if (packetCount < maxPackets) {
                    handleParsedMessage(&rxMessage, currentTime, 
                                      tempPackets.get(), packetCount, maxPackets);
                }
            }
            // Note: Ignore MAVLINK_FRAMING_INCOMPLETE
            // pymavlink handles all states internally
        }
        
        // === EXPERIMENTAL CONSUME FIX START ===
        // CRITICAL: Consume only up to last complete packet
        result.bytesConsumed = lastCompletePos;
        
        // Diagnostic during testing - log incomplete packets
        if (lastCompletePos < view.safeLen) {
            static uint32_t incompleteCount = 0;
            static uint32_t lastLogMs = 0;
            uint32_t now = millis();
            
            incompleteCount++;
            // Log every second to avoid spam
            if (now - lastLogMs > 1000) {
                size_t leftover = view.safeLen - lastCompletePos;
                log_msg(LOG_DEBUG, "[MAVLink] Incomplete packet: %zu bytes left in buffer (total: %u times)",
                        leftover, incompleteCount);
                lastLogMs = now;
            }
        }
        // === EXPERIMENTAL CONSUME FIX END ===
        
        // Copy packets to result
        if (packetCount > 0) {
            result.count = packetCount;
            result.packets = new ParsedPacket[packetCount];
            memcpy(result.packets, tempPackets.get(), sizeof(ParsedPacket) * packetCount);
        }
        
        // Update bulk detector
        bulkDetector.update();
        
        
        return result;
    }
    
    void reset() override {
        // Reset pymavlink parser state
        memset(&rxStatus, 0, sizeof(rxStatus));
        memset(&rxMessage, 0, sizeof(rxMessage));
        
        // Reset bulk detector
        bulkDetector.reset();
        
        log_msg(LOG_DEBUG, "pymav: Parser reset");
    }
    
    const char* getName() const override {
        return "MAVLink/pymav";
    }
    
    size_t getMinimumBytes() const override {
        return 3;  // STX + length + ...
    }
    
    // Extended timeout for bulk transfers
    bool requiresExtendedTimeout() const override {
        return bulkDetector.isActive();
    }
    
    // Flush strategy hooks
    bool shouldFlushNow(size_t pendingPackets, uint32_t timeSinceLastMs) const override {
        // During bulk mode - flush immediately for low latency
        if (bulkDetector.isActive()) {
            return pendingPackets > 0;  // Any packet triggers flush
        }
        
        // Normal operation - standard batching for efficiency
        return timeSinceLastMs > 3 || pendingPackets >= 5;
    }
    
    // EXPERIMENTAL: Dynamic batch timeout based on traffic type
    uint32_t getBatchTimeoutMs() const override {
        // Longer batching for bulk mode improves efficiency
        // Normal telemetry needs lower latency
        return bulkDetector.isActive() ? 20 : 5;  // 20ms for bulk, 5ms for normal
    }
    
    // Get current burst mode state from detector
    bool isBurstActive() const override {
        return bulkDetector.isActive();
    }
    
    // Set routing mode
    void setRoutingEnabled(bool enabled) { routingEnabled = enabled; }

private:
    // Handle successfully parsed message
    void handleParsedMessage(mavlink_message_t* msg, uint32_t currentTime,
                            ParsedPacket* tempPackets, size_t& packetCount, 
                            size_t maxPackets) {
        if (packetCount >= maxPackets) {
            return;  // Buffer full
        }
        
        // Calculate total packet size
        uint16_t packetLen = mavlink_msg_get_send_buffer_length(msg);
        
        // Allocate memory from pool
        size_t allocSize;
        uint8_t* packetData = (uint8_t*)memPool->allocate(packetLen, allocSize);
        
        if (!packetData) {
            log_msg(LOG_WARNING, "pymav: Failed to allocate %u bytes", packetLen);
            return;
        }
        
        // Serialize message to buffer
        uint16_t len = mavlink_msg_to_send_buffer(packetData, msg);
        
        // Fill packet structure
        tempPackets[packetCount].data = packetData;
        tempPackets[packetCount].size = len;
        tempPackets[packetCount].allocSize = allocSize;
        tempPackets[packetCount].pool = memPool;
        tempPackets[packetCount].timestamp = currentTime;
        
        // Set protocol type
        tempPackets[packetCount].protocol = PacketProtocol::MAVLINK;
        
        // Set permanent protocol fields
        tempPackets[packetCount].protocolMsgId = msg->msgid;
        tempPackets[packetCount].seqNum = ++globalSeqNum;
        
        // Set routing data
        tempPackets[packetCount].routing.mavlink.sysId = msg->sysid;
        tempPackets[packetCount].routing.mavlink.compId = msg->compid;
        
        // Extract targets only if routing is enabled
        if (routingEnabled) {
            tempPackets[packetCount].routing.mavlink.targetSys = extractTargetSystem(msg);
            tempPackets[packetCount].routing.mavlink.targetComp = extractTargetComponent(msg);
            
            // === DIAGNOSTIC START === (Remove after routing stabilization)
            static uint32_t lastRoutableLogMs = 0;
            uint32_t now = millis();
            
            // Check if this is a routable message type
            bool isRoutable = (msg->msgid == MAVLINK_MSG_ID_PARAM_REQUEST_READ || 
                              msg->msgid == MAVLINK_MSG_ID_PARAM_REQUEST_LIST || 
                              msg->msgid == MAVLINK_MSG_ID_PARAM_SET ||
                              msg->msgid == MAVLINK_MSG_ID_MISSION_REQUEST_LIST || 
                              msg->msgid == MAVLINK_MSG_ID_MISSION_COUNT || 
                              msg->msgid == MAVLINK_MSG_ID_MISSION_ITEM_INT ||
                              msg->msgid == MAVLINK_MSG_ID_COMMAND_INT || 
                              msg->msgid == MAVLINK_MSG_ID_COMMAND_LONG || 
                              msg->msgid == MAVLINK_MSG_ID_FILE_TRANSFER_PROTOCOL);
            
            if (isRoutable && (now - lastRoutableLogMs > 2000)) {  // Every 2 seconds max
                log_msg(LOG_INFO, "[PARSER-TARGET] msgid=%u target=%u comp=%u from sysid=%u",
                        msg->msgid, 
                        tempPackets[packetCount].routing.mavlink.targetSys,
                        tempPackets[packetCount].routing.mavlink.targetComp,
                        msg->sysid);
                lastRoutableLogMs = now;
            }
            
            // Special logging for FILE_TRANSFER_PROTOCOL
            if (msg->msgid == MAVLINK_MSG_ID_FILE_TRANSFER_PROTOCOL) {
                static uint32_t ftpLogCount = 0;
                if (ftpLogCount++ < 5) {  // Log first 5 FTP packets only
                    log_msg(LOG_INFO, "[PARSER-FTP] FTP packet #%u: target=%u from sysid=%u",
                            ftpLogCount, tempPackets[packetCount].routing.mavlink.targetSys, msg->sysid);
                }
            }
            // === DIAGNOSTIC END ===
        } else {
            tempPackets[packetCount].routing.mavlink.targetSys = 0;
            tempPackets[packetCount].routing.mavlink.targetComp = 0;
        }
        
        // Physical interface will be set by pipeline
        tempPackets[packetCount].physicalInterface = 0xFF;  // Invalid until set
        
        tempPackets[packetCount].parseTimeMicros = micros();
        tempPackets[packetCount].mavlinkMsgId = msg->msgid;  // DEPRECATED - use protocolMsgId
        
        diagCounters.totalParsed++;
        
        // Hints for optimization
        tempPackets[packetCount].hints.keepWhole = true;
        tempPackets[packetCount].hints.canFragment = false;
        
        packetCount++;
        
        // Update statistics
        if (stats) {
            stats->onPacketDetected(len, currentTime);
        }
    }
    
private:
};