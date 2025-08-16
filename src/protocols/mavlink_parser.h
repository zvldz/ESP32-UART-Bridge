#pragma once

#include "protocol_parser.h"
#include "packet_memory_pool.h"
#include "protocol_stats.h"
#include "../logging.h"

// --- MAVLink convenience setup must come BEFORE including mavlink.h ---
#ifndef MAVLINK_USE_CONVENIENCE_FUNCTIONS
#define MAVLINK_USE_CONVENIENCE_FUNCTIONS 1
#endif
#ifndef MAVLINK_COMM_NUM_BUFFERS
#define MAVLINK_COMM_NUM_BUFFERS 1
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#endif

// Pull basic types first so we can predeclare symbols used by helpers
#include "mavlink/mavlink_types.h"

// Predeclare the global and the byte-send hook BEFORE helpers see them
extern mavlink_system_t mavlink_system;
void comm_send_ch(mavlink_channel_t chan, uint8_t ch);

// Now include full MAVLink (helpers will find the symbols above)
#include "mavlink/ardupilotmega/mavlink.h"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

class MavlinkParser : public ProtocolParser {
private:
    PacketMemoryPool* memPool;
    
    // pymavlink structures
    mavlink_message_t rxMessage;        // Current message being parsed
    mavlink_status_t rxStatus;          // Parser status
    uint8_t rxChannel;                  // Channel ID (usually 0)
    
    // MAVFtp detection (simple counter approach)
    bool mavftpActive = false;
    uint32_t nonMavftpPacketCount = 0;
    
    // Last packet priority for flush strategy
    uint8_t lastPacketPriority = PRIORITY_NORMAL;
    
    // TEMPORARY DEBUG: Remove after testing
    uint32_t debugTotalParsed = 0;
    uint32_t debugCrcErrors = 0;
    
    // Detection of non-MAVLink stream
    uint32_t bytesWithoutPacket = 0;
    uint32_t lastNoPacketWarning = 0;

public:
    MavlinkParser() : rxChannel(0) {
        memPool = PacketMemoryPool::getInstance();
        
        // Initialize pymavlink status
        memset(&rxStatus, 0, sizeof(rxStatus));
        memset(&rxMessage, 0, sizeof(rxMessage));
        
        log_msg(LOG_INFO, "pymav: Parser initialized");
    }
    
    virtual ~MavlinkParser() = default;
    
    // Main parse method
    ParseResult parse(CircularBuffer* buffer, uint32_t currentTime) override {
        ParseResult result;
        
        size_t available = buffer->available();
        if (available == 0) {
            return result;
        }
        
        // Get view for parsing
        size_t needed = min(available, (size_t)512);
        auto view = buffer->getContiguousForParser(needed);
        
        if (!view.ptr || view.safeLen == 0) {
            return result;
        }
        
        // Temporary array for packets
        const size_t maxPackets = 10;
        ParsedPacket* tempPackets = new ParsedPacket[maxPackets];
        size_t packetCount = 0;
        size_t bytesProcessed = 0;
        
        // Parse bytes one by one (pymavlink style)
        for (size_t i = 0; i < view.safeLen; i++) {
            uint8_t byte = view.ptr[i];
            bytesProcessed++;
            
            // Parse byte through pymavlink
            uint8_t parseResult = mavlink_parse_char(
                rxChannel,
                byte,
                &rxMessage,
                &rxStatus
            );
            
            if (parseResult == MAVLINK_FRAMING_OK) {
                // Complete message received
                handleParsedMessage(&rxMessage, currentTime, 
                                  tempPackets, packetCount, maxPackets);
                
                // TEMPORARY DEBUG
                if (++debugTotalParsed % 100 == 0) {
                    log_msg(LOG_DEBUG, "pymav: Parsed %u messages, MAVFtp=%s",
                            debugTotalParsed, mavftpActive ? "active" : "inactive");
                }
            }
            else if (parseResult == MAVLINK_FRAMING_BAD_CRC) {
                // CRC error
                if (stats) {
                    stats->onDetectionError();
                }
                
                // TEMPORARY DEBUG
                if (++debugCrcErrors % 10 == 0) {
                    log_msg(LOG_DEBUG, "pymav: CRC errors = %u", debugCrcErrors);
                }
            }
        }
        
        // Handle case when no packets found
        if (packetCount == 0 && bytesProcessed > 0) {
            bytesWithoutPacket += bytesProcessed;
            
            // Warn every 10KB of non-MAVLink data
            if (bytesWithoutPacket > 10000 && 
                bytesWithoutPacket - lastNoPacketWarning > 10000) {
                log_msg(LOG_WARNING, "pymav: No valid MAVLink packets in %u bytes - check data source or switch to RAW mode",
                        bytesWithoutPacket);
                lastNoPacketWarning = bytesWithoutPacket;
            }
            
            // Consume small amount to avoid buffer deadlock
            // but not too much to allow MAVLink sync if it appears
            result.bytesConsumed = min(bytesProcessed, (size_t)10);
        } else if (packetCount > 0) {
            // Found packets - reset warning counters
            bytesWithoutPacket = 0;
            lastNoPacketWarning = 0;
            result.bytesConsumed = bytesProcessed;
        } else {
            // No bytes processed - avoid deadlock
            result.bytesConsumed = min((size_t)1, view.safeLen);
        }
        
        // Return found packets
        if (packetCount > 0) {
            result.count = packetCount;
            result.packets = new ParsedPacket[packetCount];
            memcpy(result.packets, tempPackets, sizeof(ParsedPacket) * packetCount);
        }
        
        delete[] tempPackets;
        return result;
    }
    
