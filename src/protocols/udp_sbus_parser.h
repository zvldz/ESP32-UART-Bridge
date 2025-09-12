#ifndef UDP_SBUS_PARSER_H
#define UDP_SBUS_PARSER_H

#include "protocol_parser.h"
#include "sbus_common.h"
#include "../logging.h"

class UdpSbusParser : public ProtocolParser {
private:
    uint32_t framesReceived;
    uint32_t invalidPackets;
    
public:
    UdpSbusParser() : framesReceived(0), invalidPackets(0) {}
    
    ParseResult parse(CircularBuffer* buffer, uint32_t currentTime) override {
        ParseResult result;
        
        // UDP packets should be exactly 25 bytes
        if (buffer->available() < SBUS_FRAME_SIZE) {
            return result;
        }
        
        // Get data
        ContiguousView view = buffer->getContiguousForParser(SBUS_FRAME_SIZE);
        if (view.safeLen < SBUS_FRAME_SIZE) {
            return result;
        }
        
        const uint8_t* data = view.ptr;
        
        // Validate SBUS frame
        if (data[0] != SBUS_START_BYTE) {
            // Not SBUS, skip entire packet (UDP packets are atomic)
            result.bytesConsumed = buffer->available();  // Clear entire buffer
            invalidPackets++;
            // === TEMPORARY DIAGNOSTIC START ===
            log_msg(LOG_DEBUG, "UDP→SBUS: Invalid packet (bad start byte)");
            // === TEMPORARY DIAGNOSTIC END ===
            return result;
        }
        
        // Check end byte
        uint8_t endByte = data[24];
        if (endByte != 0x00 && endByte != 0x04 && 
            endByte != 0x14 && endByte != 0x24) {
            result.bytesConsumed = SBUS_FRAME_SIZE;
            invalidPackets++;
            // === TEMPORARY DIAGNOSTIC START ===
            log_msg(LOG_DEBUG, "UDP→SBUS: Invalid packet (bad end byte)");
            // === TEMPORARY DIAGNOSTIC END ===
            return result;
        }
        
        // Valid SBUS frame received via UDP
        framesReceived++;
        
        // Create packet for Hub
        ParsedPacket* packet = new ParsedPacket();
        packet->data = new uint8_t[SBUS_FRAME_SIZE];
        memcpy(packet->data, data, SBUS_FRAME_SIZE);
        packet->size = SBUS_FRAME_SIZE;
        packet->format = DataFormat::FORMAT_SBUS;
        packet->hints.keepWhole = true;
        packet->physicalInterface = PHYS_UDP;  // Mark source
        
        // === TEMPORARY DIAGNOSTIC START ===
        // Diagnostics
        if (framesReceived == 1 || framesReceived % 100 == 0) {
            log_msg(LOG_INFO, "UDP→SBUS: %u frames received (invalid: %u)", 
                    framesReceived, invalidPackets);
        }
        // === TEMPORARY DIAGNOSTIC END ===
        
        result.packets = packet;
        result.count = 1;
        result.bytesConsumed = SBUS_FRAME_SIZE;
        
        return result;
    }
    
    void reset() override {
        framesReceived = 0;
        invalidPackets = 0;
    }
    
    const char* getName() const override {
        return "UDP_SBUS_Parser";
    }
    
    size_t getMinimumBytes() const override {
        return SBUS_FRAME_SIZE;
    }
};

#endif