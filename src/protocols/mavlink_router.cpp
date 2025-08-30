#include "mavlink_router.h"
#include "mavlink_include.h"  // For MAVLink definitions
#include <Arduino.h>

// List of messages that should always broadcast
static constexpr uint16_t ALWAYS_BROADCAST_IDS[] = {
    MAVLINK_MSG_ID_HEARTBEAT,
    MAVLINK_MSG_ID_SYS_STATUS,
    MAVLINK_MSG_ID_PARAM_VALUE,           // No target field
    MAVLINK_MSG_ID_ATTITUDE,
    MAVLINK_MSG_ID_GLOBAL_POSITION_INT,
    MAVLINK_MSG_ID_MISSION_CURRENT,
    MAVLINK_MSG_ID_VFR_HUD,
    MAVLINK_MSG_ID_COMMAND_ACK,           // Broadcast for compatibility
    MAVLINK_MSG_ID_STATUSTEXT
};


MavlinkRouter::MavlinkRouter() {
    // Initialize address book
    for (auto& entry : addressBook) {
        entry.sysId = 0;
        entry.interfaceMask = 0;
        entry.lastSeenMs = 0;
        entry.active = false;
    }
    
    // TEMPORARY: Pre-populate FC in address book with fake UART1 index
    // TODO: Remove when bidirectional pipeline implemented
    addressBook[0].sysId = 1;  // FC always sysid=1
    addressBook[0].interfaceMask = (1 << 4);  // IDX_UART1_FAKE = 4
    addressBook[0].lastSeenMs = 0xFFFFFFFF;  // Never expires
    addressBook[0].active = true;

    log_msg(LOG_INFO, "[ROUTER] TEMPORARY: FC sysid=1 on fake UART1 index 4");
}

// Check if message should always broadcast
bool MavlinkRouter::isAlwaysBroadcast(uint16_t msgId) {
    for (uint16_t id : ALWAYS_BROADCAST_IDS) {
        if (id == msgId) return true;
    }
    return false;
}

// Update address book with sender location
void MavlinkRouter::updateAddressBook(uint8_t sysId, uint8_t physicalInterface, uint32_t now) {
    if (sysId == 0 || physicalInterface == 0xFF) return;  // Invalid values
    
    uint8_t interfaceBit = 1 << physicalInterface;
    
    // Look for existing entry
    for (auto& entry : addressBook) {
        if (entry.active && entry.sysId == sysId) {
            entry.interfaceMask |= interfaceBit;
            entry.lastSeenMs = now;
            return;
        }
    }
    
    // Find empty slot for new entry
    for (auto& entry : addressBook) {
        if (!entry.active) {
            entry.sysId = sysId;
            entry.interfaceMask = interfaceBit;
            entry.lastSeenMs = now;
            entry.active = true;
            return;
        }
    }
    
    // No space - this is rare, log warning
    log_msg(LOG_WARNING, "[ROUTER] Address book full, ignoring sysId=%u", sysId);
}

// Find destination interfaces for target system
uint8_t MavlinkRouter::findDestinations(uint8_t targetSys, uint32_t now) {
    uint8_t mask = 0;
    
    for (auto& entry : addressBook) {
        if (!entry.active) continue;
        
        // Skip expired entries
        if (isExpired(now, entry.lastSeenMs, ADDR_TTL_MS)) continue;
        
        if (entry.sysId == targetSys) {
            mask |= entry.interfaceMask;
        }
    }
    
    return mask;
}

// Cleanup expired entries
void MavlinkRouter::cleanupExpiredEntries(uint32_t now) {
    for (auto& entry : addressBook) {
        if (entry.active && isExpired(now, entry.lastSeenMs, ADDR_TTL_MS)) {
            entry.active = false;
            entry.interfaceMask = 0;
        }
    }
}

void MavlinkRouter::process(ParsedPacket* packets, size_t count) {
    uint32_t now = millis();
    
    for (size_t i = 0; i < count; i++) {
        ParsedPacket& p = packets[i];
        
        // Clear routing hints
        p.hints.hasExplicitTarget = false;
        
        // Skip non-MAVLink packets
        if (p.protocol != PacketProtocol::MAVLINK) continue;
        
        // Learn sender location (including HEARTBEAT with msgid=0)
        updateAddressBook(p.routing.mavlink.sysId, p.physicalInterface, now);
        
        // Skip if always broadcast
        if (isAlwaysBroadcast(p.protocolMsgId)) {
            routingBroadcasts++;
            continue;
        }
        
        // Use pre-extracted target from parser
        uint8_t targetSys = p.routing.mavlink.targetSys;
        
        // Check for valid unicast target (0 means no target/broadcast)
        if (targetSys == 0) {
            routingBroadcasts++;
            continue;
        }
        
        // Find destination interfaces
        uint8_t destMask = findDestinations(targetSys, now);
        
        // Set explicit target only if single interface found
        if (destMask != 0 && __builtin_popcount(destMask) == 1) {
            p.hints.hasExplicitTarget = true;
            p.hints.targetDevices = destMask;
            routingHits++;
        } else {
            routingBroadcasts++;
        }
    }
    
    // Lazy cleanup of expired entries (every 10th call)
    static uint32_t cleanupCounter = 0;
    if (++cleanupCounter % 10 == 0) {
        cleanupExpiredEntries(now);
    }
    
    // Periodic address book dump (every 5 seconds)
    static uint32_t lastDumpMs = 0;
    if (millis() - lastDumpMs > 5000) {  // Every 5 seconds
        dumpAddressBook();
        log_msg(LOG_INFO, "[ROUTER] Stats: hits=%u broadcasts=%u", 
                routingHits, routingBroadcasts);
        lastDumpMs = millis();
    }
}

void MavlinkRouter::reset() {
    // Clear address book
    for (auto& entry : addressBook) {
        entry.active = false;
        entry.interfaceMask = 0;
    }
    
    // Reset statistics
    routingHits = 0;
    routingBroadcasts = 0;
    
    log_msg(LOG_INFO, "[ROUTER] MAVLink router reset");
}

void MavlinkRouter::getStats(uint32_t& hits, uint32_t& broadcasts) {
    hits = routingHits;
    broadcasts = routingBroadcasts;
}

void MavlinkRouter::learnAddress(uint8_t sysid, uint8_t physicalInterface) {
    // TEMPORARY: Public wrapper for input gateway
    // TODO: Remove when bidirectional pipeline implemented
    uint32_t now = millis();
    updateAddressBook(sysid, physicalInterface, now);
}

void MavlinkRouter::dumpAddressBook() {
    log_msg(LOG_INFO, "[ROUTER] Address book dump:");
    for (size_t i = 0; i < MAX_ADDRESSES; i++) {
        if (addressBook[i].active) {
            log_msg(LOG_INFO, "[ROUTER] [%zu] sysid=%u mask=0x%02X lastSeen=%u", 
                    i, addressBook[i].sysId, addressBook[i].interfaceMask, 
                    addressBook[i].lastSeenMs);
        }
    }
}