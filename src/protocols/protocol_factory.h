#ifndef PROTOCOL_FACTORY_H
#define PROTOCOL_FACTORY_H

#include "protocol_detector.h"
#include "protocol_stats.h"
#include "../types.h"

// Protocol types are defined in types.h

// Factory function to create protocol detector instances
ProtocolDetector* createProtocolDetector(ProtocolType type);

// Initialize protocol detection for bridge context
void initProtocolDetectionFactory(BridgeContext* ctx, ProtocolType protocolType);

// Cleanup protocol detection resources
void cleanupProtocolDetection(BridgeContext* ctx);

// Helper functions for protocol management
const char* getProtocolName(ProtocolType type);
bool isProtocolSupported(ProtocolType type);

#endif // PROTOCOL_FACTORY_H