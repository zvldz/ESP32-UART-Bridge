#include "protocol_pipeline.h"
#include "../logging.h"
#include "uart1_sender.h"
#include "../uart/uart1_tx_service.h"

ProtocolPipeline::ProtocolPipeline(BridgeContext* context) : 
    activeFlows(0),
    ctx(context),
    sharedRouter(nullptr) {
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
    
    // Create shared router if MAVLink routing enabled
    if (config->protocolOptimization == PROTOCOL_MAVLINK && 
        config->mavlinkRouting) {
        sharedRouter = new MavlinkRouter();
        log_msg(LOG_INFO, "Shared MAVLink router created");
    }
    
    // Setup data flows based on configuration
    setupFlows(config);
    
    // Create senders based on configuration
    createSenders(config);
    
    log_msg(LOG_INFO, "Protocol pipeline initialized: %zu flows, senders fixed slots=%d", activeFlows, MAX_SENDERS);
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
    
    // EXISTING Telemetry flow - update to use shared router
    if (telemetryMask && ctx->buffers.telemetryBuffer) {
        DataFlow f;
        f.name = "Telemetry";
        f.inputBuffer = ctx->buffers.telemetryBuffer;
        f.source = SOURCE_TELEMETRY;
        f.physInterface = PHYS_UART1;  // Telemetry comes from FC
        f.senderMask = telemetryMask;
        f.isInputFlow = false;  // FC→devices flow
        
        // Create parser based on protocol type
        switch (config->protocolOptimization) {
            case PROTOCOL_NONE:
                f.parser = new RawParser();
                f.router = nullptr;
                break;
            case PROTOCOL_MAVLINK:
                {
                    MavlinkParser* mavParser = new MavlinkParser();
                    mavParser->setRoutingEnabled(config->mavlinkRouting);
                    f.parser = mavParser;
                    log_msg(LOG_INFO, "MAVLink parser created with routing=%s", 
                            config->mavlinkRouting ? "enabled" : "disabled");
                }
                f.router = sharedRouter;  // CRITICAL: Use shared router, NOT new!
                break;
            default:
                f.parser = new RawParser();
                f.router = nullptr;
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
    
    // NEW: USB Input flow (USB → UART1)
    if (config->device2.role == D2_USB && ctx->buffers.usbInputBuffer) {
        DataFlow f;
        f.name = "USB_Input";
        f.inputBuffer = ctx->buffers.usbInputBuffer;
        f.source = SOURCE_TELEMETRY;  // TODO: Migration - create SOURCE_INPUT later
        f.physInterface = PHYS_USB;   // Physical: from USB
        f.senderMask = (1 << IDX_UART1);  // Only to UART1
        f.isInputFlow = true;  // device→FC flow
        
        // Create parser based on protocol
        switch (config->protocolOptimization) {
            case PROTOCOL_NONE:
                f.parser = new RawParser();
                f.router = nullptr;
                break;
            case PROTOCOL_MAVLINK:
                {
                    MavlinkParser* mavParser = new MavlinkParser();
                    mavParser->setRoutingEnabled(config->mavlinkRouting);
                    f.parser = mavParser;
                }
                f.router = sharedRouter;  // Use shared router
                break;
            default:
                f.parser = new RawParser();
                f.router = nullptr;
                break;
        }
        
        flows[activeFlows++] = f;
    }
    
    // NEW: UDP Input flow (UDP → UART1)
    if (config->device4.role == D4_NETWORK_BRIDGE && ctx->buffers.udpInputBuffer) {
        DataFlow f;
        f.name = "UDP_Input";
        f.inputBuffer = ctx->buffers.udpInputBuffer;
        f.source = SOURCE_TELEMETRY;  // TODO: Migration - create SOURCE_INPUT later
        f.physInterface = PHYS_UDP;   // Physical: from UDP
        f.senderMask = (1 << IDX_UART1);  // Only to UART1
        f.isInputFlow = true;  // device→FC flow
        
        // Create parser based on protocol
        switch (config->protocolOptimization) {
            case PROTOCOL_NONE:
                f.parser = new RawParser();
                f.router = nullptr;
                break;
            case PROTOCOL_MAVLINK:
                {
                    MavlinkParser* mavParser = new MavlinkParser();
                    mavParser->setRoutingEnabled(config->mavlinkRouting);
                    f.parser = mavParser;
                }
                f.router = sharedRouter;  // Use shared router
                break;
            default:
                f.parser = new RawParser();
                f.router = nullptr;
                break;
        }
        
        flows[activeFlows++] = f;
    }
    
    // NEW: UART2 Input flow (UART2 → UART1)
    if (config->device2.role == D2_UART2 && ctx->buffers.uart2InputBuffer) {
        DataFlow f;
        f.name = "UART2_Input";
        f.inputBuffer = ctx->buffers.uart2InputBuffer;
        f.source = SOURCE_TELEMETRY;  // TODO: Migration - create SOURCE_INPUT later
        f.physInterface = PHYS_UART2;  // Physical: from UART2
        f.senderMask = (1 << IDX_UART1);  // Only to UART1
        f.isInputFlow = true;  // device→FC flow
        
        // Create parser based on protocol
        switch (config->protocolOptimization) {
            case PROTOCOL_NONE:
                f.parser = new RawParser();
                f.router = nullptr;
                break;
            case PROTOCOL_MAVLINK:
                {
                    MavlinkParser* mavParser = new MavlinkParser();
                    mavParser->setRoutingEnabled(config->mavlinkRouting);
                    f.parser = mavParser;
                }
                f.router = sharedRouter;  // Use shared router
                break;
            default:
                f.parser = new RawParser();
                f.router = nullptr;
                break;
        }
        
        flows[activeFlows++] = f;
    }
    
    // NEW: UART3 Input flow (UART3 → UART1)
    if (config->device3.role == D3_UART3_BRIDGE && ctx->buffers.uart3InputBuffer) {
        DataFlow f;
        f.name = "UART3_Input";
        f.inputBuffer = ctx->buffers.uart3InputBuffer;
        f.source = SOURCE_TELEMETRY;  // TODO: Migration - create SOURCE_INPUT later
        f.physInterface = PHYS_UART3;  // Physical: from UART3
        f.senderMask = (1 << IDX_UART1);  // Only to UART1
        f.isInputFlow = true;  // device→FC flow
        
        // Create parser based on protocol
        switch (config->protocolOptimization) {
            case PROTOCOL_NONE:
                f.parser = new RawParser();
                f.router = nullptr;
                break;
            case PROTOCOL_MAVLINK:
                {
                    MavlinkParser* mavParser = new MavlinkParser();
                    mavParser->setRoutingEnabled(config->mavlinkRouting);
                    f.parser = mavParser;
                }
                f.router = sharedRouter;  // Use shared router
                break;
            default:
                f.parser = new RawParser();
                f.router = nullptr;
                break;
        }
        
        flows[activeFlows++] = f;
    }
    
    // RAW mode check - only one input allowed
    if (config->protocolOptimization == PROTOCOL_NONE) {
        int inputFlowCount = 0;
        for (size_t i = 0; i < activeFlows; i++) {
            if (flows[i].isInputFlow) {  // Use explicit flag instead of senderMask check
                inputFlowCount++;
            }
        }
        if (inputFlowCount > 1) {
            log_msg(LOG_ERROR, "RAW mode supports only single input! Disabling extra flows.");
            // Keep only first input flow
            // TODO: Better strategy - disable all but primary input
            size_t firstInputIdx = 0;
            for (size_t i = 0; i < activeFlows; i++) {
                if (flows[i].isInputFlow) {
                    if (firstInputIdx == 0) {
                        firstInputIdx = i;
                    } else {
                        // Disable this flow
                        flows[i].isInputFlow = false;
                        flows[i].senderMask = 0;
                    }
                }
            }
        }
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
    
    // UART1 sender (NEW)
    extern UartInterface* uartBridgeSerial;
    if (uartBridgeSerial) {
        // Initialize TX service with correct buffer size
        Uart1TxService::getInstance()->init(uartBridgeSerial, UART1_TX_RING_SIZE);
        
        // Create sender that uses TX service
        senders[IDX_UART1] = new Uart1Sender();
        log_msg(LOG_INFO, "Created UART1 sender at index %d", IDX_UART1);
    } else {
        log_msg(LOG_WARNING, "UART1 sender not created - uartBridgeSerial is NULL");
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

void ProtocolPipeline::processInputFlows() {
    // CRITICAL: Add time limit to prevent blocking main loop during heavy traffic
    uint32_t startMs = millis();
    const uint32_t MAX_PROCESSING_TIME_MS = 5;  // Max 5ms per call
    bool timeExceeded = false;
    
    for (size_t i = 0; i < activeFlows; i++) {
        if (flows[i].isInputFlow) {  // Use explicit flag
            // Check time before processing each flow
            if (millis() - startMs >= MAX_PROCESSING_TIME_MS) {
                timeExceeded = true;
                break;
            }
            
            processFlow(flows[i]);
        }
    }
    
    // === TEMPORARY DIAGNOSTIC START ===
    static uint32_t exceedCount = 0;
    if (timeExceeded && ++exceedCount % 100 == 0) {
        log_msg(LOG_DEBUG, "[INPUT] Processing time limit exceeded %u times", exceedCount);
    }
    // === TEMPORARY DIAGNOSTIC END ===
}

void ProtocolPipeline::processTelemetryFlow() {
    // === TEMPORARY DIAGNOSTIC BLOCK START ===
    static uint32_t packetCount = 0;
    static uint32_t lastReport = 0;
    static uint32_t exhaustiveIterations = 0;  // Track how many parse iterations
    // === TEMPORARY DIAGNOSTIC BLOCK END ===
    
    // === TEMPORARY DIAGNOSTIC BLOCK START ===
    // Count function call frequency
    static uint32_t callCount = 0;
    static uint32_t lastCallReport = 0;
    callCount++;
    
    if (millis() - lastCallReport > 1000) {
        log_msg(LOG_INFO, "[FLOW] processTelemetryFlow called %u times/sec", callCount);
        callCount = 0;
        lastCallReport = millis();
    }
    // === TEMPORARY DIAGNOSTIC BLOCK END ===
    
    // Process telemetry with exhaustive parsing for efficiency
    const uint32_t MAX_TIME_MS = 10;  // Max 10ms for telemetry processing
    const size_t MAX_ITERATIONS = 20; // Safety limit to prevent infinite loops
    uint32_t startTime = millis();
    
    for (size_t i = 0; i < activeFlows; i++) {
        if (!flows[i].isInputFlow) {  // Telemetry flow (FC→GCS)
            
            // Process this flow until empty or timeout
            size_t iterations = 0;
            while (flows[i].inputBuffer && 
                   flows[i].inputBuffer->available() > 0 &&
                   (millis() - startTime) < MAX_TIME_MS &&
                   iterations < MAX_ITERATIONS) {
                
                // === TEMPORARY DIAGNOSTIC BLOCK START ===
                size_t beforePackets = packetCount;
                // === TEMPORARY DIAGNOSTIC BLOCK END ===
                
                // Remember buffer state before processing
                size_t availableBefore = flows[i].inputBuffer->available();
                
                // Process one chunk (up to 296 bytes for MAVLink)
                processFlow(flows[i]);
                
                // Check if parser made progress
                size_t availableAfter = flows[i].inputBuffer->available();
                if (availableAfter >= availableBefore) {
                    break;
                }
                
                iterations++;
                
                // === TEMPORARY DIAGNOSTIC BLOCK START ===
                exhaustiveIterations++;
                // Count packets processed (approximation)
                if (availableAfter > 0) {
                    packetCount++;
                }
                // === TEMPORARY DIAGNOSTIC BLOCK END ===
            }
            
            // Log if we hit limits (for debugging)
            if (iterations >= MAX_ITERATIONS) {
                log_msg(LOG_WARNING, "Telemetry processing hit iteration limit (%zu)", iterations);
            } else if ((millis() - startTime) >= MAX_TIME_MS) {
                log_msg(LOG_DEBUG, "Telemetry processing hit time limit after %zu iterations", iterations);
            }
        }
    }
    
    // === TEMPORARY DIAGNOSTIC BLOCK START ===
    if (millis() - lastReport > 1000) {
        log_msg(LOG_INFO, "Telemetry: %u packets/sec, %u parse iterations/sec", 
                packetCount, exhaustiveIterations);
        packetCount = 0;
        exhaustiveIterations = 0;
        lastReport = millis();
    }
    // === TEMPORARY DIAGNOSTIC BLOCK END ===
}

void ProtocolPipeline::processFlow(DataFlow& flow) {
    if (!flow.parser || !flow.inputBuffer) return;
    
    // === TEMPORARY DIAGNOSTIC BLOCK START ===
    static uint32_t telemetryBytesTotal = 0;
    static uint32_t parsedPacketsTotal = 0;
    static uint32_t lastFlowReport = 0;
    
    // Count bytes available in telemetry buffer before parsing
    size_t available = flow.inputBuffer->available();
    telemetryBytesTotal += available;
    // === TEMPORARY DIAGNOSTIC BLOCK END ===
    
    uint32_t now = micros();
    
    // Parse packets from input buffer
    ParseResult result = flow.parser->parse(flow.inputBuffer, now);
    
    // === TEMPORARY DIAGNOSTIC BLOCK START ===
    // Count successfully parsed packets
    parsedPacketsTotal += result.count;
    
    // Log flow statistics every second
    if (millis() - lastFlowReport > 1000) {
        log_msg(LOG_INFO, "Flow stats: Processed %u bytes/sec, Parsed %u packets/sec", 
                telemetryBytesTotal, parsedPacketsTotal);
        telemetryBytesTotal = 0;
        parsedPacketsTotal = 0;
        lastFlowReport = millis();
    }
    // === TEMPORARY DIAGNOSTIC BLOCK END ===
    
    // === TEMPORARY DIAGNOSTIC BLOCK START ===
    // Detailed telemetry parser diagnostics
    if (strcmp(flow.name, "Telemetry") == 0 && result.bytesConsumed > 0) {
        static uint32_t totalConsumed = 0;
        static uint32_t totalPackets = 0;
        static uint32_t lastParseReport = 0;
        
        totalConsumed += result.bytesConsumed;
        totalPackets += result.count;
        
        if (millis() - lastParseReport > 1000) {
            log_msg(LOG_INFO, "[PARSE] Telemetry: consumed %u bytes, parsed %u packets/sec", 
                    totalConsumed, totalPackets);
            totalConsumed = 0;
            totalPackets = 0;
            lastParseReport = millis();
        }
    }
    // === TEMPORARY DIAGNOSTIC BLOCK END ===
    
    // Consume bytes if parser processed them
    if (result.bytesConsumed > 0) {
        flow.inputBuffer->consume(result.bytesConsumed);
    }
    
    // Set physical interface for anti-echo
    for (size_t i = 0; i < result.count; i++) {
        result.packets[i].physicalInterface = flow.physInterface;
    }
    
    // Apply routing if configured
    if (flow.router && result.count > 0) {
        flow.router->process(result.packets, result.count);
    }
    
    // Distribute packets
    if (result.count > 0) {
        distributePackets(result.packets, result.count, flow.source, flow.senderMask);
    }
    
    // Clean up parse result
    result.free();
}

void ProtocolPipeline::distributePackets(ParsedPacket* packets, size_t count, PacketSource source, uint8_t senderMask) {
    for (size_t i = 0; i < count; i++) {
        packets[i].source = source;
        
        // Get physical interface (with validation)
        PhysicalInterface phys = static_cast<PhysicalInterface>(packets[i].physicalInterface);
        
        // Determine final destination mask
        uint8_t finalMask;
        if (packets[i].hints.hasExplicitTarget) {
            // Router determined specific target
            finalMask = packets[i].hints.targetDevices;
        } else if (phys == PHYS_NONE) {
            // Internal source (no physical interface) - send to all
            finalMask = senderMask;
        } else if (isValidPhysicalInterface(phys)) {
            // Apply anti-echo using physical interface
            finalMask = senderMask & ~physicalInterfaceBit(phys);
        } else {
            // Invalid physical interface - log and broadcast
            log_msg(LOG_WARNING, "Invalid physical interface %d, broadcasting", phys);
            finalMask = senderMask;
        }
        
        // Send to selected interfaces
        for (size_t j = 0; j < MAX_SENDERS; j++) {
            if (senders[j] && (finalMask & (1 << j))) {
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
    
    // Add router statistics if MAVLink routing active
    // Find MAVLink flow (don't assume it's at index 0)
    for (size_t i = 0; i < activeFlows; i++) {
        if (flows[i].router) {
            JsonObject routerStats = stats["router"].to<JsonObject>();
            uint32_t hits = 0, broadcasts = 0;
            flows[i].router->getStats(hits, broadcasts);
            routerStats["unicast_hits"] = hits;
            routerStats["broadcasts"] = broadcasts;
            routerStats["enabled"] = true;
            break;  // Only one router expected
        }
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
    // Delete all parsers and routers
    for (size_t i = 0; i < activeFlows; i++) {
        if (flows[i].parser) {
            delete flows[i].parser;
            flows[i].parser = nullptr;
        }
        if (flows[i].router) {
            delete flows[i].router;
            flows[i].router = nullptr;
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

