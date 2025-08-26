#include "protocol_pipeline.h"
#include "../logging.h"

ProtocolPipeline::ProtocolPipeline(BridgeContext* context) : 
    activeFlows(0),
    ctx(context) {
    // Initialize sender slots to nullptr
    for (size_t i = 0; i < MAX_SENDERS; i++) {
        senders[i] = nullptr;
    }
}

ProtocolPipeline::~ProtocolPipeline() {
    cleanup();
}

void ProtocolPipeline::init(Config* config) {
    // Initialize sender slots to nullptr
    for (size_t i = 0; i < MAX_SENDERS; i++) {
        senders[i] = nullptr;
    }
    
    // Setup data flows based on configuration
    setupFlows(config);
    
    // Create senders based on configuration
    createSenders(config);
    
    log_msg(LOG_INFO, "Protocol pipeline initialized: %zu flows, senders fixed slots=4", activeFlows);
}

void ProtocolPipeline::setupFlows(Config* config) {
    activeFlows = 0;
    
    // Flow 1: Telemetry
    uint8_t telemetryMask = 0;
    
    // Device2 routing
    if (config->device2.role == D2_USB) {
        telemetryMask |= SENDER_USB;
    } else if (config->device2.role == D2_UART2) {
        telemetryMask |= SENDER_UART2;  // NEW!
    }
    
    // Device3 routing
    if (config->device3.role == D3_UART3_MIRROR || 
        config->device3.role == D3_UART3_BRIDGE) {
        telemetryMask |= SENDER_UART3;
    }
    
    // Device4 routing - ONLY for BRIDGE mode, not for LOGGER
    if (config->device4.role == D4_NETWORK_BRIDGE) {
        telemetryMask |= SENDER_UDP;
    }
    
    // Check for no destinations configured
    if (telemetryMask == 0) {
        log_msg(LOG_WARNING, "No telemetry destinations configured - data will be dropped");
    }
    
    // Create flow with updated mask
    if (telemetryMask && ctx->buffers.telemetryBuffer) {
        DataFlow f;
        f.name = "Telemetry";
        f.inputBuffer = ctx->buffers.telemetryBuffer;
        f.source = SOURCE_TELEMETRY;
        f.senderMask = telemetryMask;
        
        // Create parser based on protocol type
        switch (config->protocolOptimization) {
            case PROTOCOL_NONE:
                f.parser = new RawParser();
                break;
            case PROTOCOL_MAVLINK:
                f.parser = new MavlinkParser();
                break;
            default:
                f.parser = new RawParser();
                break;
        }
        
        // Link statistics
        if (ctx->protocol.stats) {
            f.parser->setStats(ctx->protocol.stats);
        }
        
        flows[activeFlows++] = f;
        
        // Log the final telemetry routing mask for debugging
        log_msg(LOG_INFO, "Telemetry routing mask: 0x%02X (USB:%d UART2:%d UART3:%d UDP:%d)",
                telemetryMask,
                (telemetryMask & SENDER_USB) ? 1 : 0,
                (telemetryMask & SENDER_UART2) ? 1 : 0,
                (telemetryMask & SENDER_UART3) ? 1 : 0,
                (telemetryMask & SENDER_UDP) ? 1 : 0);
    } else if (telemetryMask) {
        log_msg(LOG_ERROR, "Telemetry buffer not allocated but telemetry senders configured!");
    }
    
    // Flow 2: Logger (Logger mode only)
    if (config->device4.role == D4_LOG_NETWORK && ctx->buffers.logBuffer) {
        DataFlow f;
        f.name = "Logger";
        f.inputBuffer = ctx->buffers.logBuffer;
        f.source = SOURCE_LOGS;
        f.senderMask = SENDER_UDP;  // Logs only go to UDP
        f.parser = new LineBasedParser();
        
        // Link statistics
        if (ctx->protocol.stats) {
            f.parser->setStats(ctx->protocol.stats);
        }
        
        flows[activeFlows++] = f;
        log_msg(LOG_INFO, "Logger flow created with LineBasedParser");
    } else if (config->device4.role == D4_LOG_NETWORK) {
        log_msg(LOG_ERROR, "Log buffer not allocated for Logger mode!");
    }
}

