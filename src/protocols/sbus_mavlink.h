// SBUS to MAVLink RC_CHANNELS_OVERRIDE Conversion
// Converts binary SBUS frames to MAVLink messages for wireless RC control
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "sbus_common.h"
#include "sbus_text.h"  // For sbusToUs()
#include "mavlink_include.h"  // Project's unified MAVLink header

// MAVLink RC_OVERRIDE packet size: header(10) + payload(38) + crc(2) = 50 bytes (MAVLink v2)
// MAVLink v1: header(6) + payload(18) + crc(2) = 26 bytes (but we use v2 for 18 channels)
static constexpr size_t SBUS_MAVLINK_BUFFER_SIZE = 64;

// System/Component IDs for RC_CHANNELS_OVERRIDE
static constexpr uint8_t RC_OVERRIDE_SYSTEM_ID = 255;      // GCS system ID
static constexpr uint8_t RC_OVERRIDE_COMPONENT_ID = 190;   // MAV_COMP_ID_UDP_BRIDGE
static constexpr uint8_t RC_OVERRIDE_TARGET_SYSTEM = 1;    // FC system ID
static constexpr uint8_t RC_OVERRIDE_TARGET_COMPONENT = 1; // FC component ID

// Convert binary SBUS frame to MAVLink RC_CHANNELS_OVERRIDE packet
// Returns length of MAVLink packet (0 on error)
// buffer must be at least SBUS_MAVLINK_BUFFER_SIZE bytes
inline size_t sbusFrameToMavlink(const uint8_t* frame, uint8_t* buffer, size_t bufferSize) {
    if (bufferSize < SBUS_MAVLINK_BUFFER_SIZE || frame[0] != SBUS_START_BYTE) {
        return 0;
    }

    // Decode SBUS channels
    uint16_t channels[SBUS_CHANNELS];
    unpackSbusChannels(frame + 1, channels);  // Skip start byte

    // Convert to microseconds (1000-2000 range)
    uint16_t ch_us[18];
    for (int i = 0; i < 16; i++) {
        ch_us[i] = sbusToUs(channels[i]);
    }
    // CH17 and CH18 from flags (digital channels)
    uint8_t flags = frame[23];
    ch_us[16] = (flags & 0x80) ? 2000 : 1000;  // CH17
    ch_us[17] = (flags & 0x40) ? 2000 : 1000;  // CH18

    // Build MAVLink message
    mavlink_message_t msg;
    mavlink_msg_rc_channels_override_pack(
        RC_OVERRIDE_SYSTEM_ID,
        RC_OVERRIDE_COMPONENT_ID,
        &msg,
        RC_OVERRIDE_TARGET_SYSTEM,
        RC_OVERRIDE_TARGET_COMPONENT,
        ch_us[0], ch_us[1], ch_us[2], ch_us[3],
        ch_us[4], ch_us[5], ch_us[6], ch_us[7],
        ch_us[8], ch_us[9], ch_us[10], ch_us[11],
        ch_us[12], ch_us[13], ch_us[14], ch_us[15],
        ch_us[16], ch_us[17]
    );

    // Serialize to buffer
    uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);
    return len;
}