    void reset() override {
        // Reset pymavlink parser state
        memset(&rxStatus, 0, sizeof(rxStatus));
        memset(&rxMessage, 0, sizeof(rxMessage));
        
        // Reset detection state
        mavftpActive = false;
        nonMavftpPacketCount = 0;
        lastPacketPriority = PRIORITY_NORMAL;
        
        // Reset debug counters
        debugTotalParsed = 0;
        debugCrcErrors = 0;
        
        // Reset non-MAVLink stream detection
        bytesWithoutPacket = 0;
        lastNoPacketWarning = 0;
        
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
        return mavftpActive;
    }
    
    // EXPERIMENTAL: Flush strategy hooks
    bool shouldFlushNow(size_t pendingPackets, uint32_t timeSinceLastMs) const override {
        // Critical packets - flush immediately
        if (lastPacketPriority == PRIORITY_CRITICAL) {
            return true;
        }
        
        // During MAVFtp - batch longer for efficiency
        if (mavftpActive) {
            return timeSinceLastMs > 10 || pendingPackets >= 20;
        }
        
        // Normal operation - standard batching
        return timeSinceLastMs > 5 || pendingPackets >= 5;
    }
    
    uint32_t getBatchTimeoutMs() const override {
        return mavftpActive ? 10 : 5;
    }

private:
    // Handle successfully parsed message
    void handleParsedMessage(mavlink_message_t* msg, uint32_t currentTime,
                            ParsedPacket* tempPackets, size_t& packetCount, 
                            size_t maxPackets) {
        if (packetCount >= maxPackets) {
            return;  // Buffer full
        }
        
        // Detect MAVFtp mode
        detectMavftpMode(msg->msgid);
        
        // Get packet priority
        uint8_t priority = getPacketPriority(msg->msgid);
        lastPacketPriority = priority;
        
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
        tempPackets[packetCount].priority = priority;
        
        // Hints for optimization
        tempPackets[packetCount].hints.keepWhole = true;
        tempPackets[packetCount].hints.canFragment = false;
        tempPackets[packetCount].hints.urgentFlush = (priority == PRIORITY_CRITICAL);
        
        packetCount++;
        
        // Update statistics
        if (stats) {
            stats->onPacketDetected(len, currentTime);
        }
    }
    
    // Detect MAVFtp session
    void detectMavftpMode(uint32_t msgId) {
        // MAVFtp message ID = 110
        if (msgId == MAVLINK_MSG_ID_FILE_TRANSFER_PROTOCOL) {
            if (!mavftpActive) {
                mavftpActive = true;
                log_msg(LOG_INFO, "pymav: MAVFtp session started");
            }
            nonMavftpPacketCount = 0;
        } else if (mavftpActive) {
            nonMavftpPacketCount++;
            if (nonMavftpPacketCount > 50) {
                mavftpActive = false;
                log_msg(LOG_INFO, "pymav: MAVFtp session ended");
            }
        }
    }
    
    // Get packet priority for optimization
    uint8_t getPacketPriority(uint32_t msgId) {
        switch(msgId) {
            case MAVLINK_MSG_ID_HEARTBEAT:
            case MAVLINK_MSG_ID_COMMAND_LONG:
            case MAVLINK_MSG_ID_COMMAND_ACK:
            case MAVLINK_MSG_ID_SYS_STATUS:
            case MAVLINK_MSG_ID_ATTITUDE:
                return PRIORITY_CRITICAL;
                
            case MAVLINK_MSG_ID_FILE_TRANSFER_PROTOCOL:
            case MAVLINK_MSG_ID_PARAM_VALUE:
            case MAVLINK_MSG_ID_PARAM_REQUEST_READ:
            case MAVLINK_MSG_ID_PARAM_REQUEST_LIST:
                return PRIORITY_BULK;
                
            default:
                return PRIORITY_NORMAL;
        }
    }
};