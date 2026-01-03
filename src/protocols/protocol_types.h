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
    uint32_t interPacketGap; // Microseconds between packets (for UART)
    bool canFragment;        // OK to send partially if needed
    uint8_t targetDevices;   // Bitmask: which devices should receive
    bool hasExplicitTarget;  // Router found unique destination
    
    // Default constructor
    TransmitHints() : 
        keepWhole(false), 
        interPacketGap(0),
        canFragment(true), 
        targetDevices(0xFF),
        hasExplicitTarget(false) {}
};

// Forward declaration
class PacketMemoryPool;

// Protocol identification
enum class PacketProtocol : uint8_t {
    UNKNOWN = 0,
    MAVLINK = 1,
    RAW = 2,
    // Future protocols
};

// Data format for protocol-specific handling
enum class DataFormat : uint8_t {
    FORMAT_RAW = 0,        // Raw data, no specific format
    FORMAT_MAVLINK = 1,    // MAVLink formatted data  
    FORMAT_SBUS = 2,       // SBUS formatted data
    FORMAT_CRSF = 3,       // Future: CRSF formatted data
    FORMAT_UNKNOWN = 0xFF  // Unknown format
};

// Packet source identification for routing
enum PacketSource {
    SOURCE_DATA = 0,       // Default: data flow (telemetry, commands, SBUS, etc.)
    SOURCE_LOGS = 1,       // Log data from logging system
    SOURCE_DEVICE4 = 2     // UDP incoming data (legacy)
};

// Parsed packet structure
struct ParsedPacket {
    uint8_t* data;           // Pointer to packet data
    size_t size;             // Packet size
    size_t allocSize;        // Allocated size (for pool)
    TransmitHints hints;     // Transmission optimization hints
    PacketMemoryPool* pool;  // Pool to return memory to
    PacketSource source;     // Source of packet data for routing
    
    // Protocol identification
    PacketProtocol protocol;        // Set by parser
    DataFormat format;              // Data format for protocol-specific handling
    
    // Protocol fields
    uint16_t protocolMsgId;         // Message ID for routing (HEARTBEAT=0 is valid)
    uint8_t physicalInterface;      // Source interface (UART1/USB/UDP index)
    
    // Protocol-specific routing data
    union {
        struct {
            uint8_t sysId;
            uint8_t compId;
            uint8_t targetSys;       // Will be extracted by router, not parser
            uint8_t targetComp;      // Will be extracted by router, not parser
        } mavlink;
        // Future protocols can add their structures here
    } routing;

    // Diagnostic field (useful for latency analysis on new platforms)
    uint32_t parseTimeMicros;     // When packet was parsed

    ParsedPacket() : data(nullptr), size(0), allocSize(0),
                    pool(nullptr), source(SOURCE_DATA),
                    protocol(PacketProtocol::UNKNOWN), format(DataFormat::FORMAT_UNKNOWN),
                    protocolMsgId(0), physicalInterface(0xFF),
                    parseTimeMicros(0)
    {
        // Initialize routing union
        routing.mavlink.sysId = 0;
        routing.mavlink.compId = 0;
        routing.mavlink.targetSys = 0;
        routing.mavlink.targetComp = 0;
    }
    
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

// Physical interface identification (for routing and anti-echo)
// CRITICAL: Must match sender indices exactly!
enum PhysicalInterface : uint8_t {
    PHYS_USB   = 0,  // MUST match IDX_DEVICE2_USB
    PHYS_UART2 = 1,  // MUST match IDX_DEVICE2_UART2
    PHYS_UART3 = 2,  // MUST match IDX_DEVICE3
    PHYS_UDP   = 3,  // MUST match IDX_DEVICE4
    PHYS_UART1 = 4,  // MUST match IDX_UART1
#if defined(BOARD_MINIKIT_ESP32)
    PHYS_BT    = 5,  // MUST match IDX_DEVICE5 (Bluetooth SPP, MiniKit only)
#endif
    PHYS_NONE  = 0xFF // No physical interface (internal sources)
};

// Sender indices - MUST match PhysicalInterface values
enum SenderIndex : uint8_t {
    IDX_DEVICE2_USB   = 0,
    IDX_DEVICE2_UART2 = 1,
    IDX_DEVICE3       = 2,
    IDX_DEVICE4       = 3,
    IDX_UART1         = 4,
#if defined(BOARD_MINIKIT_ESP32)
    IDX_DEVICE5       = 5,  // Bluetooth SPP (MiniKit only)
    MAX_SENDERS       = 6
#else
    MAX_SENDERS       = 5
#endif
};

// Compile-time verification of mapping
static_assert(static_cast<int>(PHYS_USB) == static_cast<int>(IDX_DEVICE2_USB), "PHYS_USB must match IDX_DEVICE2_USB");
static_assert(static_cast<int>(PHYS_UART2) == static_cast<int>(IDX_DEVICE2_UART2), "PHYS_UART2 must match IDX_DEVICE2_UART2");
static_assert(static_cast<int>(PHYS_UART3) == static_cast<int>(IDX_DEVICE3), "PHYS_UART3 must match IDX_DEVICE3");
static_assert(static_cast<int>(PHYS_UDP) == static_cast<int>(IDX_DEVICE4), "PHYS_UDP must match IDX_DEVICE4");
static_assert(static_cast<int>(PHYS_UART1) == static_cast<int>(IDX_UART1), "PHYS_UART1 must match IDX_UART1");
#if defined(BOARD_MINIKIT_ESP32)
static_assert(static_cast<int>(PHYS_BT) == static_cast<int>(IDX_DEVICE5), "PHYS_BT must match IDX_DEVICE5");
#endif

// Helper functions to avoid casts throughout code
inline uint8_t physicalInterfaceBit(PhysicalInterface iface) {
    // PHYS_NONE returns 0 (no bit set)
    return (static_cast<int>(iface) < static_cast<int>(MAX_SENDERS)) ? (1 << iface) : 0;
}

inline bool isValidPhysicalInterface(PhysicalInterface iface) {
    return static_cast<int>(iface) < static_cast<int>(MAX_SENDERS);
}

inline SenderIndex physicalToSenderIndex(PhysicalInterface iface) {
    // Safe conversion with validation
    return (static_cast<int>(iface) < static_cast<int>(MAX_SENDERS)) ? static_cast<SenderIndex>(iface) : MAX_SENDERS;
}

#endif // PROTOCOL_TYPES_H