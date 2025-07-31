#ifndef MAVLINK_DETECTOR_H
#define MAVLINK_DETECTOR_H

#include "protocol_detector.h"

// MAVLink protocol constants
#define MAVLINK_STX_V1 0xFE
#define MAVLINK_STX_V2 0xFD
#define MAVLINK_V1_HEADER_LEN 6
#define MAVLINK_V2_HEADER_LEN 10
#define MAVLINK_MAX_PAYLOAD_LEN 255
#define MAVLINK_CHECKSUM_LEN 2
#define MAVLINK_SIGNATURE_LEN 13
#define MAVLINK_MAX_SEARCH_WINDOW 64  // Max bytes to search for next start

class MavlinkDetector : public ProtocolDetector {
private:
    // State for current packet detection
    uint8_t currentVersion;     // 1 or 2, 0 if unknown
    bool headerValidated;       // Header fields checked
    
    // Validate header fields for medium-level checking
    virtual bool validateHeader(const uint8_t* data, size_t len, uint8_t version);
    virtual bool validateExtended(const uint8_t* data, size_t len) { 
        return true;  // Not implemented - for future CRC/deep validation
    }
    virtual int findNextStartByte(const uint8_t* data, size_t len, size_t startPos);
    
public:
    MavlinkDetector();
    virtual ~MavlinkDetector() = default;
    
    // ProtocolDetector interface
    bool canDetect(const uint8_t* data, size_t len) override;
    int findPacketBoundary(const uint8_t* data, size_t len) override;
    const char* getName() const override { return "MAVLink"; }
    
    // Protocol-specific optimizations
    uint32_t getOptimalRxTimeout() const override { return 20; }  // 20 bit periods
    uint32_t getMaxPacketSize() const override { return 280; }   // Max v2 packet
    bool requiresTimingCheck() const override { return false; }
    uint8_t getPriority() const override { return 50; }  // Medium priority
};

// TODO Phase 4.2.3: Extended validation
// - validateExtended(): Optional deep validation for specific use cases
// - validateCRC(): Full CRC validation for high-reliability scenarios
// - Configuration option to select validation level (BASIC/EXTENDED/FULL)
// - Performance impact measurement of each validation level
// 
// Future validation levels:
// 1. BASIC (current): Start byte + payload length only
// 2. EXTENDED: + optional constraints (configurable per deployment)
// 3. FULL: + CRC check
// 4. STATISTICAL: + pattern analysis for auto-detection
//
// NOTE: System/Component IDs are NOT validated as they vary by device:
// - Flight controllers: usually 1/1
// - GCS: often 255/X
// - Modems: 51, 52, etc.
// - Companion computers: various IDs

#endif // MAVLINK_DETECTOR_H