void ProtocolPipeline::createSenders(Config* config) {
    // Initialize all slots to nullptr first
    for (size_t i = 0; i < MAX_SENDERS; i++) {
        senders[i] = nullptr;
    }
    
    // Device2 - USB or UART2 (mutually exclusive)
    if (config->device2.role == D2_USB && ctx->interfaces.usbInterface) {
        senders[IDX_DEVICE2_USB] = new UsbSender(
            ctx->interfaces.usbInterface
        );
        log_msg(LOG_INFO, "Created USB sender at index %d", IDX_DEVICE2_USB);
    } 
    else if (config->device2.role == D2_UART2 && ctx->interfaces.device2Serial) {
        senders[IDX_DEVICE2_UART2] = new Uart2Sender(
            ctx->interfaces.device2Serial
        );
        log_msg(LOG_INFO, "Created UART2 sender at index %d", IDX_DEVICE2_UART2);
    }
    
    // Device3 - UART3 (for MIRROR and BRIDGE modes only)
    if ((config->device3.role == D3_UART3_MIRROR || 
         config->device3.role == D3_UART3_BRIDGE) &&
        ctx->interfaces.device3Serial) {
        senders[IDX_DEVICE3] = new Uart3Sender(
            ctx->interfaces.device3Serial
        );
        log_msg(LOG_INFO, "Created UART3 sender at index %d", IDX_DEVICE3);
    }
    
    // Device4 - UDP (unchanged)
    if ((config->device4.role == D4_NETWORK_BRIDGE || 
         config->device4.role == D4_LOG_NETWORK)) {
        extern AsyncUDP* udpTransport;
        if (udpTransport) {
            UdpSender* udpSender = new UdpSender(
                udpTransport
            );
            udpSender->setBatchingEnabled(config->udpBatchingEnabled);
            senders[IDX_DEVICE4] = udpSender;
            log_msg(LOG_INFO, "Created UDP sender at index %d", IDX_DEVICE4);
        }
    }
}

void ProtocolPipeline::process() {
    // Process all data flows
    for (size_t i = 0; i < activeFlows; i++) {
        processFlow(flows[i]);
    }
    
    // Calculate bulk mode from ALL active flows (not just first!)
    bool bulkMode = false;
    for (size_t i = 0; i < activeFlows; i++) {
        if (flows[i].parser && flows[i].parser->isBurstActive()) {
            bulkMode = true;
            break;
        }
    }
    
    // Process send queues for all senders with bulk mode state
    for (size_t i = 0; i < MAX_SENDERS; i++) {
        if (senders[i]) {
            senders[i]->processSendQueue(bulkMode);
        }
    }
}

void ProtocolPipeline::processFlow(DataFlow& flow) {
    if (!flow.parser || !flow.inputBuffer) return;
    
    uint32_t now = micros();
    
    // Parse packets from input buffer
    ParseResult result = flow.parser->parse(flow.inputBuffer, now);
    
    // ALWAYS consume bytes if parser processed them
    if (result.bytesConsumed > 0) {
        flow.inputBuffer->consume(result.bytesConsumed);
    }
    
    // THEN distribute packets if parser found them
    if (result.count > 0) {
        distributePackets(result.packets, result.count, flow.source, flow.senderMask);
    }
    
    // Clean up parse result
    result.free();
}

void ProtocolPipeline::distributePackets(ParsedPacket* packets, size_t count, PacketSource source, uint8_t senderMask) {
    // Tag packets with source and distribute to matching senders
    for (size_t i = 0; i < count; i++) {
        packets[i].source = source;  // Tag packet with source
        
        // Send to senders based on mask
        for (size_t j = 0; j < MAX_SENDERS; j++) {
            if (senders[j] && (senderMask & (1 << j))) {
                senders[j]->enqueue(packets[i]);
            }
        }
    }
}

void ProtocolPipeline::handleBackpressure() {
    // Check if any sender is overwhelmed
    for (size_t i = 0; i < MAX_SENDERS; i++) {
        if (senders[i] && senders[i]->getQueueDepth() > 15) {
            log_msg(LOG_WARNING, "%s sender queue depth: %d",
                    senders[i]->getName(),
                    senders[i]->getQueueDepth());
        }
    }
}

