// mavlink_include.h - unified MAVLink header inclusion point
#pragma once

#ifndef MAVLINK_INCLUDE_H
#define MAVLINK_INCLUDE_H

// 1. MAVLink configuration
#ifndef MAVLINK_USE_CONVENIENCE_FUNCTIONS
#define MAVLINK_USE_CONVENIENCE_FUNCTIONS 1
#endif
#ifndef MAVLINK_COMM_NUM_BUFFERS
#define MAVLINK_COMM_NUM_BUFFERS 4
#endif

// 2. Disable warnings
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#endif

// 3. Basic types
#include "mavlink/mavlink_types.h"

// 4. Forward declarations
extern mavlink_system_t mavlink_system;
void comm_send_ch(mavlink_channel_t chan, uint8_t ch);

// 5. Full MAVLink
#include "mavlink/ardupilotmega/mavlink.h"

// 6. Restore warnings
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif // MAVLINK_INCLUDE_H