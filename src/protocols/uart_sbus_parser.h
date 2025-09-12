#ifndef UART_SBUS_PARSER_H
#define UART_SBUS_PARSER_H

#include "protocol_parser.h"
#include "sbus_common.h"
#include "../logging.h"

class UartSbusParser : public ProtocolParser {
private:
    uint32_t framesFound;
    uint32_t invalidFrames;
    
public:
    UartSbusParser() : framesFound(0), invalidFrames(0) {}
    
    ParseResult parse(CircularBuffer* buffer, uint32_t currentTime) override {
        ParseResult result;
        
        // Need minimum 25 bytes for SBUS frame
        if (buffer->available() < SBUS_FRAME_SIZE) {
            return result;
        }
        
        // Get data for validation
        ContiguousView view = buffer->getContiguousForParser(SBUS_FRAME_SIZE);
        if (view.safeLen < SBUS_FRAME_SIZE) {
            return result;
        }
        
        const uint8_t* data = view.ptr;
        
        // Validation: must start with 0x0F
        if (data[0] != SBUS_START_BYTE) {
            // Not SBUS frame, skip 1 byte
            result.bytesConsumed = 1;
            invalidFrames++;
            return result;
        }
        
        // Simple end validation (can be 0x00, 0x04, 0x14, 0x24)
        uint8_t endByte = data[24];
        if (endByte != 0x00 && endByte != 0x04 && 
            endByte != 0x14 && endByte != 0x24) {
            // Invalid end, skip
            result.bytesConsumed = 1;
            invalidFrames++;
            return result;
        }
        
        // Valid SBUS frame found
        framesFound++;
        
        // Create packet for Hub
        ParsedPacket* packet = new ParsedPacket();
        packet->data = new uint8_t[SBUS_FRAME_SIZE];
        memcpy(packet->data, data, SBUS_FRAME_SIZE);
        packet->size = SBUS_FRAME_SIZE;
        packet->format = DataFormat::FORMAT_SBUS;
        packet->hints.keepWhole = true;
        
        // === TEMPORARY DIAGNOSTIC START ===
        // Log first and every 100th frame
        if (framesFound == 1 || framesFound % 100 == 0) {
            log_msg(LOG_INFO, "UART→SBUS: Frame %u received (invalid: %u)", 
                    framesFound, invalidFrames);
        }
        // === TEMPORARY DIAGNOSTIC END ===
        
        result.packets = packet;
        result.count = 1;
        result.bytesConsumed = SBUS_FRAME_SIZE;
        
        return result;
    }
    
    void reset() override {
        framesFound = 0;
        invalidFrames = 0;
    }
    
    const char* getName() const override {
        return "UART_SBUS_Parser";
    }
    
    size_t getMinimumBytes() const override {
        return SBUS_FRAME_SIZE;
    }
};

#endif // UART_SBUS_PARSER_H