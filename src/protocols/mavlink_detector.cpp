#include "mavlink_detector.h"
#include <cstring>
#include <Arduino.h>

MavlinkDetector::MavlinkDetector() {
    fmav_init(); // Important! Called in example
    reset();
    log_msg(LOG_INFO, "MAV: FastMAVLink detector initialized (DroneBridge approach)");
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
    // FastMAVLink can start parsing from any byte
    // Keep original behavior - trust FastMAVLink to find sync
    return length > 0;
}

PacketDetectionResult MavlinkDetector::findPacketBoundary(const uint8_t* data, size_t length) {
    if (length == 0) return PacketDetectionResult(0, 0);
    
    // Process byte by byte using DroneBridge approach
    for (size_t i = 0; i < length; i++) {
        fmav_result_t result = {0};
        
        // Parse byte into independent frame buffer
        uint8_t res = fmav_parse_and_check_to_frame_buf(
            &result,      // Result with metadata
            frameBuffer,  // Independent buffer, NOT the data buffer!
            &fmavStatus,  // Parser state (persistent between calls)
            data[i]       // Single byte to parse
        );
        
        // Check if complete packet was assembled
        if (res) {
            // Packet complete in frameBuffer!
            // Calculate how many bytes to skip before packet start
            // Example: if we processed 100 bytes (i=99, i+1=100) and packet is 80 bytes,
            // then we had 20 bytes of garbage before packet start
            size_t skipBytes = 0;
            if (i + 1 >= result.frame_len) {
                skipBytes = (i + 1) - result.frame_len;
            }
            
            // Update statistics if available
            if (stats) {
                stats->onPacketDetected(result.frame_len, millis());
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

