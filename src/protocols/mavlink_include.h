// mavlink_include.h - unified MAVLink header inclusion point
#pragma once

#ifndef MAVLINK_INCLUDE_H
#define MAVLINK_INCLUDE_H

// 1. MAVLink configuration
#ifndef MAVLINK_USE_CONVENIENCE_FUNCTIONS
#define MAVLINK_USE_CONVENIENCE_FUNCTIONS 1
#endif
// Channels: 0=Telemetry, 1=USB, 2=UDP, 3=UART2, 4=UART3, 5=BT (MiniKit only)
// NOTE: If BLE transport added for S3 boards, change to 6 for all platforms
#ifndef MAVLINK_COMM_NUM_BUFFERS
#if defined(BOARD_MINIKIT_ESP32)
#define MAVLINK_COMM_NUM_BUFFERS 6
#else
#define MAVLINK_COMM_NUM_BUFFERS 5
#endif
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