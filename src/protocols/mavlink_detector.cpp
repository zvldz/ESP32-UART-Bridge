#include "mavlink_detector.h"
#include "../logging.h"
#include <Arduino.h>  // For millis()

MavlinkDetector::MavlinkDetector() : currentVersion(0), headerValidated(false) {
    log_msg(LOG_INFO, "MAVLink detector initialized");
}

bool MavlinkDetector::canDetect(const uint8_t* data, size_t len) {
    if (len == 0) return false;
    
    // Check for MAVLink start bytes
    return (data[0] == MAVLINK_STX_V1 || data[0] == MAVLINK_STX_V2);
}

int MavlinkDetector::findPacketBoundary(const uint8_t* data, size_t len) {
    // Debug first call
    static bool firstCall = true;
    if (firstCall && len > 0) {
        log_msg(LOG_INFO, "MAVLink detector active, first byte: 0x%02X", data[0]);
        firstCall = false;
    }
    
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
            log_msg(LOG_INFO, "MAV: Resync invalid 0x%02X → next %d", data[0], nextStart);
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
                log_msg(LOG_INFO, "MAV: Resync v%d header fail → next %d", version, nextStart);
                // This is a resync event - log it
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
        log_msg(LOG_INFO, "MAV: Invalid payload len %d (max %d)", payloadLen, MAVLINK_MAX_PAYLOAD_LEN);
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
            log_msg(LOG_DEBUG, "MAVLink v2: Unsupported incompat_flags 0x%02X", incompatFlags);
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
    
    // If not found and we hit the search window limit
    if (searchEnd < len && searchEnd == startPos + MAVLINK_MAX_SEARCH_WINDOW) {
        // Skip to end of search window to make faster progress
        // This helps when there's garbage data before valid packets
        return static_cast<int>(MAVLINK_MAX_SEARCH_WINDOW);
    }
    
    // If searched less than window size, skip what we searched
    if (searchEnd > startPos + 1) {
        return static_cast<int>(searchEnd - startPos);
    }
    
    return 1;  // Default: skip 1 byte
}