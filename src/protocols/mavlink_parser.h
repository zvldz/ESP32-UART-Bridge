#pragma once

#include "protocol_parser.h"
#include "packet_memory_pool.h"
#include "protocol_stats.h"
#include "../logging.h"

// FastMAVLink configuration
#include "fastmavlink_config.h"

// FIRST include common.h (which defines FASTMAVLINK_MESSAGE_CRCS)
#include "fastmavlink_lib/c_library/common/common.h"

// THEN include fastmavlink.h (which uses FASTMAVLINK_MESSAGE_CRCS)
#include "fastmavlink_lib/c_library/lib/fastmavlink.h"

// Include specific message definitions for constants
#include "fastmavlink_lib/c_library/common/mavlink_msg_param_value.h"
#include "fastmavlink_lib/c_library/common/mavlink_msg_file_transfer_protocol.h"

// Structure for packet detection result (legacy compatibility)
struct PacketDetectionResult {
    int packetSize;    // Size of detected packet in bytes
    int skipBytes;     // Number of bytes to skip before packet start (garbage/sync bytes)
    
    // Constructor for convenience
    PacketDetectionResult() : packetSize(0), skipBytes(0) {}
    PacketDetectionResult(int size, int skip) : packetSize(size), skipBytes(skip) {}
};

class MavlinkParser : public ProtocolParser {
private:
    PacketMemoryPool* memPool;
    
    // FastMAVLink state
    fmav_status_t fmavStatus;
    uint8_t frameBuffer[296];  // Independent buffer for frame assembly
    
    // Error counters for diagnostics
    uint32_t signatureErrors = 0;
    uint32_t crcErrors = 0;
    uint32_t lengthErrors = 0;
    uint32_t lastErrorReport = 0;
    
    uint8_t getPacketPriority(uint8_t msgId) {
        switch(msgId) {
            case 0:   // HEARTBEAT
            case 1:   // SYS_STATUS
            case 24:  // GPS_RAW_INT
            case 30:  // ATTITUDE
                return PRIORITY_CRITICAL;
            
            case 110: // FILE_TRANSFER_PROTOCOL
                return PRIORITY_BULK;
            
            default:
                return PRIORITY_NORMAL;
        }
    }
    
    uint8_t extractMessageId(const uint8_t* data, size_t len) {
        if (len < 6) return 0xFF;
        
        if (data[0] == 0xFE && len >= 6) {  // MAVLink v1
            return data[5];
        } else if (data[0] == 0xFD && len >= 10) {  // MAVLink v2
            return data[7];
        }
        return 0xFF;  // Unknown
    }
    
    // Legacy method for compatibility with existing detection code
    PacketDetectionResult findPacketBoundary(const uint8_t* data, size_t length) {
        if (length == 0) return PacketDetectionResult(0, 0);
        
        // Process input data byte by byte for packet assembly
        for (size_t i = 0; i < length; i++) {
            fmav_result_t result = {0};
            
            // Parse byte into independent frame buffer to avoid conflicts with buffer management
            uint8_t res = fmav_parse_and_check_to_frame_buf(
                &result,      // Result structure with packet metadata
                frameBuffer,  // Separate buffer for frame assembly (prevents memmove conflicts)
                &fmavStatus,  // Parser state maintained across calls
                data[i]       // Current byte to process
            );
            
            // Check if complete packet was assembled
            if (res) {
                // Complete packet assembled in frameBuffer
                // Calculate bytes to skip (garbage/sync bytes before packet start)
                // If processed 100 bytes total and packet is 80 bytes long,
                // then 20 bytes of non-packet data preceded this packet
                size_t skipBytes = 0;
                if (i + 1 >= result.frame_len) {
                    skipBytes = (i + 1) - result.frame_len;
                }
                
                // Update statistics if available
                if (stats) {
                    stats->onPacketDetected(result.frame_len, micros());
                    // Keep this for verification that fix works
                    static uint32_t detectCounter = 0;
                    if (++detectCounter % 100 == 0) {
                        log_msg(LOG_INFO, "MAVLink: Detected %u packets (stats: %u total)", 
                                detectCounter, stats->packetsDetected);
                    }
                }
                
                // Return packet info
                // Parser automatically resets to IDLE state after complete packet
                return PacketDetectionResult(result.frame_len, skipBytes);
            }
            
            // Check for specific errors (but don't reset parser!)
            if (result.res == FASTMAVLINK_PARSE_RESULT_SIGNATURE_ERROR) {
                signatureErrors++;
                if (signatureErrors <= 5 || signatureErrors % 100 == 1) {
                    log_msg(LOG_WARNING, "MAV: Signature error #%u (should not happen!)", signatureErrors);
                }
                if (stats) {
                    stats->onDetectionError();
                }
            } else if (result.res == FASTMAVLINK_PARSE_RESULT_CRC_ERROR) {
                crcErrors++;
                if (crcErrors <= 5 || crcErrors % 100 == 1) {
                    log_msg(LOG_WARNING, "MAV: CRC error #%u", crcErrors);
                }
                if (stats) {
                    stats->onDetectionError();
                }
            } else if (result.res == FASTMAVLINK_PARSE_RESULT_LENGTH_ERROR) {
                lengthErrors++;
                if (lengthErrors <= 5 || lengthErrors % 100 == 1) {
                    log_msg(LOG_WARNING, "MAV: Length error #%u", lengthErrors);
                }
                if (stats) {
                    stats->onDetectionError();
                }
            }
        }
        
        // No complete packet found in this data
        return PacketDetectionResult(0, 0);
    }

public:
    MavlinkParser() : frameBuffer{0} {  // Initialize frame buffer with zeros
        memPool = PacketMemoryPool::getInstance();
        fmav_init(); // Important! Called in example
        reset();
        log_msg(LOG_INFO, "MAV: FastMAVLink parser initialized with independent frame buffer");
    }
    