void ProtocolPipeline::getStats(char* buffer, size_t bufSize) {
    size_t offset = 0;
    
    // Show active flows
    for (size_t i = 0; i < activeFlows; i++) {
        offset += snprintf(buffer + offset, bufSize - offset,
                          "Flow[%zu]: %s (%s)\\n", i, flows[i].name,
                          flows[i].parser ? flows[i].parser->getName() : "None");
    }
    
    // Show senders
    for (size_t i = 0; i < MAX_SENDERS; i++) {
        if (senders[i]) {
            offset += snprintf(buffer + offset, bufSize - offset,
                              "Sender[%zu]: %s: Sent=%u Dropped=%u Queue=%zu\\n",
                              i, senders[i]->getName(),
                              senders[i]->getSentCount(),
                              senders[i]->getDroppedCount(),
                              senders[i]->getQueueDepth());
        }
    }
}

void ProtocolPipeline::getStatsString(char* buffer, size_t bufSize) {
    size_t offset = 0;
    
    // Show active flows
    offset += snprintf(buffer + offset, bufSize - offset, "Flows: %zu ", activeFlows);
    
    // Show active senders
    size_t activeSenders = 0;
    for (size_t i = 0; i < MAX_SENDERS; i++) {
        if (senders[i]) {
            activeSenders++;
            offset += snprintf(buffer + offset, bufSize - offset,
                              "%s: Sent=%u Dropped=%u Queue=%zu Max=%zu ",
                              senders[i]->getName(),
                              senders[i]->getSentCount(),
                              senders[i]->getDroppedCount(),
                              senders[i]->getQueueDepth(),
                              senders[i]->getMaxQueueDepth());
        }
    }
    
    if (activeSenders == 0) {
        offset += snprintf(buffer + offset, bufSize - offset, "No active senders");
    }
}

void ProtocolPipeline::appendStatsToJson(JsonDocument& doc) {
    JsonObject stats = doc["protocolStats"].to<JsonObject>();
    
    // CRITICAL FIX: Check all pointers before access
    if (!ctx) {
        stats["error"] = "Pipeline context not initialized";
        log_msg(LOG_WARNING, "Pipeline: appendStatsToJson called with null context");
        return;
    }

    if (!ctx->system.config) {
        stats["error"] = "Configuration not available";
        log_msg(LOG_WARNING, "Pipeline: Config pointer is null in appendStatsToJson");
        return;
    }

    // Basic protocol information - now safe to access
    stats["protocolType"] = ctx->system.config->protocolOptimization;  // 0=RAW, 1=MAVLink, 2=SBUS
    
    // Show active flows
    JsonArray flowsArray = stats["flows"].to<JsonArray>();
    for (size_t i = 0; i < activeFlows; i++) {
        JsonObject flow = flowsArray.createNestedObject();
        flow["name"] = flows[i].name;
        flow["parser"] = flows[i].parser ? flows[i].parser->getName() : "None";
        flow["source"] = flows[i].source;
        flow["senderMask"] = flows[i].senderMask;
    }
    
    // Parser statistics (from first active flow)
    if (activeFlows > 0 && flows[0].parser && ctx->protocol.stats) {
        JsonObject parserStats = stats["parser"].to<JsonObject>();
        parserStats["bytesProcessed"] = ctx->protocol.stats->totalBytes;
        parserStats["packetsTransmitted"] = ctx->protocol.stats->packetsTransmitted;
        
        // Protocol-specific metrics
        switch(ctx->system.config->protocolOptimization) {
            case PROTOCOL_NONE:  // RAW (0)
                parserStats["chunksCreated"] = ctx->protocol.stats->packetsTransmitted;
                parserStats["bytesProcessed"] = ctx->protocol.stats->totalBytes;
                break;
                
            case PROTOCOL_MAVLINK:  // MAVLink (1)
                // Rename "packetsDetected" to "packetsParsed" for clarity
                parserStats["packetsParsed"] = ctx->protocol.stats->packetsDetected;
                
                // Get sent/dropped from USB sender (Device 2)
                uint32_t packetsSent = 0;
                uint32_t packetsDropped = 0;
                if (senders[IDX_DEVICE2_USB]) {
                    packetsSent = senders[IDX_DEVICE2_USB]->getSentCount();
                    packetsDropped = senders[IDX_DEVICE2_USB]->getDroppedCount();
                }
                parserStats["packetsSent"] = packetsSent;
                parserStats["packetsDropped"] = packetsDropped;
                
                // Detection errors from parser
                parserStats["detectionErrors"] = ctx->protocol.stats->detectionErrors;
                break;

            // Future: PROTOCOL_SBUS (2)
            // parserStats["framesDetected"] = ctx->protocol.stats->packetsDetected;
            // parserStats["framingErrors"] = ctx->protocol.stats->detectionErrors;
        }
        
        // Common metrics for all protocols
        parserStats["avgPacketSize"] = ctx->protocol.stats->avgPacketSize;
        parserStats["minPacketSize"] = (ctx->protocol.stats->minPacketSize == UINT32_MAX) ? 
                                       0 : ctx->protocol.stats->minPacketSize;
        parserStats["maxPacketSize"] = ctx->protocol.stats->maxPacketSize;
        
        // Calculate time since last activity
        unsigned long currentMillis = millis();
        unsigned long lastActivityMs = 0;
        if (ctx->protocol.stats->lastPacketTime > 0 && 
            currentMillis >= ctx->protocol.stats->lastPacketTime) {
            lastActivityMs = currentMillis - ctx->protocol.stats->lastPacketTime;
        }
        parserStats["lastActivityMs"] = lastActivityMs;
    } else {
        // Stats not available yet
        JsonObject parserStats = stats["parser"].to<JsonObject>();
        parserStats["info"] = "Statistics not yet initialized";
    }
    
    // Sender statistics - with null checks
    JsonArray sendersArray = stats["senders"].to<JsonArray>();
    for (size_t i = 0; i < MAX_SENDERS; i++) {
        if (senders[i]) {  // Check sender pointer
            JsonObject sender = sendersArray.createNestedObject();
            sender["name"] = senders[i]->getName();
            sender["index"] = i;  // Show fixed index
            sender["sent"] = senders[i]->getSentCount();
            sender["dropped"] = senders[i]->getDroppedCount();
            sender["queueDepth"] = senders[i]->getQueueDepth();
            sender["maxQueueDepth"] = senders[i]->getMaxQueueDepth();
        }
    }
    
    // Add UDP batching stats for UDP sender
    if (senders[IDX_DEVICE4]) {
        JsonObject udpStats = stats["udpBatching"].to<JsonObject>();
        ((UdpSender*)senders[IDX_DEVICE4])->getBatchingStats(udpStats);
    }
    
    // Buffer statistics - from first active flow
    if (activeFlows > 0 && flows[0].inputBuffer) {
        JsonObject buffer = stats["buffer"].to<JsonObject>();
        buffer["used"] = flows[0].inputBuffer->available();
        buffer["capacity"] = flows[0].inputBuffer->getCapacity();

        // Avoid division by zero
        size_t capacity = flows[0].inputBuffer->getCapacity();
        if (capacity > 0) {
            buffer["utilizationPercent"] = (flows[0].inputBuffer->available() * 100) / capacity;
        } else {
            buffer["utilizationPercent"] = 0;
        }
    }
}

