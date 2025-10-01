#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include "types.h"

// Print boot information
void printBootInfo();

// System diagnostics (memory stats)
void systemDiagnostics();

// Helper functions for device role names
const char* getDevice1RoleName(uint8_t role);
const char* getDevice2RoleName(uint8_t role);
const char* getDevice3RoleName(uint8_t role);
const char* getDevice4RoleName(uint8_t role);

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