// device_stats.h
#pragma once
#include <atomic>

// Device statistics structure
struct DeviceStatistics {
    struct DeviceCounter {
        std::atomic<unsigned long> rxBytes{0};
        std::atomic<unsigned long> txBytes{0};
        std::atomic<unsigned long> rxPackets{0};  // Only used by Device4
        std::atomic<unsigned long> txPackets{0};  // Only used by Device4
    };
    
    DeviceCounter device1;
    DeviceCounter device2;
    DeviceCounter device3;
    DeviceCounter device4;
    
    std::atomic<unsigned long> systemStartTime{0};
    std::atomic<unsigned long> lastGlobalActivity{0};
};

// Global instance declaration  
extern DeviceStatistics g_deviceStats;

// Function declarations
void initDeviceStatistics();
void resetDeviceStatistics(DeviceStatistics& stats, unsigned long currentTimeMs);