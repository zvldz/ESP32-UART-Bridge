#pragma once

#include "protocol_detector.h"
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

// Structure for packet detection result
struct PacketDetectionResult {
    int packetSize;    // Size of detected packet in bytes
    int skipBytes;     // Number of bytes to skip before packet start (garbage/sync bytes)
    
    // Constructor for convenience
    PacketDetectionResult() : packetSize(0), skipBytes(0) {}
    PacketDetectionResult(int size, int skip) : packetSize(size), skipBytes(skip) {}
};

class MavlinkDetector : public ProtocolDetector {
public:
    MavlinkDetector();
    ~MavlinkDetector() override = default;
    
    // ProtocolDetector interface
    bool canDetect(const uint8_t* data, size_t length) override;
    PacketDetectionResult findPacketBoundary(const uint8_t* data, size_t length) override;
    void reset() override;
    void setStats(ProtocolStats* stats) { this->stats = stats; }
    const char* getName() const override { return "MAVLink/FastMAV"; }
    
    // Protocol characteristics
    uint32_t getOptimalRxTimeout() const override { return 20; }
    uint32_t getMaxPacketSize() const override { return 280; }
    bool requiresTimingCheck() const override { return false; }
    uint8_t getPriority() const override { return 50; }
    
private:
    ProtocolStats* stats = nullptr;
    
    // FastMAVLink state
    fmav_status_t fmavStatus;
    uint8_t frameBuffer[296];  // Independent buffer for frame assembly
    
    // Error counters for diagnostics
    uint32_t signatureErrors = 0;
    uint32_t crcErrors = 0;
    uint32_t lengthErrors = 0;
    uint32_t lastErrorReport = 0;
};