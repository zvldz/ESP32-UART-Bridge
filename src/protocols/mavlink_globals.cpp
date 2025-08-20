// mavlink_globals.cpp - global MAVLink variable definitions
#include "mavlink_include.h"

// Global MAVLink system structure definition
mavlink_system_t mavlink_system = {255, 0};

// Byte send hook stub (required by pymavlink)
void comm_send_ch(mavlink_channel_t chan, uint8_t ch) {
    // Empty implementation - we don't use MAVLink for sending
    (void)chan; 
    (void)ch;
}