// Access methods for compatibility
ProtocolParser* ProtocolPipeline::getParser() const {
    return (activeFlows > 0) ? flows[0].parser : nullptr;
}

CircularBuffer* ProtocolPipeline::getInputBuffer() const {
    return (activeFlows > 0) ? flows[0].inputBuffer : nullptr;
}

PacketSender* ProtocolPipeline::getSender(size_t index) const {
    return (index < MAX_SENDERS) ? senders[index] : nullptr;
}

// External component interfaces
void ProtocolPipeline::distributeParsedPackets(ParseResult* result) {
    if (result && result->count > 0) {
        // Default to telemetry source for external calls
        distributePackets(result->packets, result->count, SOURCE_TELEMETRY, SENDER_ALL);
    }
}

void ProtocolPipeline::processSenders() {
    // Calculate bulk mode from all active flows
    bool bulkMode = false;
    for (size_t i = 0; i < activeFlows; i++) {
        if (flows[i].parser && flows[i].parser->isBurstActive()) {
            bulkMode = true;
            break;
        }
    }
    
    for (size_t i = 0; i < MAX_SENDERS; i++) {
        if (senders[i]) {
            senders[i]->processSendQueue(bulkMode);
        }
    }
}

void ProtocolPipeline::cleanup() {
    // Delete all parsers
    for (size_t i = 0; i < activeFlows; i++) {
        if (flows[i].parser) {
            delete flows[i].parser;
            flows[i].parser = nullptr;
        }
    }
    activeFlows = 0;
    
    // Delete all senders
    for (size_t i = 0; i < MAX_SENDERS; i++) {
        if (senders[i]) {
            delete senders[i];
            senders[i] = nullptr;
        }
    }
}