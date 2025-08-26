#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include "types.h"

// Print boot information
void printBootInfo();

// System diagnostics (memory stats)
void systemDiagnostics();

// Helper functions for device role names
const char* getDevice2RoleName(uint8_t role);
const char* getDevice3RoleName(uint8_t role);
const char* getDevice4RoleName(uint8_t role);

// Statistics management functions removed - using atomic g_deviceStats
// updateSharedStats and resetStatistics removed - using direct atomic operations

// Statistics update functions removed - using atomic operations in g_deviceStats

// Separate diagnostic functions for TaskScheduler
void runBridgeActivityLog();
void runStackDiagnostics();
void runDroppedDataStats();
void runAllStacksDiagnostics();

// Set bridge context for diagnostic functions
void setBridgeContext(BridgeContext* ctx);

// Get bridge context for protocol stats access
BridgeContext* getBridgeContext();

#ifdef DEBUG
    void forceSerialLog(const char* format, ...) __attribute__((format(printf, 1, 2)));
#endif

#endif // DIAGNOSTICS_H