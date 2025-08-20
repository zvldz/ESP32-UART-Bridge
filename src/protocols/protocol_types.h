#ifndef PROTOCOL_TYPES_H
#define PROTOCOL_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Protocol types
enum ProtocolType {
    PROTOCOL_RAW = 0,       // Adaptive buffering without parsing (default)
    PROTOCOL_NONE = 0,      // Compatibility alias for PROTOCOL_RAW
    PROTOCOL_MAVLINK = 1,   // MAVLink packet detection
    PROTOCOL_SBUS = 2,      // Future: SBUS protocol
    PROTOCOL_CRSF = 3,      // Future: CRSF protocol
};

// Transmission hints for optimized sending
struct TransmitHints {
    bool keepWhole;          // Don't fragment (important for UDP)
    bool urgentFlush;        // Send immediately, don't batch
    uint32_t interPacketGap; // Microseconds between packets (for UART)
    bool canFragment;        // OK to send partially if needed
    bool canBatch;           // OK to group with other packets (for UDP)
    uint8_t targetDevices;   // Bitmask: which devices should receive
    
    // Default constructor
    TransmitHints() : 
        keepWhole(false), 
        urgentFlush(false), 
        interPacketGap(0),
        canFragment(true), 
        canBatch(true),
        targetDevices(0xFF) {}
};

// Forward declaration
class PacketMemoryPool;

// === DIAGNOSTIC START === (Remove after MAVLink stabilization)
// Global sequence counter for packet tracking
static uint32_t globalSeqNum = 0;
// === DIAGNOSTIC END ===

// Parsed packet structure
struct ParsedPacket {
    uint8_t* data;           // Pointer to packet data
    size_t size;             // Packet size
    size_t allocSize;        // Allocated size (for pool)
    uint32_t timestamp;      // When packet was received (micros)
    TransmitHints hints;     // Transmission optimization hints
    PacketMemoryPool* pool;  // Pool to return memory to
    
    // === DIAGNOSTIC START === (Remove after MAVLink stabilization)
    uint32_t parseTimeMicros;     // When packet was parsed
    uint32_t enqueueTimeMicros;   // When packet was enqueued  
    uint32_t seqNum;              // This packet's sequence number
    uint16_t mavlinkMsgId;        // MAVLink message ID for tracking
    // === DIAGNOSTIC END ===
    
    ParsedPacket() : data(nullptr), size(0), allocSize(0), 
                    timestamp(0), pool(nullptr)
    // === DIAGNOSTIC START === (Remove after MAVLink stabilization)
                    , parseTimeMicros(0), enqueueTimeMicros(0), 
                    seqNum(0), mavlinkMsgId(0)
    // === DIAGNOSTIC END ===
    {}
    
    // Duplicate packet (allocates from pool if available)
    ParsedPacket duplicate() const;
    
    // Cleanup - returns to pool or deletes
    void free();
};

// Parser result - can contain multiple packets
struct ParseResult {
    ParsedPacket* packets;   // Array of parsed packets
    size_t count;            // Number of packets
    size_t bytesConsumed;    // How many bytes were processed from buffer
    
    ParseResult() : packets(nullptr), count(0), bytesConsumed(0) {}
    
    // Cleanup - CRITICAL: always call this!
    void free() {
        if (packets) {
            for (size_t i = 0; i < count; i++) {
                packets[i].free();  // Each packet returns to pool
            }
            delete[] packets;
            packets = nullptr;
        }
        count = 0;
    }
};

#endif // PROTOCOL_TYPES_H