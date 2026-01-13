// SBUS Text Format Conversion
// Converts binary SBUS frames to text format compatible with TX16S-RC
// Format: "RC 1500,1500,...\r\n" (16 channels in microseconds)
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "sbus_common.h"

// Text output buffer size: "RC " + 16 channels * 5 chars max + 15 commas + "\r\n" + null
static constexpr size_t SBUS_TEXT_BUFFER_SIZE = 3 + 16 * 5 + 15 + 2 + 1;

// Convert SBUS raw value to microseconds
// OpenTX/EdgeTX standard: 172 → 988µs, 992 → 1500µs, 1811 → 2012µs
// Digital channels (CH17/CH18) are handled specially
inline int sbusToUs(int raw) {
    // Digital CH17/CH18 (flags) - high value means ON
    if (raw > 10000) return 2012;  // was 2000 in Betaflight-style
    if (raw == 0) return 988;      // was 1000 in Betaflight-style

    // OpenTX/EdgeTX compatible conversion (matches radio display)
    // 172 → 988µs, 992 → 1500µs, 1811 → 2012µs
    return 988 + (int)((raw - 172) * 1024.0f / (1811 - 172) + 0.5f);

    // Old Betaflight-style formula (1000-2000µs range):
    // return 1000 + (int)((raw - 173) * 1000.0f / (1811 - 173));
}

// Convert binary SBUS frame to text format
// Returns length of text string (excluding null terminator)
// buffer must be at least SBUS_TEXT_BUFFER_SIZE bytes
inline size_t sbusFrameToText(const uint8_t* frame, char* buffer, size_t bufferSize) {
    if (bufferSize < SBUS_TEXT_BUFFER_SIZE || frame[0] != SBUS_START_BYTE) {
        return 0;
    }

    // Decode channels using sbus_common.h function
    uint16_t channels[SBUS_CHANNELS];
    unpackSbusChannels(frame + 1, channels);  // Skip start byte

    // Format: "RC 1500,1500,...\r\n"
    int len = snprintf(buffer, bufferSize,
        "RC %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
        sbusToUs(channels[0]),  sbusToUs(channels[1]),  sbusToUs(channels[2]),  sbusToUs(channels[3]),
        sbusToUs(channels[4]),  sbusToUs(channels[5]),  sbusToUs(channels[6]),  sbusToUs(channels[7]),
        sbusToUs(channels[8]),  sbusToUs(channels[9]),  sbusToUs(channels[10]), sbusToUs(channels[11]),
        sbusToUs(channels[12]), sbusToUs(channels[13]), sbusToUs(channels[14]), sbusToUs(channels[15]));

    return (len > 0 && (size_t)len < bufferSize) ? len : 0;
}
