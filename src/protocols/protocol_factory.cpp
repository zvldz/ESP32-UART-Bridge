#include "protocol_factory.h"
#include "mavlink_detector.h"
#include "../logging.h"

ProtocolDetector* createProtocolDetector(ProtocolType type) {
    switch (type) {
        case PROTOCOL_MAVLINK:
            log_msg(LOG_INFO, "Creating MAVLink protocol detector");
            return new MavlinkDetector();
        
        case PROTOCOL_NONE:
        default:
            log_msg(LOG_DEBUG, "No protocol detector requested");
            return nullptr;
    }
}

void initProtocolDetectionFactory(BridgeContext* ctx, ProtocolType protocolType) {
    if (!ctx) {
        log_msg(LOG_ERROR, "BridgeContext is null in initProtocolDetection");
        return;
    }
    
    // Cleanup existing detector if any
    cleanupProtocolDetection(ctx);
    
    // Create new detector
    ctx->protocol.detector = createProtocolDetector(protocolType);
    
    // Initialize statistics - ensure it's properly created
    if (!ctx->protocol.stats) {
        ctx->protocol.stats = new ProtocolStats();
        log_msg(LOG_DEBUG, "Protocol statistics initialized");
    } else {
        ctx->protocol.stats->reset();
        log_msg(LOG_DEBUG, "Protocol statistics reset");
    }
    
    char logBuf[64];
    snprintf(logBuf, sizeof(logBuf), "Protocol detection initialized: %s", getProtocolName(protocolType));
    log_msg(LOG_INFO, "%s", logBuf);
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