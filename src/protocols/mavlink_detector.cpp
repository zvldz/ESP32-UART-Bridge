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
    
    // Extract payload length FIRST - this is critical for validation
    uint8_t payloadLen = data[1];
    
    // CRITICAL CHECK: Validate payload length immediately
    if (payloadLen > MAVLINK_MAX_PAYLOAD_LEN) {
        log_msg(LOG_INFO, "MAV: Invalid payload len %d (max %d)", payloadLen, MAVLINK_MAX_PAYLOAD_LEN);
        // Search for next start byte
        int nextStart = findNextStartByte(data, len, 1);
        if (nextStart > 0) {
            return -nextStart;
        }
        return -1;
    }
    
    // Calculate expected total packet size
    size_t expectedSize = headerLen + payloadLen + MAVLINK_CHECKSUM_LEN;
    
    // For MAVLink v2, check for signature flag
    if (version == 2 && len >= 3) {
        uint8_t incompatFlags = data[2];
        
        // ENHANCED CHECK: Detect if this might be a false positive
        // If we see another MAVLink start byte where we expect data, it's likely misaligned
        if (len >= 4 && (data[3] == MAVLINK_STX_V1 || data[3] == MAVLINK_STX_V2)) {
            log_msg(LOG_WARNING, "MAV: Detected nested start byte at offset 3, likely misaligned");
            // Skip to the nested start byte
            int nextStart = findNextStartByte(data, len, 1);
            return -nextStart;
        }
        
        // Debug logging for strange flags (A. from action plan)
        if (incompatFlags != 0x00 && incompatFlags != 0x01) {
            log_msg(LOG_WARNING, "MAVLink v2: Unusual incompat_flags 0x%02X", incompatFlags);
            
            // Hex dump first 20 bytes of packet for context
            char hexBuf[128];
            int pos = 0;
            size_t dumpLen = (len > 20) ? 20 : len;
            
            for (size_t i = 0; i < dumpLen; i++) {
                pos += snprintf(hexBuf + pos, sizeof(hexBuf) - pos, "%02X ", data[i]);
                if (i == 2) {
                    // Mark the problematic byte
                    pos += snprintf(hexBuf + pos, sizeof(hexBuf) - pos, "<-- ");
                }
            }
            log_msg(LOG_WARNING, "Packet context: %s", hexBuf);
        }
        
        if (incompatFlags & 0x01) {  // MAVLINK_IFLAG_SIGNED
            expectedSize += MAVLINK_SIGNATURE_LEN;
        }
        
        // Check for invalid incompat_flags that indicate misalignment
        // Valid flags are only 0x00 or 0x01 currently
        if (incompatFlags & 0xFE) {
            // Special case: if incompat_flags is 0xFD or 0xFE, it's likely another packet start
            if (incompatFlags == 0xFD || incompatFlags == 0xFE) {
                log_msg(LOG_WARNING, "MAV: incompat_flags is start byte (0x%02X), misaligned!", incompatFlags);
                // We're looking at offset 2, but the real packet starts at offset 2
                return -2;  // Skip 2 bytes to get to the real start
            }
            
            log_msg(LOG_DEBUG, "MAVLink v2: Unsupported incompat_flags 0x%02X", incompatFlags);
            int nextStart = findNextStartByte(data, len, 1);
            if (nextStart > 0) {
                return -nextStart;
            }
            return -1;
        }
    }
    
    // IMPORTANT: Check if we have enough data for the complete packet
    if (len < expectedSize) {
        // Additional safety check - make sure we're not waiting for an impossibly large packet
        if (expectedSize > 300) {  // MAVLink max is ~280 bytes
            log_msg(LOG_WARNING, "MAV: Unrealistic packet size %zu, likely misaligned", expectedSize);
            int nextStart = findNextStartByte(data, len, 1);
            return -nextStart;
        }
        return 0;  // Need more data
    }
    
    // EXTRA VALIDATION: Check if there's another start byte where payload should be
    // This catches many misalignment cases
    if (expectedSize < len) {
        // Check byte right after where packet should end
        uint8_t nextByte = data[expectedSize];
        if (nextByte == MAVLINK_STX_V1 || nextByte == MAVLINK_STX_V2) {
            // Good, next packet starts right after this one
        } else if (expectedSize + 1 < len) {
            // Check one byte further (in case of off-by-one)
            uint8_t nextNextByte = data[expectedSize + 1];
            if (nextNextByte == MAVLINK_STX_V1 || nextNextByte == MAVLINK_STX_V2) {
                log_msg(LOG_WARNING, "MAV: Next packet start is off by one, size might be wrong");
            }
        }
    }
    
    // If packet seems way too small, it's likely a false positive
    if (expectedSize < 8) {  // Minimum viable MAVLink packet
        log_msg(LOG_WARNING, "MAV: Packet too small (%zu bytes), likely false positive", expectedSize);
        int nextStart = findNextStartByte(data, len, 1);
        return -nextStart;
    }
    
    // Log packet size statistics (B. from action plan)
    static uint32_t detectedPackets = 0;
    static uint64_t totalSizeSum = 0;
    static uint32_t minSize = 9999;
    static uint32_t maxSize = 0;
    
    detectedPackets++;
    totalSizeSum += expectedSize;
    if (expectedSize < minSize) minSize = expectedSize;
    if (expectedSize > maxSize) maxSize = expectedSize;
    
    // Log first packet and every 100th packet
    if (detectedPackets == 1 || detectedPackets % 100 == 0) {
        uint32_t avgSize = (detectedPackets > 0) ? (totalSizeSum / detectedPackets) : 0;
        log_msg(LOG_INFO, "MAVLink stats after %u packets: avg=%u, min=%u, max=%u, current=%zu (v%d, payload=%d)",
                detectedPackets, avgSize, minSize, maxSize, expectedSize, version, payloadLen);
    }
    
    // Special logging for impossibly small packets
    if (expectedSize < 12 && version == 2) {
        log_msg(LOG_ERROR, "MAVLink v2 packet too small: %zu bytes (payload=%d)", 
                expectedSize, payloadLen);
        // Hex dump the packet
        char hexBuf[64];
        int pos = 0;
        for (size_t i = 0; i < expectedSize && i < 20; i++) {
            pos += snprintf(hexBuf + pos, sizeof(hexBuf) - pos, "%02X ", data[i]);
        }
        log_msg(LOG_ERROR, "Small packet dump: %s", hexBuf);
    }
    
    // All checks passed - we have a valid packet
    return static_cast<int>(expectedSize);
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
    // Enhanced resync logging (C. from action plan)
    static uint32_t resyncCount = 0;
    bool logThisResync = false;
    
    // Track resync for logging
    size_t searchStart = startPos;  // Save original position for logging
    
    size_t searchEnd = (startPos + MAVLINK_MAX_SEARCH_WINDOW < len) ? 
                       startPos + MAVLINK_MAX_SEARCH_WINDOW : len;
    
    for (size_t i = startPos; i < searchEnd; i++) {
        if (data[i] == MAVLINK_STX_V1 || data[i] == MAVLINK_STX_V2) {
            // Found next start byte - this is a resync event
            resyncCount++;
            
            // Always log first 5 resyncs, then every 10th
            if (resyncCount <= 5 || resyncCount % 10 == 0) {
                logThisResync = true;
                
                log_msg(LOG_WARNING, "Protocol: Resync #%u at offset %zu, skipping %d bytes",
                        resyncCount, searchStart, static_cast<int>(i - startPos));
                
                // Show what we're skipping
                char hexBuf[64];
                int pos = 0;
                size_t showBytes = ((i - startPos) < 10) ? (i - startPos) : 10;
                for (size_t j = 0; j < showBytes && (startPos + j) < len; j++) {
                    pos += snprintf(hexBuf + pos, sizeof(hexBuf) - pos, "%02X ", 
                                   data[startPos + j]);
                }
                log_msg(LOG_WARNING, "Skipping bytes: %s", hexBuf);
            }
            
            return static_cast<int>(i);
        }
    }
    
    // If not found and we hit the search window limit
    if (searchEnd < len && searchEnd == startPos + MAVLINK_MAX_SEARCH_WINDOW) {
        // Skip to end of search window to make faster progress
        // This helps when there's garbage data before valid packets
        
        // Log this large skip
        resyncCount++;
        if (resyncCount <= 5 || resyncCount % 10 == 0) {
            log_msg(LOG_WARNING, "Protocol: Resync #%u - large skip %d bytes (no start found)",
                    resyncCount, MAVLINK_MAX_SEARCH_WINDOW);
        }
        
        return static_cast<int>(MAVLINK_MAX_SEARCH_WINDOW);
    }
    
    // If searched less than window size, skip what we searched
    if (searchEnd > startPos + 1) {
        return static_cast<int>(searchEnd - startPos);
    }
    
    return 1;  // Default: skip 1 byte
}