#pragma once

// ESP32 optimizations for FastMAVLink
#define FASTMAVLINK_PACKED_MESSAGES 1
#define FASTMAVLINK_CRC_EXTRA 1

// Support for MAVLink v1 and v2
#define FASTMAVLINK_MAVLINK1 1
#define FASTMAVLINK_MAVLINK2 1

// Signing support - parse size but don't verify signature
#define FASTMAVLINK_HAS_SIGNING 1
#define FASTMAVLINK_IGNORE_SIGNATURE 1

// Memory optimization for ESP32
#define FASTMAVLINK_STATIC_ASSERTION_FAILS_ON_PRINT_FIELD 1

// Suppress warnings about packed structures on ESP32
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#endif