#include "mavlink_detector.h"
#include <cstring>
#include <Arduino.h>

MavlinkDetector::MavlinkDetector() 
    : frameBuffer{0} {  // Initialize frame buffer with zeros
    fmav_init(); // Important! Called in example
    reset();
    log_msg(LOG_INFO, "MAV: FastMAVLink detector initialized with independent frame buffer");
}

void MavlinkDetector::reset() {
    // Reset FastMAVLink parser state
    fmav_status_reset(&fmavStatus);
    // Note: frameBuffer does NOT need clearing - parser manages it
    
    // Reset error counters
    signatureErrors = 0;
    crcErrors = 0;
    lengthErrors = 0;
    lastErrorReport = 0;
    
    log_msg(LOG_DEBUG, "MAV: Detector reset");
}

bool MavlinkDetector::canDetect(const uint8_t* data, size_t length) {
    // TEMPORARY DEBUG LOG - START
    static uint32_t canDetectCalls = 0;
    if (++canDetectCalls <= 10 || canDetectCalls % 100 == 0) {
        log_msg(LOG_DEBUG, "[TEMP] MAV canDetect #%u: len=%zu, minBytes=%zu, data[0]=%02X", 
                canDetectCalls, length, getMinimumBytesNeeded(), data ? data[0] : 0xFF);
    }
    // TEMPORARY DEBUG LOG - END
    
    // Check for sufficient data
    if (!data || length < getMinimumBytesNeeded()) {
        // TEMPORARY DEBUG LOG - START
        static uint32_t insufficientCount = 0;
        if (++insufficientCount <= 5) {
            log_msg(LOG_DEBUG, "[TEMP] MAV canDetect: insufficient data (len=%zu, need=%zu)", 
                    length, getMinimumBytesNeeded());
        }
        // TEMPORARY DEBUG LOG - END
        return false;
    }
    
    // Check for MAVLink start bytes
    bool isV1 = (data[0] == 0xFE);
    bool isV2 = (data[0] == 0xFD);
    
    // TEMPORARY DEBUG LOG - START
    static uint32_t checkCount = 0;
    if (++checkCount <= 10 || checkCount % 100 == 0) {
        log_msg(LOG_DEBUG, "[TEMP] MAV canDetect #%u: isV1=%d, isV2=%d, byte0=%02X", 
                checkCount, isV1, isV2, data[0]);
    }
    // TEMPORARY DEBUG LOG - END
    
    // FastMAVLink can start parsing from any byte, but we check for valid start
    // to avoid unnecessary processing
    return isV1 || isV2;
}

PacketDetectionResult MavlinkDetector::findPacketBoundary(const uint8_t* data, size_t length) {
    // TEMPORARY DEBUG LOG - START
    static uint32_t detectCalls = 0;
    if (++detectCalls <= 10 || detectCalls % 100 == 0) {
        log_msg(LOG_INFO, "[TEMP][DETECT] findPacketBoundary #%u: len=%zu", 
                detectCalls, length);
    }
    // TEMPORARY DEBUG LOG - END
    
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
                stats->onPacketDetected(result.frame_len, millis());
                // Keep this for verification that fix works
                static uint32_t detectCounter = 0;
                if (++detectCounter % 100 == 0) {
                    log_msg(LOG_INFO, "MAVLink: Detected %u packets (stats: %u total)", 
                            detectCounter, stats->packetsDetected);
                }
            }
            
            // Log packet detection periodically
            static uint32_t packetsFound = 0;
            packetsFound++;
            if (packetsFound % 1000 == 0) {
                log_msg(LOG_INFO, "MAV: Detected %u packets, last msgid=%u from %u:%u", 
                        packetsFound, result.msgid, result.sysid, result.compid);
            }
            
#ifdef DEBUG
            // Detailed debug logging
            if (packetsFound <= 10 || packetsFound % 1000 == 0) {
                log_msg(LOG_DEBUG, "MAV: Packet #%u - msgid=%u, size=%u, skip=%zu", 
                        packetsFound, result.msgid, result.frame_len, skipBytes);
            }
#endif
            
            // Return packet info
            // Parser automatically resets to IDLE state after complete packet
            return PacketDetectionResult(result.frame_len, skipBytes);
        }
        
        // Check for specific errors (but don't reset parser!)
        if (result.res == FASTMAVLINK_PARSE_RESULT_SIGNATURE_ERROR) {
            signatureErrors++;
            // This should not happen with FASTMAVLINK_IGNORE_SIGNATURE=1
            if (signatureErrors <= 5 || signatureErrors % 100 == 1) {
                log_msg(LOG_WARNING, "MAV: Signature error #%u (should not happen!)", signatureErrors);
            }
            if (stats) {
                stats->onDetectionError();
            }
        } else if (result.res == FASTMAVLINK_PARSE_RESULT_CRC_ERROR) {
            crcErrors++;
            if (crcErrors % 100 == 1) {
                log_msg(LOG_DEBUG, "MAV: CRC error #%u", crcErrors);
            }
            if (stats) {
                stats->onDetectionError();
                stats->onResyncEvent();
            }
        } else if (result.res == FASTMAVLINK_PARSE_RESULT_LENGTH_ERROR) {
            lengthErrors++;
            if (lengthErrors % 100 == 1) {
                log_msg(LOG_DEBUG, "MAV: Length error #%u", lengthErrors);
            }
            if (stats) {
                stats->onDetectionError();
            }
        }
        // Note: MSGID_UNKNOWN is not logged (normal for extensions)
    }
    
    // No complete packet found yet - need more data
    return PacketDetectionResult(0, 0);
}

