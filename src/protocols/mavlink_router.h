#pragma once
#include "protocol_router.h"
#include "../logging.h"

class MavlinkRouter : public ProtocolRouter {
private:
    static constexpr size_t MAX_ADDRESSES = 12;
    static constexpr uint32_t ADDR_TTL_MS = 120000;  // 2 minutes
    
    struct AddressEntry {
        uint8_t sysId;
        uint8_t interfaceMask;   // Bitmask of interfaces where seen
        uint32_t lastSeenMs;
        bool active;
    };
    
    AddressEntry addressBook[MAX_ADDRESSES];
    uint32_t routingHits = 0;
    uint32_t routingBroadcasts = 0;
    
    // Helper: check if time expired (handles wrap-around)
    static bool isExpired(uint32_t now, uint32_t last, uint32_t ttl) {
        return (int32_t)(now - last) > (int32_t)ttl;
    }
    
    // Update address book with sender location
    void updateAddressBook(uint8_t sysId, uint8_t physicalInterface, uint32_t now);
    
    // Find interfaces for target system
    uint8_t findDestinations(uint8_t targetSys, uint32_t now);
    
    // Check if message should always broadcast
    bool isAlwaysBroadcast(uint16_t msgId);
    
    
    // Cleanup expired entries
    void cleanupExpiredEntries(uint32_t now);
    
public:
    MavlinkRouter();
    void process(ParsedPacket* packets, size_t count) override;
    void reset() override;
    void getStats(uint32_t& hits, uint32_t& broadcasts) override;
    
    // Debug: dump address book contents (LOG_DEBUG level)
    void dumpAddressBook();
};