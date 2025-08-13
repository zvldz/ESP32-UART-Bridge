#ifndef PROTOCOL_DETECTOR_H
#define PROTOCOL_DETECTOR_H

#include <stdint.h>
#include <stddef.h>

// Forward declarations
struct PacketDetectionResult;
class ProtocolStats;

// Base interface for protocol detection
class ProtocolDetector {
public:
    virtual ~ProtocolDetector() = default;
    
    // Check if detector can handle this data
    virtual bool canDetect(const uint8_t* data, size_t len) = 0;
    
    // Find packet boundary in buffer
    // Returns PacketDetectionResult with packet size and bytes to skip
    virtual PacketDetectionResult findPacketBoundary(const uint8_t* data, size_t len) = 0;
    
    // Reset detector state for cleanup
    virtual void reset() = 0;
    
    // Protocol name for logging
    virtual const char* getName() const = 0;
    
    // Optional: Protocol characteristics for optimization
    virtual uint32_t getOptimalRxTimeout() const { return 10; }  // DMA timeout in bit periods
    virtual uint32_t getMaxPacketSize() const { return 1024; }
    virtual bool requiresTimingCheck() const { return false; }
    
    // Protocol priority for future multi-protocol support
    virtual uint8_t getPriority() const { return 50; }  // 0-100, higher = priority
    
    // Set statistics pointer for detector
    virtual void setStats(ProtocolStats* statsPtr) { 
        // Default empty implementation
    }
    
    // Get minimum bytes needed for protocol detection
    // Each protocol should return its minimum header size
    virtual size_t getMinimumBytesNeeded() const = 0;
};

#endif // PROTOCOL_DETECTOR_H