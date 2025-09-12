#ifndef SBUS_PARSER_H
#define SBUS_PARSER_H

#include "protocol_parser.h"
#include "sbus_common.h"
#include "../logging.h"
#include "../config.h"

// External config for debug mode
extern Config config;

class SbusParser : public ProtocolParser {
private:
    uint32_t lastFrameTime;
    uint32_t frameLostCount;
    uint32_t failsafeCount;
    uint32_t validFrames;
    uint32_t invalidFrames;
    
public:
    SbusParser() : 
        lastFrameTime(0),
        frameLostCount(0),
        failsafeCount(0),
        validFrames(0),
        invalidFrames(0) {}
    
    ParseResult parse(CircularBuffer* buffer, uint32_t currentTime) override {
        ParseResult result;  // Use default constructor
        
        // Need at least one SBUS frame
        if (buffer->available() < SBUS_FRAME_SIZE) {
            return result;
        }
        
        // Get contiguous view of buffer
        ContiguousView view = buffer->getContiguousForParser(SBUS_FRAME_SIZE);
        if (view.safeLen < SBUS_FRAME_SIZE) {
            return result;
        }
        
        const uint8_t* data = view.ptr;
        
        // Validate SBUS frame
        if (data[0] != SBUS_START_BYTE) {
            // Not a valid frame start, skip one byte
            result.bytesConsumed = 1;
            invalidFrames++;
            if (stats) {
                stats->onDetectionError();
            }
            return result;
        }
        
        // Check end byte (can be 0x00, 0x04, 0x14, 0x24)
        uint8_t endByte = data[24];
        if (endByte != 0x00 && endByte != 0x04 && endByte != 0x14 && endByte != 0x24) {
            // Invalid end byte, skip one byte
            result.bytesConsumed = 1;
            invalidFrames++;
            if (stats) {
                stats->onDetectionError();
            }
            return result;
        }
        
        // Valid frame found
        validFrames++;
        lastFrameTime = currentTime;
        
        
        // Extract flags
        SbusFlags flags = extractSbusFlags(data[23]);
        if (flags.frameLost) frameLostCount++;
        if (flags.failsafe) failsafeCount++;
        
        // Minimal diagnostics - first frame and every 1000th
        static uint32_t frameCount = 0;
        frameCount++;
        
        if (frameCount == 1 || frameCount % 1000 == 0) {
            uint16_t channels[16];
            unpackSbusChannels(&data[1], channels);
            log_msg(LOG_INFO, "SBUS frame #%u: Ch1=%u Ch2=%u Ch3=%u Ch4=%u FS=%d FL=%d",
                    frameCount, channels[0], channels[1], channels[2], channels[3],
                    flags.failsafe ? 1 : 0, flags.frameLost ? 1 : 0);
        }
        
        // Create parsed packet with raw SBUS frame
        ParsedPacket* packet = new ParsedPacket();
        packet->data = new uint8_t[SBUS_FRAME_SIZE];
        memcpy(packet->data, data, SBUS_FRAME_SIZE);
        packet->size = SBUS_FRAME_SIZE;
        packet->format = DataFormat::FORMAT_SBUS;
        packet->hints.keepWhole = true;
        
        // Update statistics
        if (stats) {
            stats->onPacketDetected(SBUS_FRAME_SIZE, micros());
        }
        
        result.packets = packet;
        result.count = 1;
        result.bytesConsumed = SBUS_FRAME_SIZE;
        
        return result;
    }
    
    void reset() override {
        lastFrameTime = 0;
        frameLostCount = 0;
        failsafeCount = 0;
        validFrames = 0;
        invalidFrames = 0;
    }
    
    const char* getName() const override {
        return "SBUS_Parser";
    }
    
    size_t getMinimumBytes() const override {
        return SBUS_FRAME_SIZE;
    }
    
    // Get statistics
    uint32_t getValidFrames() const { return validFrames; }
    uint32_t getInvalidFrames() const { return invalidFrames; }
    uint32_t getFrameLostCount() const { return frameLostCount; }
    uint32_t getFailsafeCount() const { return failsafeCount; }
};

#endif // SBUS_PARSER_H