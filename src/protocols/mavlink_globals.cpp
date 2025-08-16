#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#endif

#ifndef MAVLINK_USE_CONVENIENCE_FUNCTIONS
#define MAVLINK_USE_CONVENIENCE_FUNCTIONS 1
#endif
#ifndef MAVLINK_COMM_NUM_BUFFERS
#define MAVLINK_COMM_NUM_BUFFERS 1
#endif

#include <stdint.h>

#include "mavlink/mavlink_types.h"
extern mavlink_system_t mavlink_system;
void comm_send_ch(mavlink_channel_t chan, uint8_t ch);

#include "mavlink/ardupilotmega/mavlink.h"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

mavlink_system_t mavlink_system = {255, 0};

void comm_send_ch(mavlink_channel_t chan, uint8_t ch) {
    (void)chan; (void)ch;
}
