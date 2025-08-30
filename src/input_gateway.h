#pragma once
#include <stdint.h>
#include "protocols/mavlink_router.h"

// TEMPORARY: Input Gateway for MAVLink routing
// TODO: Remove when bidirectional pipeline implemented
class InputGateway {
private:
    MavlinkRouter* router;
    bool enabled;
    
    // Statistics
    uint32_t packetsProcessed;
    uint32_t sysidsLearned;
    
    // Extract all sysids from buffer (handles multiple packets)
    void extractAllSysIds(const uint8_t* data, size_t len, uint8_t sourceInterface);
    
public:
    InputGateway();
    void init(MavlinkRouter* r, bool mavlinkRoutingEnabled);
    void processInput(const uint8_t* data, size_t len, uint8_t sourceInterface);
    void getStats(uint32_t& packets, uint32_t& learned);
    bool isEnabled() const { return enabled; }
};