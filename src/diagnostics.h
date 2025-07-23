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

// Statistics management
void updateSharedStats(unsigned long device1Rx, unsigned long device1Tx,
                      unsigned long device2Rx, unsigned long device2Tx,
                      unsigned long device3Rx, unsigned long device3Tx,
                      unsigned long lastActivity);
void resetStatistics(UartStats* stats);

// Separate diagnostic functions for TaskScheduler
void runBridgeActivityLog();
void runStackDiagnostics();
void runDroppedDataStats();
void runAllStacksDiagnostics();

// Set bridge context for diagnostic functions
void setBridgeContext(BridgeContext* ctx);

#endif // DIAGNOSTICS_H