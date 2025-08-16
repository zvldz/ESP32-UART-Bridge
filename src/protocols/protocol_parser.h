#ifndef PROTOCOL_PARSER_H
#define PROTOCOL_PARSER_H

#include "protocol_types.h"
#include "protocol_stats.h"
#include "../circular_buffer.h"
#include "../types.h"

class ProtocolParser {
protected:
    ProtocolStats* stats;
    
public:
    ProtocolParser() : stats(nullptr) {}
    virtual ~ProtocolParser() {}
    
    // Main parsing method - analyzes buffer and returns packets
    virtual ParseResult parse(
        CircularBuffer* buffer,
        uint32_t currentTime
    ) = 0;
    
    // Reset parser state
    virtual void reset() = 0;
    
    // Get parser name for logging
    virtual const char* getName() const = 0;
    
    // Get minimum bytes needed for parsing
    virtual size_t getMinimumBytes() const = 0;
    
    // Handle backpressure - which packets to drop
    virtual void prioritizePackets(
        ParsedPacket* packets, 
        size_t count,
        size_t availableSpace
    ) {
        // Default: no prioritization, keep all
    }
    
    // Set statistics collector
    void setStats(ProtocolStats* s) { 
        stats = s; 
    }
    
    // Adaptive timeout support - check if protocol needs extended timeouts
    virtual bool requiresExtendedTimeout() const { 
        return false;  // By default use normal timeout
    }
    
    // EXPERIMENTAL: Flush strategy hooks
    // TODO: Remove if not effective in production
    virtual bool shouldFlushNow(size_t pendingPackets, uint32_t timeSinceLastMs) const {
        // Default: flush after 2ms or 10 packets
        return timeSinceLastMs > 2 || pendingPackets >= 10;
    }
    
    virtual uint32_t getBatchTimeoutMs() const {
        // Default: 2ms batching window
        return 2;
    }
    
    // EXPERIMENTAL: For future use if needed
    // virtual size_t getMaxBatchSize() const { return 10; }
};

#endif // PROTOCOL_PARSER_H