    ParseResult parse(CircularBuffer* buffer, uint32_t currentTime) override {
        ParseResult result;
        
        size_t available = buffer->available();
        if (available < getMinimumBytes()) {
            return result;  // Need more data
        }
        
        // Get contiguous view for parsing
        size_t needed = min((size_t)available, (size_t)512);
        auto view = buffer->getContiguousForParser(needed);
        
        if (!view.ptr || view.safeLen < getMinimumBytes()) {
            return result;
        }
        
        // Check for MAVLink start bytes
        if (view.ptr[0] != 0xFE && view.ptr[0] != 0xFD) {
            // Not MAVLink, skip one byte
            result.bytesConsumed = 1;
            if (stats) stats->onDetectionError();
            return result;
        }
        
        // Try to find complete packets
        size_t maxPackets = 10;  // Process up to 10 packets per call
        ParsedPacket* tempPackets = new ParsedPacket[maxPackets];
        size_t packetCount = 0;
        size_t offset = 0;
        
        while (offset < view.safeLen && packetCount < maxPackets) {
            // Use existing findPacketBoundary logic
            PacketDetectionResult pdr = findPacketBoundary(
                view.ptr + offset, 
                view.safeLen - offset
            );
            
            if (pdr.packetSize > 0) {
                // Complete packet found
                size_t allocSize;
                tempPackets[packetCount].data = memPool->allocate(pdr.packetSize, allocSize);
                
                if (!tempPackets[packetCount].data) {
                    log_msg(LOG_ERROR, "MAV: Failed to allocate %d bytes", pdr.packetSize);
                    break;  // Can't allocate more packets
                }
                
                memcpy(tempPackets[packetCount].data, 
                       view.ptr + offset + pdr.skipBytes, 
                       pdr.packetSize);
                
                tempPackets[packetCount].size = pdr.packetSize;
                tempPackets[packetCount].allocSize = allocSize;
                tempPackets[packetCount].pool = memPool;
                tempPackets[packetCount].timestamp = currentTime;
                
                // Extract message ID for priority
                uint8_t msgId = extractMessageId(
                    view.ptr + offset + pdr.skipBytes, 
                    pdr.packetSize
                );
                tempPackets[packetCount].priority = getPacketPriority(msgId);
                
                // Set hints based on message type
                tempPackets[packetCount].hints.keepWhole = true;  // For UDP
                tempPackets[packetCount].hints.canFragment = false;
                if (tempPackets[packetCount].priority == PRIORITY_CRITICAL) {
                    tempPackets[packetCount].hints.urgentFlush = true;
                }
                
                packetCount++;
                offset += pdr.skipBytes + pdr.packetSize;
                
                // Update stats
                if (stats) {
                    stats->packetsDetected++;
                    stats->totalBytes += pdr.packetSize;
                    if (pdr.skipBytes > 0) {
                        stats->totalSkippedBytes += pdr.skipBytes;
                    }
                }
            } else if (pdr.packetSize == 0) {
                // Incomplete packet, need more data
                break;
            } else {
                // Error, skip byte
                offset++;
                if (stats) stats->onDetectionError();
            }
        }
        
        // Copy found packets to result
        if (packetCount > 0) {
            result.count = packetCount;
            result.packets = new ParsedPacket[packetCount];
            for (size_t i = 0; i < packetCount; i++) {
                result.packets[i] = tempPackets[i];
            }
            result.bytesConsumed = offset;
        }
        
        delete[] tempPackets;
        return result;
    }
    
    void prioritizePackets(
        ParsedPacket* packets, 
        size_t count,
        size_t availableSpace
    ) override {
        // Mark BULK packets for dropping if needed
        size_t criticalSize = 0;
        size_t normalSize = 0;
        size_t bulkSize = 0;
        
        for (size_t i = 0; i < count; i++) {
            switch (packets[i].priority) {
                case PRIORITY_CRITICAL:
                    criticalSize += packets[i].size;
                    break;
                case PRIORITY_NORMAL:
                    normalSize += packets[i].size;
                    break;
                case PRIORITY_BULK:
                    bulkSize += packets[i].size;
                    break;
            }
        }
        
        // If critical packets don't fit, we have a problem
        if (criticalSize > availableSpace) {
            log_msg(LOG_ERROR, "Critical packets don't fit in available space!");
            return;
        }
        
        // Drop bulk first, then normal if needed
        if (criticalSize + normalSize > availableSpace) {
            // Drop all bulk
            for (size_t i = 0; i < count; i++) {
                if (packets[i].priority == PRIORITY_BULK) {
                    packets[i].size = 0;  // Mark for dropping
                }
            }
        }
    }
    
    void reset() override {
        // Reset FastMAVLink parser state
        fmav_status_reset(&fmavStatus);
        // Note: frameBuffer does NOT need clearing - parser manages it
        
        // Reset error counters
        signatureErrors = 0;
        crcErrors = 0;
        lengthErrors = 0;
        lastErrorReport = 0;
        
        log_msg(LOG_DEBUG, "MAV: Parser reset");
    }
    
    const char* getName() const override {
        return "MAVLink/FastMAV";
    }
    
    size_t getMinimumBytes() const override {
        return 12;  // MAVLink v2 minimum header size (v1 needs 8, but v2 is more common)
    }
};