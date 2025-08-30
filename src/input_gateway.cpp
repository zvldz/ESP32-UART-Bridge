#include "input_gateway.h"
#include "logging.h"
#include "uart/uart_interface.h"

// External UART1 interface
extern UartInterface* uartBridgeSerial;

InputGateway::InputGateway() : router(nullptr), enabled(false), 
                                packetsProcessed(0), sysidsLearned(0) {}

void InputGateway::init(MavlinkRouter* r, bool mavlinkRoutingEnabled) {
    router = r;
    enabled = mavlinkRoutingEnabled && (router != nullptr);
    
    if (enabled) {
        log_msg(LOG_INFO, "[GATEWAY] Input gateway initialized for MAVLink routing");
    }
}

void InputGateway::processInput(const uint8_t* data, size_t len, uint8_t sourceInterface) {
    // TEMPORARY: Direct passthrough to UART1
    // TODO: Remove when bidirectional pipeline implemented
    if (uartBridgeSerial) {
        uartBridgeSerial->write(data, len);
    }
    
    // If routing enabled, extract sysids for learning
    if (enabled && router) {
        packetsProcessed++;
        extractAllSysIds(data, len, sourceInterface);
    }
}

void InputGateway::extractAllSysIds(const uint8_t* data, size_t len, uint8_t sourceInterface) {
    // Variant A+: Scan entire buffer for all MAVLink packets
    size_t i = 0;
    bool foundAny = false;
    static uint32_t lastLogMs = 0;
    static uint32_t packetCount = 0;
    uint32_t now = millis();
    
    while (i < len) {
        // Need at least 6 bytes for MAVLink v1, 10 for v2
        // TODO: If this code remains after refactoring, replace magic numbers with constants:
        //       0xFD -> MAVLINK_STX, 0xFE -> MAVLINK_STX_V1, 10 -> MAVLINK_MIN_V2_LEN, 6 -> MAVLINK_MIN_V1_LEN
        if (data[i] == 0xFD && i + 10 <= len) {  // MAVLink v2
            uint8_t sysid = data[i + 5];  // TODO: use proper sysid offset constant
            router->learnAddress(sysid, sourceInterface);
            foundAny = true;
            sysidsLearned++;
            packetCount++;
            
            // Log only periodically to avoid spam
            if (now - lastLogMs > 10000) {  // Every 10 seconds
                log_msg(LOG_DEBUG, "[GATEWAY] Extracted %u packets, last: v2 sysid=%u from iface=%u", 
                        packetCount, sysid, sourceInterface);
                packetCount = 0;
                lastLogMs = now;
            }
            
            i += 2;  // Skip magic + length, continue scanning
        }
        else if (data[i] == 0xFE && i + 6 <= len) {  // MAVLink v1
            uint8_t sysid = data[i + 3];  // TODO: use proper sysid offset constant
            router->learnAddress(sysid, sourceInterface);
            foundAny = true;
            sysidsLearned++;
            packetCount++;
            
            // Log only periodically
            if (now - lastLogMs > 10000) {  // Every 10 seconds
                log_msg(LOG_DEBUG, "[GATEWAY] Extracted %u packets, last: v1 sysid=%u from iface=%u", 
                        packetCount, sysid, sourceInterface);
                packetCount = 0;
                lastLogMs = now;
            }
            
            i += 2;  // Skip magic + length
        }
        else {
            i++;
        }
    }
    
    // Only log "no MAVLink" once per minute to avoid spam
    if (!foundAny && len > 0) {
        static uint32_t lastNoMavlinkLogMs = 0;
        if (now - lastNoMavlinkLogMs > 60000) {  // Every 60 seconds
            log_msg(LOG_DEBUG, "[GATEWAY] No MAVLink found in %zu bytes from iface=%u", 
                    len, sourceInterface);
            lastNoMavlinkLogMs = now;
        }
    }
}

void InputGateway::getStats(uint32_t& packets, uint32_t& learned) {
    packets = packetsProcessed;
    learned = sysidsLearned;
}