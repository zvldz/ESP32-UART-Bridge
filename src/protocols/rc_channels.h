#ifndef RC_CHANNELS_H
#define RC_CHANNELS_H

#include <stdint.h>

#define RC_CHANNEL_COUNT 16

// Shared RC channel storage — written by SBUS/CRSF parsers, read by web API
struct RcChannelData {
    uint16_t channels[RC_CHANNEL_COUNT];  // Channel values in microseconds (988-2012)
    uint32_t lastUpdateMs;                // millis() of last update
};

// Global instance
extern RcChannelData rcChannels;

#endif // RC_CHANNELS_H
