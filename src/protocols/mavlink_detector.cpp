#include "mavlink_detector.h"
#include "../logging.h"
#include <Arduino.h>  // For millis()

MavlinkDetector::MavlinkDetector() : currentVersion(0), headerValidated(false) {
    log_msg("MAVLink detector initialized", LOG_DEBUG);
}

bool MavlinkDetector::canDetect(const uint8_t* data, size_t len) {
    if (len == 0) return false;
    
    // Check for MAVLink start bytes
    return (data[0] == MAVLINK_STX_V1 || data[0] == MAVLINK_STX_V2);
}

int MavlinkDetector::findPacketBoundary(const uint8_t* data, size_t len) {
    if (len == 0) return 0;  // Need more data
    
    // Check start byte and determine version
    uint8_t version = 0;
    if (data[0] == MAVLINK_STX_V1) {
        version = 1;
    } else if (data[0] == MAVLINK_STX_V2) {
        version = 2;
    } else {
        // Invalid start byte - search for next valid start
        int nextStart = findNextStartByte(data, len, 1);
        if (nextStart > 0) {
            log_msg("MAVLink: Invalid start byte 0x" + String(data[0], HEX) + ", found next at pos " + String(nextStart), LOG_DEBUG);
            return -nextStart;  // Skip to next start position
        }
        return -1;  // Skip 1 byte and try again
    }
    
    // Check if we have enough data for header
    size_t headerLen = (version == 1) ? MAVLINK_V1_HEADER_LEN : MAVLINK_V2_HEADER_LEN;
    if (len < headerLen) {
        return 0;  // Need more data
    }
    
    // Validate header if not done yet or version changed
    if (!headerValidated || currentVersion != version) {
        if (!validateHeader(data, len, version)) {
            // Header validation failed - search for next start byte
            int nextStart = findNextStartByte(data, len, 1);
            if (nextStart > 0) {
                log_msg("MAVLink: Header validation failed, found next at pos " + String(nextStart), LOG_DEBUG);
                return -nextStart;  // Skip to next start position
            }
            return -1;  // Skip 1 byte and try again
        }
        headerValidated = true;
        currentVersion = version;
    }
    
    // Extract payload length
    uint8_t payloadLen = data[1];
    if (payloadLen > MAVLINK_MAX_PAYLOAD_LEN) {
        log_msg("MAVLink: Invalid payload length " + String(payloadLen), LOG_DEBUG);
        headerValidated = false;  // Reset validation state
        int nextStart = findNextStartByte(data, len, 1);
        if (nextStart > 0) {
            return -nextStart;
        }
        return -1;
    }
    
    // Calculate total packet size
    size_t totalSize = headerLen + payloadLen + MAVLINK_CHECKSUM_LEN;
    
    // For MAVLink v2, check for signature
    if (version == 2) {
        // Check incompat_flags (byte 2 in v2)
        uint8_t incompatFlags = data[2];
        if (incompatFlags & 0x01) {  // MAVLINK_IFLAG_SIGNED
            totalSize += MAVLINK_SIGNATURE_LEN;
        }
        
        // Check for unsupported incompat_flags (bits 1-7 reserved)
        if (incompatFlags & 0xFE) {
            log_msg("MAVLink v2: Unsupported incompat_flags 0x" + String(incompatFlags, HEX), LOG_DEBUG);
            headerValidated = false;
            int nextStart = findNextStartByte(data, len, 1);
            if (nextStart > 0) {
                return -nextStart;
            }
            return -1;
        }
    }
    
    // Check if we have complete packet
    if (len < totalSize) {
        return 0;  // Need more data
    }
    
    log_msg("MAVLink v" + String(version) + " packet detected: " + String(totalSize) + " bytes (payload: " + String(payloadLen) + ")", LOG_DEBUG);
    
    // Reset state for next packet
    headerValidated = false;
    currentVersion = 0;
    
    return static_cast<int>(totalSize);
}

bool MavlinkDetector::validateHeader(const uint8_t* data, size_t len, uint8_t version) {
    if (version == 1) {
        if (len < MAVLINK_V1_HEADER_LEN) return false;
        
        // For v1: STX(1) + LEN(1) + SEQ(1) + SYS(1) + COMP(1) + MSG(1)
        // Only validate payload length (already done in findPacketBoundary)
        return true;
        
    } else if (version == 2) {
        if (len < MAVLINK_V2_HEADER_LEN) return false;
        
        // For v2: STX(1) + LEN(1) + INCOMPAT(1) + COMPAT(1) + SEQ(1) + SYS(1) + COMP(1) + MSG(3)
        // Payload length validation is done in findPacketBoundary
        // Incompat flags validation is done in findPacketBoundary
        return true;
    }
    
    return false;
}

int MavlinkDetector::findNextStartByte(const uint8_t* data, size_t len, size_t startPos) {
    size_t searchEnd = (startPos + MAVLINK_MAX_SEARCH_WINDOW < len) ? 
                       startPos + MAVLINK_MAX_SEARCH_WINDOW : len;
    
    for (size_t i = startPos; i < searchEnd; i++) {
        if (data[i] == MAVLINK_STX_V1 || data[i] == MAVLINK_STX_V2) {
            return static_cast<int>(i);
        }
    }
    
    return -1;  // No start byte found within search window
}