#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <Arduino.h>

// Print boot information to Serial (only for critical reset reasons)
void printBootInfo();

// System diagnostics - memory stats, uptime (replaces debugOutput)
void systemDiagnostics();

#endif // DIAGNOSTICS_H