#ifndef SBUS_COMMON_H
#define SBUS_COMMON_H

#include <stdint.h>
#include "../defines.h"

// SBUS frame structure - exactly 25 bytes
#pragma pack(push, 1)
struct SbusFrame {
    uint8_t startByte;           // 0x0F
    uint8_t channelData[22];     // Packed 16 channels (11 bits each)
    uint8_t flags;               // Flags and channels 17-18
    uint8_t endByte;             // 0x00 or 0x04 or 0x14 or 0x24
};
#pragma pack(pop)

// SBUS flags structure
struct SbusFlags {
    bool ch17;        // bit 7 (0x80) - digital channel
    bool ch18;        // bit 6 (0x40) - digital channel  
    bool frameLost;   // bit 5 (0x20) - frame lost indicator
    bool failsafe;    // bit 4 (0x10) - failsafe active
};

// Helper function to extract flags from SBUS frame
inline SbusFlags extractSbusFlags(uint8_t flagByte) {
    SbusFlags flags;
    flags.ch17 = (flagByte & 0x80) != 0;
    flags.ch18 = (flagByte & 0x40) != 0;
    flags.frameLost = (flagByte & 0x20) != 0;
    flags.failsafe = (flagByte & 0x10) != 0;
    return flags;
}

// Unpack 11-bit SBUS channels from packed data
inline void unpackSbusChannels(const uint8_t* data, uint16_t* channels) {
    // Unpack 16 channels (11 bits each) from 22 bytes
    channels[0]  = ((data[0]     | data[1]<<8)                 & 0x07FF);
    channels[1]  = ((data[1]>>3  | data[2]<<5)                 & 0x07FF);
    channels[2]  = ((data[2]>>6  | data[3]<<2  | data[4]<<10)  & 0x07FF);
    channels[3]  = ((data[4]>>1  | data[5]<<7)                 & 0x07FF);
    channels[4]  = ((data[5]>>4  | data[6]<<4)                 & 0x07FF);
    channels[5]  = ((data[6]>>7  | data[7]<<1  | data[8]<<9)   & 0x07FF);
    channels[6]  = ((data[8]>>2  | data[9]<<6)                 & 0x07FF);
    channels[7]  = ((data[9]>>5  | data[10]<<3)                & 0x07FF);
    channels[8]  = ((data[11]    | data[12]<<8)                & 0x07FF);
    channels[9]  = ((data[12]>>3 | data[13]<<5)                & 0x07FF);
    channels[10] = ((data[13]>>6 | data[14]<<2 | data[15]<<10) & 0x07FF);
    channels[11] = ((data[15]>>1 | data[16]<<7)                & 0x07FF);
    channels[12] = ((data[16]>>4 | data[17]<<4)                & 0x07FF);
    channels[13] = ((data[17]>>7 | data[18]<<1 | data[19]<<9)  & 0x07FF);
    channels[14] = ((data[19]>>2 | data[20]<<6)                & 0x07FF);
    channels[15] = ((data[20]>>5 | data[21]<<3)                & 0x07FF);
}

// Pack channels into SBUS data format
inline void packSbusChannels(const uint16_t* channels, uint8_t* data) {
    // Pack 16 channels (11 bits each) into 22 bytes
    data[0]  = channels[0] & 0xFF;
    data[1]  = ((channels[0] >> 8) & 0x07) | ((channels[1] << 3) & 0xF8);
    data[2]  = ((channels[1] >> 5) & 0x3F) | ((channels[2] << 6) & 0xC0);
    data[3]  = (channels[2] >> 2) & 0xFF;
    data[4]  = ((channels[2] >> 10) & 0x01) | ((channels[3] << 1) & 0xFE);
    data[5]  = ((channels[3] >> 7) & 0x0F) | ((channels[4] << 4) & 0xF0);
    data[6]  = ((channels[4] >> 4) & 0x7F) | ((channels[5] << 7) & 0x80);
    data[7]  = (channels[5] >> 1) & 0xFF;
    data[8]  = ((channels[5] >> 9) & 0x03) | ((channels[6] << 2) & 0xFC);
    data[9]  = ((channels[6] >> 6) & 0x1F) | ((channels[7] << 5) & 0xE0);
    data[10] = (channels[7] >> 3) & 0xFF;
    data[11] = channels[8] & 0xFF;
    data[12] = ((channels[8] >> 8) & 0x07) | ((channels[9] << 3) & 0xF8);
    data[13] = ((channels[9] >> 5) & 0x3F) | ((channels[10] << 6) & 0xC0);
    data[14] = (channels[10] >> 2) & 0xFF;
    data[15] = ((channels[10] >> 10) & 0x01) | ((channels[11] << 1) & 0xFE);
    data[16] = ((channels[11] >> 7) & 0x0F) | ((channels[12] << 4) & 0xF0);
    data[17] = ((channels[12] >> 4) & 0x7F) | ((channels[13] << 7) & 0x80);
    data[18] = (channels[13] >> 1) & 0xFF;
    data[19] = ((channels[13] >> 9) & 0x03) | ((channels[14] << 2) & 0xFC);
    data[20] = ((channels[14] >> 6) & 0x1F) | ((channels[15] << 5) & 0xE0);
    data[21] = (channels[15] >> 3) & 0xFF;
}

#endif // SBUS_COMMON_H