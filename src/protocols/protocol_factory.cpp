#include "protocol_factory.h"
#include "mavlink_detector.h"
#include "../logging.h"

ProtocolDetector* createProtocolDetector(ProtocolType type) {
    switch (type) {
        case PROTOCOL_MAVLINK:
            log_msg("Creating MAVLink protocol detector", LOG_DEBUG);
            return new MavlinkDetector();
        
        case PROTOCOL_NONE:
        default:
            log_msg("No protocol detector requested", LOG_DEBUG);
            return nullptr;
    }
}

void initProtocolDetectionFactory(BridgeContext* ctx, ProtocolType protocolType) {
    if (!ctx) {
        log_msg("BridgeContext is null in initProtocolDetection", LOG_ERROR);
        return;
    }
    
    // Cleanup existing detector if any
    cleanupProtocolDetection(ctx);
    
    // Create new detector
    ctx->protocol.detector = createProtocolDetector(protocolType);
    
    // Initialize statistics
    if (!ctx->protocol.stats) {
        ctx->protocol.stats = new ProtocolStats();
    } else {
        ctx->protocol.stats->reset();
    }
    
    log_msg("Protocol detection initialized: " + String(getProtocolName(protocolType)), LOG_INFO);
}

void cleanupProtocolDetection(BridgeContext* ctx) {
    if (!ctx) return;
    
    if (ctx->protocol.detector) {
        delete ctx->protocol.detector;
        ctx->protocol.detector = nullptr;
    }
    
    // Note: We don't delete stats here as they might be accessed for display
    // They will be reset when new protocol is initialized
}

const char* getProtocolName(ProtocolType type) {
    switch (type) {
        case PROTOCOL_MAVLINK:
            return "MAVLink";
        case PROTOCOL_NONE:
        default:
            return "None";
    }
}

bool isProtocolSupported(ProtocolType type) {
    switch (type) {
        case PROTOCOL_NONE:
        case PROTOCOL_MAVLINK:
            return true;
        default:
            return false;
    }
}