// device_stats.cpp
#include "device_stats.h"
#include <Arduino.h>

// Global instance definition
DeviceStatistics g_deviceStats = {0};

// Initialize statistics at startup
void initDeviceStatistics() {
    resetDeviceStatistics(g_deviceStats, millis());
}

// Reset implementation
void resetDeviceStatistics(DeviceStatistics& stats, unsigned long currentTimeMs) {
    stats.device1.rxBytes.store(0, std::memory_order_relaxed);
    stats.device1.txBytes.store(0, std::memory_order_relaxed);
    stats.device1.rxPackets.store(0, std::memory_order_relaxed);
    stats.device1.txPackets.store(0, std::memory_order_relaxed);
    
    stats.device2.rxBytes.store(0, std::memory_order_relaxed);
    stats.device2.txBytes.store(0, std::memory_order_relaxed);
    stats.device2.rxPackets.store(0, std::memory_order_relaxed);
    stats.device2.txPackets.store(0, std::memory_order_relaxed);
    
    stats.device3.rxBytes.store(0, std::memory_order_relaxed);
    stats.device3.txBytes.store(0, std::memory_order_relaxed);
    stats.device3.rxPackets.store(0, std::memory_order_relaxed);
    stats.device3.txPackets.store(0, std::memory_order_relaxed);
    
    stats.device4.rxBytes.store(0, std::memory_order_relaxed);
    stats.device4.txBytes.store(0, std::memory_order_relaxed);
    stats.device4.rxPackets.store(0, std::memory_order_relaxed);
    stats.device4.txPackets.store(0, std::memory_order_relaxed);
    
    stats.lastGlobalActivity.store(0, std::memory_order_relaxed);
    stats.systemStartTime.store(currentTimeMs, std::memory_order_relaxed);
}