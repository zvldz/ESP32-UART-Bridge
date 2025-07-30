#ifndef PROTOCOL_DETECTOR_H
#define PROTOCOL_DETECTOR_H

#include <stdint.h>
#include <stddef.h>

// Base interface for protocol detection
class ProtocolDetector {
public:
    virtual ~ProtocolDetector() = default;
    
    // Check if detector can handle this data
    virtual bool canDetect(const uint8_t* data, size_t len) = 0;
    
    // Find packet boundary in buffer
    // Returns: > 0 = packet size, 0 = need more data, < 0 = error/not this protocol
    virtual int findPacketBoundary(const uint8_t* data, size_t len) = 0;
    
    // Protocol name for logging
    virtual const char* getName() const = 0;
    
    // Optional: Protocol characteristics for optimization
    virtual uint32_t getOptimalRxTimeout() const { return 10; }  // DMA timeout in bit periods
    virtual uint32_t getMaxPacketSize() const { return 1024; }
    virtual bool requiresTimingCheck() const { return false; }
};

#endif // PROTOCOL_DETECTOR_H