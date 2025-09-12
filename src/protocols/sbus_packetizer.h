#ifndef SBUS_PACKETIZER_H
#define SBUS_PACKETIZER_H

#include "protocol_parser.h"
#include "sbus_common.h"
#include "../logging.h"

class SbusPacketizer : public ProtocolParser {
private:
    uint32_t lastOutputTime;
    uint16_t lastChannels[16];
    uint8_t lastFlags;
    bool hasData;
    uint32_t framesGenerated;
    
public:
    SbusPacketizer() : 
        lastOutputTime(0),
        lastFlags(0),
        hasData(false),
        framesGenerated(0) {
        // Initialize channels to center position (1500 microseconds = ~1024 in SBUS)
        for (int i = 0; i < 16; i++) {
            lastChannels[i] = 1024;  // Center position
        }
    }
    
    ParseResult parse(CircularBuffer* buffer, uint32_t currentTime) override {
        ParseResult result;  // Use default constructor
        
        // Check if it's time to generate SBUS frame (14ms interval)
        if (currentTime - lastOutputTime < (SBUS_UPDATE_RATE_MS * 1000)) {  // Convert ms to microseconds
            // Not time yet, but consume any available UART frames
            if (buffer->available() >= SBUS_FRAME_SIZE) {
                updateFromBuffer(buffer);
                result.bytesConsumed = SBUS_FRAME_SIZE;
            }
            return result;
        }
        
        // Consume any available UART frames before generating output
        while (buffer->available() >= SBUS_FRAME_SIZE) {
            updateFromBuffer(buffer);
            result.bytesConsumed += SBUS_FRAME_SIZE;
        }
        
        // Generate SBUS output frame
        SbusFrame frame;
        frame.startByte = SBUS_START_BYTE;
        packSbusChannels(lastChannels, frame.channelData);
        frame.flags = lastFlags;
        frame.endByte = 0x00;  // Standard end byte
        
        // Create parsed packet
        ParsedPacket* packet = new ParsedPacket();
        packet->data = new uint8_t[SBUS_FRAME_SIZE];
        memcpy(packet->data, &frame, SBUS_FRAME_SIZE);
        packet->size = SBUS_FRAME_SIZE;
        packet->format = DataFormat::FORMAT_SBUS;
        packet->hints.keepWhole = true;
        
        // Update statistics
        framesGenerated++;
        lastOutputTime = currentTime;
        
        if (stats) {
            stats->packetsTransmitted++;
            stats->totalBytes += SBUS_FRAME_SIZE;
            stats->lastPacketTime = millis();
        }
        
        result.packets = packet;
        result.count = 1;
        
        return result;
    }
    
    void reset() override {
        lastOutputTime = 0;
        hasData = false;
        framesGenerated = 0;
        // Keep channels at center position
        for (int i = 0; i < 16; i++) {
            lastChannels[i] = 1024;
        }
        lastFlags = 0;
    }
    
    const char* getName() const override {
        return "SBUS_Packetizer";
    }
    
    size_t getMinimumBytes() const override {
        return 0;  // Can generate frames without input
    }
    
private:
    void updateFromBuffer(CircularBuffer* buffer) {
        // Read SBUS frame from UART
        ContiguousView view = buffer->getContiguousForParser(SBUS_FRAME_SIZE);
        if (view.safeLen < SBUS_FRAME_SIZE) {
            return;
        }
        
        const uint8_t* data = view.ptr;
        
        // Validate it's a proper SBUS frame
        if (data[0] == SBUS_START_BYTE) {
            // Update channels and flags
            unpackSbusChannels(&data[1], lastChannels);
            lastFlags = data[23];
            hasData = true;
            
            // Consume the frame
            buffer->consume(SBUS_FRAME_SIZE);
        } else {
            // Skip invalid byte
            buffer->consume(1);
        }
    }
};

#endif // SBUS_PACKETIZER_H