#ifndef PROTOCOL_STATS_H
#define PROTOCOL_STATS_H

#include <stdint.h>

// Protocol detection and processing statistics
struct ProtocolStats {
    // Packet statistics
    uint32_t packetsDetected;       // Successfully detected packets
    uint32_t packetsTransmitted;    // Packets sent through optimized path
    uint32_t detectionErrors;       // Header validation failures
    uint32_t resyncEvents;          // Times detector had to search for next start byte
    
    // Performance metrics
    uint32_t totalBytes;            // Total bytes processed through protocol detector
    uint32_t minPacketSize;         // Smallest detected packet
    uint32_t maxPacketSize;         // Largest detected packet
    uint32_t avgPacketSize;         // Running average packet size
    
    // Timing statistics
    uint32_t lastPacketTime;        // Timestamp of last detected packet (millis)
    uint32_t packetsPerSecond;      // Current packet rate (updated every second)
    
    // Error tracking
    uint32_t consecutiveErrors;     // Current streak of errors (for future auto-disable)
    uint32_t maxConsecutiveErrors;  // Highest error streak seen
    
    // Constructor
    ProtocolStats() { reset(); }
    
    // Reset all statistics
    void reset() {
        packetsDetected = 0;
        packetsTransmitted = 0;
        detectionErrors = 0;
        resyncEvents = 0;
        totalBytes = 0;
        minPacketSize = UINT32_MAX;
        maxPacketSize = 0;
        avgPacketSize = 0;
        lastPacketTime = 0;
        packetsPerSecond = 0;
        consecutiveErrors = 0;
        maxConsecutiveErrors = 0;
    }
    
    // Update packet size statistics
    void updatePacketSize(uint32_t size) {
        if (size < minPacketSize) minPacketSize = size;
        if (size > maxPacketSize) maxPacketSize = size;
        
        // Simple running average (can be improved with proper windowing)
        if (packetsDetected == 0) {
            avgPacketSize = size;
        } else {
            avgPacketSize = (avgPacketSize * packetsDetected + size) / (packetsDetected + 1);
        }
    }
    
    // Update packet rate (called periodically)
    void updatePacketRate(uint32_t currentTime) {
        static uint32_t lastRateUpdate = 0;
        static uint32_t packetsInLastSecond = 0;
        
        if (currentTime - lastRateUpdate >= 1000) {  // Every second
            packetsPerSecond = packetsInLastSecond;
            packetsInLastSecond = 0;
            lastRateUpdate = currentTime;
        } else {
            packetsInLastSecond++;
        }
    }
    
    // Record successful packet detection
    void onPacketDetected(uint32_t size, uint32_t currentTime) {
        packetsDetected++;
        totalBytes += size;
        updatePacketSize(size);
        lastPacketTime = currentTime;
        updatePacketRate(currentTime);
        consecutiveErrors = 0;  // Reset error streak
    }
    
    // Record packet transmission through optimized path
    void onPacketTransmitted(uint32_t size) {
        packetsTransmitted++;
    }
    
    // Record detection error
    void onDetectionError() {
        detectionErrors++;
        consecutiveErrors++;
        if (consecutiveErrors > maxConsecutiveErrors) {
            maxConsecutiveErrors = consecutiveErrors;
        }
    }
    
    // Record resynchronization event
    void onResyncEvent() {
        resyncEvents++;
    }
};

#endif // PROTOCOL_STATS_H