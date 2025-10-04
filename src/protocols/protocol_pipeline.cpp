#include "protocol_pipeline.h"
#include "../logging.h"
#include "uart1_sender.h"
#include "../uart/uart1_tx_service.h"
#include "sbus_fast_parser.h"

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

    // Device1 SBUS_IN flow (CRITICAL - new feature!)
    if (config->device1.role == D1_SBUS_IN && ctx->buffers.telemetryBuffer) {
        DataFlow f;
        f.name = "Device1_SBUS_IN";
        f.inputBuffer = ctx->buffers.telemetryBuffer;  // Device1 uses telemetryBuffer
        f.physInterface = PHYS_UART1;
        f.source = SOURCE_TELEMETRY;
        f.senderMask = calculateSbusInputRouting(config);
        f.isInputFlow = true;
        f.parser = new SbusFastParser(SBUS_SOURCE_DEVICE1);  // Device1 SBUS_IN
        f.router = nullptr;


        flows[activeFlows++] = f;
        log_msg(LOG_INFO, "Device1 SBUS_IN flow created - FIRST TIME Device1 is not UART bridge!");
    }

    // Device2 SBUS_IN flow
    if (config->device2.role == D2_SBUS_IN && ctx->buffers.uart2InputBuffer) {
        DataFlow f;
        f.name = "Device2_SBUS_IN";
        f.inputBuffer = ctx->buffers.uart2InputBuffer;  // Device2 uses uart2InputBuffer
        f.physInterface = PHYS_UART2;
        f.source = SOURCE_TELEMETRY;
        f.senderMask = calculateSbusInputRouting(config);
        f.isInputFlow = true;
        f.parser = new SbusFastParser(SBUS_SOURCE_DEVICE2);  // Device2 SBUS_IN
        f.router = nullptr;

        flows[activeFlows++] = f;
        log_msg(LOG_INFO, "Device2 SBUS_IN flow created");
    }

    // Check if any SBUS device is configured
    bool hasSbusDevice = (config->device1.role == D1_SBUS_IN ||
                          config->device2.role == D2_SBUS_IN ||
                          config->device2.role == D2_SBUS_OUT ||
                          config->device3.role == D3_SBUS_OUT);
    
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
    
    // EXISTING Telemetry flow - only for D1_UART1 mode
    // D1_SBUS_IN has its own dedicated flow above
    if (telemetryMask && ctx->buffers.telemetryBuffer && config->device1.role != D1_SBUS_IN) {
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
                    MavlinkParser* mavParser = new MavlinkParser(0);  // Telemetry channel
                    mavParser->setRoutingEnabled(config->mavlinkRouting);
                    f.parser = mavParser;
                    log_msg(LOG_INFO, "MAVLink parser created for Telemetry flow (channel=0) with routing=%s", 
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
                    MavlinkParser* mavParser = new MavlinkParser(1);  // USB Input channel
                    mavParser->setRoutingEnabled(config->mavlinkRouting);
                    f.parser = mavParser;
                    log_msg(LOG_INFO, "MAVLink parser created for USB_Input flow (channel=1)");
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
    if (config->device4.role == D4_NETWORK_BRIDGE && ctx->buffers.udpInputBuffer &&
        !hasSbusDevice) {
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
                    MavlinkParser* mavParser = new MavlinkParser(2);  // UDP Input channel
                    mavParser->setRoutingEnabled(config->mavlinkRouting);
                    f.parser = mavParser;
                    log_msg(LOG_INFO, "MAVLink parser created for UDP_Input flow (channel=2)");
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
    if (config->device2.role == D2_UART2 && ctx->buffers.uart2InputBuffer &&
        !hasSbusDevice) {
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
                    MavlinkParser* mavParser = new MavlinkParser(3);  // UART2 Input channel
                    mavParser->setRoutingEnabled(config->mavlinkRouting);
                    f.parser = mavParser;
                    log_msg(LOG_INFO, "MAVLink parser created for UART2_Input flow (channel=3)");
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
    if (config->device3.role == D3_UART3_BRIDGE && ctx->buffers.uart3InputBuffer &&
        !hasSbusDevice) {
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
                    MavlinkParser* mavParser = new MavlinkParser(4);  // UART3 Input channel
                    mavParser->setRoutingEnabled(config->mavlinkRouting);
                    f.parser = mavParser;
                    log_msg(LOG_INFO, "MAVLink parser created for UART3_Input flow (channel=4)");
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
    
    
    // SBUS Input flow (SBUS → UART1)
    if (config->device2.role == D2_SBUS_IN && ctx->buffers.uart2InputBuffer) {
        
        DataFlow f;
        f.name = "SBUS_Input_D2";
        f.inputBuffer = ctx->buffers.uart2InputBuffer;
        f.physInterface = PHYS_UART2;
        
        f.source = SOURCE_TELEMETRY;
        f.senderMask = calculateSbusInputRouting(config);  // Dynamic routing calculation
        f.isInputFlow = true;
        f.parser = new SbusFastParser(SBUS_SOURCE_DEVICE2);  // Device2 SBUS_IN
        f.router = nullptr;  // No routing for SBUS
        
        flows[activeFlows++] = f;
        log_msg(LOG_INFO, "SBUS Input flow created with routing mask: 0x%02X", f.senderMask);
    }

    // SBUS Output flow (UART1 → SBUS)
    // SBUS_OUTPUT via legacy pipeline (only when no Fast Path SBUS input)
    // Fast Path handles SBUS_OUT directly, legacy path needs telemetryBuffer
    bool hasLegacySbusOut = (config->device2.role == D2_SBUS_OUT || config->device3.role == D3_SBUS_OUT);
    bool hasFastPathSbusIn = (config->device1.role == D1_SBUS_IN || config->device2.role == D2_SBUS_IN);

    if (hasLegacySbusOut && !hasFastPathSbusIn && ctx->buffers.telemetryBuffer) {
        
        DataFlow f;
        f.name = "SBUS_Output";
        f.inputBuffer = ctx->buffers.telemetryBuffer;  // From UART1
        f.source = SOURCE_TELEMETRY;
        f.physInterface = PHYS_UART1;
        f.isInputFlow = false;
        f.parser = new SbusFastParser(SBUS_SOURCE_DEVICE1);
        f.router = nullptr;
        
        // Select output device
        if (config->device2.role == D2_SBUS_OUT) {
            f.senderMask = (1 << IDX_DEVICE2_UART2);
        } else {
            f.senderMask = (1 << IDX_DEVICE3);
        }
        
        flows[activeFlows++] = f;
        log_msg(LOG_INFO, "SBUS Output flow created");
    }

    // TODO: UART→SBUS conversion not implemented in Phase 2
    // Legacy SBUS Hub architecture removed - need new implementation
    // Planned combinations:
    //   - D2_UART2 + D3_SBUS_OUT (UART2 transport → SBUS OUT)
    //   - D3_UART3_BRIDGE + D2_SBUS_OUT (UART3 transport → SBUS OUT)
    // Requires: Parser to extract SBUS frames from UART stream
    //           Integration with SbusRouter singleton

    // Context-aware Device4 handling
    // Use the hasSbusDevice variable already defined above

    if (hasSbusDevice && config->device4.role == D4_NETWORK_BRIDGE) {
        // Split into separate TX/RX for SBUS
        log_msg(LOG_WARNING, "SBUS detected - Device4 should use SBUS_UDP_TX or SBUS_UDP_RX");
    }

    // Handle SBUS UDP RX (UDP → SBUS)
    if (config->device4.role == D4_SBUS_UDP_RX &&
        (config->device2.role == D2_SBUS_OUT || config->device3.role == D3_SBUS_OUT) &&
        ctx->buffers.udpInputBuffer) {

        DataFlow f;
        f.name = "UDP_SBUS_Input";
        f.inputBuffer = ctx->buffers.udpInputBuffer;
        f.source = SOURCE_TELEMETRY;
        f.physInterface = PHYS_UDP;
        f.isInputFlow = true;
        f.parser = new SbusFastParser(SBUS_SOURCE_UDP);  // UDP source
        f.router = nullptr;
        f.senderMask = 0;  // No senders - routing via SbusRouter in parser

        flows[activeFlows++] = f;
        log_msg(LOG_INFO, "UDP->SBUS input flow created (routing via SbusRouter)");
    }

    // Handle SBUS UDP TX (SBUS → UDP)
    // D4_SBUS_UDP_TX: Read from appropriate SBUS input buffer
    if (config->device4.role == D4_SBUS_UDP_TX) {
        CircularBuffer* sbusInputBuffer = nullptr;
        const char* sourceDesc = "Unknown";

        if (config->device1.role == D1_SBUS_IN && ctx->buffers.telemetryBuffer) {
            sbusInputBuffer = ctx->buffers.telemetryBuffer;
            sourceDesc = "Device1_SBUS";
        } else if (config->device2.role == D2_SBUS_IN && ctx->buffers.uart2InputBuffer) {
            sbusInputBuffer = ctx->buffers.uart2InputBuffer;
            sourceDesc = "Device2_SBUS";
        }

        if (sbusInputBuffer) {
            DataFlow f;
            f.name = "SBUS_UDP_Output";
            f.inputBuffer = sbusInputBuffer;  // From SBUS input buffer
            f.source = SOURCE_TELEMETRY;
            f.physInterface = PHYS_UART1;  // SBUS source
        f.isInputFlow = false;  // Output flow
        f.parser = new SbusFastParser(SBUS_SOURCE_DEVICE1);  // Process for UDP send
        f.router = nullptr;
        f.senderMask = (1 << IDX_DEVICE4);  // To UDP sender

        flows[activeFlows++] = f;
        log_msg(LOG_INFO, "SBUS->UDP output flow created");
        }
    }
    
    // TODO Phase 8: Remove when multi-source support implemented
    // Check for conflicting SBUS inputs (only one source allowed currently)
    int sbusSourceCount = 0;
    const char* sources[4] = {nullptr};
    int sourceIdx = 0;

    // Check physical SBUS inputs
    if (config->device2.role == D2_SBUS_IN) {
        sources[sourceIdx++] = "Device2 SBUS_IN";
        sbusSourceCount++;
    }
    // D3_SBUS_IN removed - no longer supported

    // Check UART→SBUS bridges
    if (config->device2.role == D2_UART2 && config->device3.role == D3_SBUS_OUT) {
        sources[sourceIdx++] = "UART2→SBUS bridge";
        sbusSourceCount++;
    }
    if (config->device3.role == D3_UART3_BRIDGE && config->device2.role == D2_SBUS_OUT) {
        sources[sourceIdx++] = "UART3→SBUS bridge";
        sbusSourceCount++;
    }

    // Check UDP→SBUS input
    if (config->device4.role == D4_NETWORK_BRIDGE && 
        (config->device2.role == D2_SBUS_OUT || config->device3.role == D3_SBUS_OUT)) {
        sources[sourceIdx++] = "UDP→SBUS input";
        sbusSourceCount++;
    }

    if (sbusSourceCount > 1) {
        log_msg(LOG_ERROR, "SBUS: Multiple input sources detected (%d):", sbusSourceCount);
        for (int i = 0; i < sourceIdx; i++) {
            if (sources[i]) log_msg(LOG_ERROR, "  - %s", sources[i]);
        }
        log_msg(LOG_INFO, "Only one SBUS source supported currently (Phase 8 will add multi-source)");
    }
}

uint8_t ProtocolPipeline::calculateSbusInputRouting(Config* config) {
    uint8_t mask = 0;
    
    // Device1 (UART1) always receives SBUS data for SBUS→UART conversion
    // This allows transporting SBUS over regular UART at different baudrates
    mask |= (1 << IDX_UART1);
    
    // Device2 routing based on its role
    if (config->device2.role == D2_UART2) {
        // D3_SBUS_IN no longer supported
    } else if (config->device2.role == D2_SBUS_OUT) {
        // D3_SBUS_IN no longer supported
    }
    
    // Device3 routing based on its role
    if (config->device3.role == D3_UART3_BRIDGE || 
        config->device3.role == D3_UART3_MIRROR) {
        // Device3 is regular UART - send SBUS frames if input from Device2
        if (config->device2.role == D2_SBUS_IN) {
            mask |= (1 << IDX_DEVICE3);
            log_msg(LOG_INFO, "SBUS routing: D2_SBUS_IN -> D3_UART enabled");
        }
    } else if (config->device3.role == D3_SBUS_OUT) {
        // Device3 is SBUS_OUT repeater - send if input from Device2
        if (config->device2.role == D2_SBUS_IN) {
            mask |= (1 << IDX_DEVICE3);
            log_msg(LOG_INFO, "SBUS routing: D2_SBUS_IN -> D3_SBUS_OUT enabled");
        }
    }
    
    // Device4 for network transport
    if (config->device4.role == D4_NETWORK_BRIDGE) {
        mask |= (1 << IDX_DEVICE4);
        log_msg(LOG_INFO, "SBUS routing: SBUS_IN -> UDP enabled");
    }
    
    // Log final routing configuration
    log_msg(LOG_INFO, "SBUS routing mask: 0x%02X (UART1=%d D2=%d D3=%d D4=%d)",
            mask,
            (mask & (1 << IDX_UART1)) ? 1 : 0,
            (mask & (1 << IDX_DEVICE2_UART2)) ? 1 : 0,
            (mask & (1 << IDX_DEVICE3)) ? 1 : 0,
            (mask & (1 << IDX_DEVICE4)) ? 1 : 0);
    
    return mask;
}

void ProtocolPipeline::createSenders(Config* config) {
    // Initialize all slots to nullptr first
    for (size_t i = 0; i < MAX_SENDERS; i++) {
        senders[i] = nullptr;
    }
    
    // Device2 - USB, UART2, or SBUS_OUT (mutually exclusive)
    if (config->device2.role == D2_USB && ctx->interfaces.usbInterface) {
        senders[IDX_DEVICE2_USB] = new UsbSender(
            ctx->interfaces.usbInterface
        );
        log_msg(LOG_INFO, "Created USB sender at index %d", IDX_DEVICE2_USB);
    }
    else if ((config->device2.role == D2_UART2 || config->device2.role == D2_SBUS_OUT) &&
             ctx->interfaces.device2Serial) {
        senders[IDX_DEVICE2_UART2] = new Uart2Sender(
            ctx->interfaces.device2Serial
        );
        log_msg(LOG_INFO, "Created UART2 sender at index %d for role %d", IDX_DEVICE2_UART2, config->device2.role);
    }
    
    // Device3 - UART3 (for MIRROR, BRIDGE, and SBUS_OUT modes)
    if ((config->device3.role == D3_UART3_MIRROR ||
         config->device3.role == D3_UART3_BRIDGE ||
         config->device3.role == D3_SBUS_OUT) &&
        ctx->interfaces.device3Serial) {
        senders[IDX_DEVICE3] = new Uart3Sender(
            ctx->interfaces.device3Serial
        );
        log_msg(LOG_INFO, "Created UART3 sender at index %d for role %d", IDX_DEVICE3, config->device3.role);
    }
    
    // Device4 - UDP sender (for all UDP TX roles)
    if ((config->device4.role == D4_NETWORK_BRIDGE ||
         config->device4.role == D4_LOG_NETWORK ||
         config->device4.role == D4_SBUS_UDP_TX)) {
        extern AsyncUDP* udpTransport;
        if (udpTransport) {
            UdpSender* udpSender = new UdpSender(
                udpTransport
            );
            udpSender->setBatchingEnabled(config->udpBatchingEnabled);
            senders[IDX_DEVICE4] = udpSender;
            log_msg(LOG_INFO, "Created UDP sender at index %d for role %d", IDX_DEVICE4, config->device4.role);
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

// LEGACY: Old unified process() method - no longer used
// TODO: Remove after verification that processInputFlows() + processSendQueues() split works correctly
/*
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
*/

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

    // Process telemetry with exhaustive parsing for efficiency (MAVLink/SBUS only)
    const uint32_t MAX_TIME_MS = 10;  // Max 10ms for telemetry processing
    const size_t MAX_ITERATIONS = 20; // Safety limit to prevent infinite loops
    uint32_t startTime = millis();

    for (size_t i = 0; i < activeFlows; i++) {
        if (!flows[i].isInputFlow && flows[i].source != SOURCE_LOGS) {  // Telemetry flow (FC→GCS), exclude Logger

            // RAW parser uses timeout-based logic, incompatible with exhaustive loop
            // Exhaustive loop calls parse() too fast (< 1ms) preventing timeouts from triggering
            if (flows[i].parser && strcmp(flows[i].parser->getName(), "RAW") == 0) {
                // RAW parser: single iteration only
                processFlow(flows[i]);
            } else {
                // MAVLink/SBUS: exhaustive loop (packet/frame-based parsers)
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

    // Process Logger flows separately (not part of telemetry)
    for (size_t i = 0; i < activeFlows; i++) {
        if (!flows[i].isInputFlow && flows[i].source == SOURCE_LOGS) {
            processFlow(flows[i]);
        }
    }
}

void ProtocolPipeline::processFlow(DataFlow& flow) {
    if (!flow.parser || !flow.inputBuffer) return;

    // NEW: Try fast path first
    if (flow.parser->tryFastProcess(flow.inputBuffer, ctx)) {
        return;  // Processed via fast path, skip normal parsing
    }

    // === TEMPORARY DIAGNOSTIC BLOCK START ===
    static uint32_t telemetryBytesTotal = 0;
    static uint32_t parsedPacketsTotal = 0;
    static uint32_t lastFlowReport = 0;

    // Count bytes available in telemetry buffer before parsing
    size_t available = flow.inputBuffer->available();
    telemetryBytesTotal += available;

    // === TEMPORARY DIAGNOSTIC BLOCK END ===

    uint32_t nowMicros = micros();
    uint32_t nowMillis = millis();

    // Parse packets from input buffer (pass millis for lastPacketTime tracking)
    ParseResult result = flow.parser->parse(flow.inputBuffer, nowMillis);
    
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
                
            case PROTOCOL_MAVLINK: {  // MAVLink (1)
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
            }

            case PROTOCOL_SBUS: {  // SBUS (2)
                // SBUS specific statistics
                parserStats["framesDetected"] = ctx->protocol.stats->packetsDetected;
                parserStats["framingErrors"] = ctx->protocol.stats->detectionErrors;
                
                // Find SBUS parser in any flow (not just first)
                for (size_t i = 0; i < activeFlows; i++) {
                    if (flows[i].parser && 
                        strcmp(flows[i].parser->getName(), "SBUS_Fast") == 0) {
                        
                        // Cast to SbusFastParser to get specific stats
                        SbusFastParser* sbusParser = static_cast<SbusFastParser*>(flows[i].parser);
                        parserStats["validFrames"] = sbusParser->getValidFrames();
                        parserStats["invalidFrames"] = sbusParser->getInvalidFrames();
                        // Fast Path doesn't need complex legacy metrics
                        break;  // Found it, stop searching
                    }
                }
                break;
            }
        }
        
        // Common metrics for all protocols
        parserStats["avgPacketSize"] = ctx->protocol.stats->avgPacketSize;
        parserStats["minPacketSize"] = (ctx->protocol.stats->minPacketSize == UINT32_MAX) ? 
                                       0 : ctx->protocol.stats->minPacketSize;
        parserStats["maxPacketSize"] = ctx->protocol.stats->maxPacketSize;
        
        // Calculate time since last activity
        unsigned long currentMillis = millis();
        long lastActivityMs = -1;  // -1 means never had packets
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
            // Skip Device1 (UART1) when it's in SBUS_IN mode - it's an INPUT, not output
            if (i == IDX_UART1 && ctx->system.config->device1.role == D1_SBUS_IN) {
                continue;  // Don't show Device1 SBUS_IN in Output Devices
            }

            JsonObject sender = sendersArray.createNestedObject();
            sender["name"] = senders[i]->getName();
            sender["index"] = i;  // Show fixed index
            sender["sent"] = senders[i]->getSentCount();
            sender["dropped"] = senders[i]->getDroppedCount();
            sender["queueDepth"] = senders[i]->getQueueDepth();
            sender["maxQueueDepth"] = senders[i]->getMaxQueueDepth();
        }
    }

    // Add SBUS output devices as virtual senders (for Output Devices display)
    if (ctx->system.config) {
        // Device2 SBUS_OUT
        if (ctx->system.config->device2.role == D2_SBUS_OUT) {
            JsonObject sender = sendersArray.createNestedObject();
            sender["name"] = "Device2 SBUS";
            sender["index"] = 100;  // Virtual index
            sender["sent"] = g_deviceStats.device2.txBytes.load(std::memory_order_relaxed) / 25;  // SBUS frames
            sender["dropped"] = 0;  // SBUS fast path doesn't track drops
            sender["queueDepth"] = 0;  // Fast path has no queue
            sender["maxQueueDepth"] = 0;
        }

        // Device3 SBUS_OUT
        if (ctx->system.config->device3.role == D3_SBUS_OUT) {
            JsonObject sender = sendersArray.createNestedObject();
            sender["name"] = "Device3 SBUS";
            sender["index"] = 101;  // Virtual index
            sender["sent"] = g_deviceStats.device3.txBytes.load(std::memory_order_relaxed) / 25;  // SBUS frames
            sender["dropped"] = 0;  // SBUS fast path doesn't track drops
            sender["queueDepth"] = 0;  // Fast path has no queue
            sender["maxQueueDepth"] = 0;
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
    extern Config config;

    // Calculate bulk mode from all active flows
    bool bulkMode = false;
    for (size_t i = 0; i < activeFlows; i++) {
        if (flows[i].parser && flows[i].parser->isBurstActive()) {
            bulkMode = true;
            break;
        }
    }

    for (size_t i = 0; i < MAX_SENDERS; i++) {
        if (!senders[i]) continue;

        // Skip queue processing for SBUS fast path senders
        // SBUS uses sendDirect() called from SbusRouter (no queue involved)
        if (i == IDX_DEVICE2_UART2 && config.device2.role == D2_SBUS_OUT) continue;
        if (i == IDX_DEVICE3 && config.device3.role == D3_SBUS_OUT) continue;
        if (i == IDX_DEVICE4 && config.device4.role == D4_SBUS_UDP_TX) continue;

        senders[i]->processSendQueue(bulkMode);
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

    // Reset shared router pointer (don't delete - belongs to parser)
    sharedRouter = nullptr;

    // Delete all senders
    for (size_t i = 0; i < MAX_SENDERS; i++) {
        if (senders[i]) {
            delete senders[i];
            senders[i] = nullptr;
        }
    }
}

