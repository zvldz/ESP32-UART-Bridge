#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <Arduino.h>
#include "types.h"

// Print boot information to Serial (only for critical reset reasons)
void printBootInfo();

// System diagnostics - memory stats, uptime (replaces debugOutput)
void systemDiagnostics();

// Helper functions for device role names
const char* getDevice2RoleName(uint8_t role);
const char* getDevice3RoleName(uint8_t role);

// Thread-safe statistics update
void updateSharedStats(unsigned long device1Rx, unsigned long device1Tx,
                      unsigned long device2Rx, unsigned long device2Tx,
                      unsigned long device3Rx, unsigned long device3Tx,
                      unsigned long lastActivity);

// Statistics reset
void resetStatistics(UartStats* stats);

// Bridge diagnostics functions
void logBridgeActivity(BridgeContext* ctx, DeviceMode currentMode);
void logStackDiagnostics(BridgeContext* ctx);
void logDmaStatistics(class UartInterface* uartSerial);
void logDroppedDataStats(BridgeContext* ctx);

// Main periodic diagnostics update function
void updatePeriodicDiagnostics(BridgeContext* ctx, DeviceMode currentMode);

#endif // DIAGNOSTICS_H