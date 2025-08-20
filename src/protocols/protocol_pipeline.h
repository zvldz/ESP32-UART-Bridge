#ifndef PROTOCOL_PIPELINE_H
#define PROTOCOL_PIPELINE_H

#include "protocol_types.h"
#include "protocol_parser.h"
#include "packet_sender.h"
#include "raw_parser.h"
#include "mavlink_parser.h"
#include "usb_sender.h"
#include "uart_sender.h"
#include "udp_sender.h"
#include "../types.h"
#include "../circular_buffer.h"
#include "../logging.h"
#include <ArduinoJson.h>

class ProtocolPipeline {
private:
    ProtocolParser* parser;
    PacketSender* senders[3];  // Max 3 output devices
    size_t senderCount;
    CircularBuffer* inputBuffer;
    BridgeContext* ctx;
    
public:
    ProtocolPipeline(BridgeContext* context) : 
        parser(nullptr),
        senderCount(0),
        inputBuffer(nullptr),
        ctx(context) {  // CRITICAL: Save context pointer

        memset(senders, 0, sizeof(senders));
    }
    
    ~ProtocolPipeline() {
        cleanup();
    }
    
    void init(Config* config) {
        // Create parser based on protocol type
        // Map old PROTOCOL_NONE to new PROTOCOL_RAW
        switch (config->protocolOptimization) {
            case PROTOCOL_NONE:  // OLD enum value
                parser = new RawParser();
                break;
            case PROTOCOL_MAVLINK:
                parser = new MavlinkParser();
                break;
            default:
                parser = new RawParser();  // Fallback to RAW
                break;
        }
        
        // Link statistics
        if (ctx->protocol.stats) {
            parser->setStats(ctx->protocol.stats);
        }
        
        // Create senders based on configuration
        createSenders(config);
        
        // Set input buffer
        inputBuffer = ctx->adaptive.circBuf;
        
        log_msg(LOG_INFO, "Protocol pipeline initialized: %s parser, %d senders",
                parser->getName(), senderCount);
    }
    
    void process() {
        if (!parser || !inputBuffer) return;
        
        uint32_t now = micros();
        
        // Parse packets from input buffer
        ParseResult result = parser->parse(inputBuffer, now);
        
        // ALWAYS consume bytes if parser processed them
        if (result.bytesConsumed > 0) {
            inputBuffer->consume(result.bytesConsumed);
        }
        
        // THEN distribute packets if parser found them
        if (result.count > 0) {
            broadcastPackets(result.packets, result.count);
        }
        
        // Clean up parse result
        result.free();
        
        // Process send queues for all senders
        for (size_t i = 0; i < senderCount; i++) {
            if (senders[i]) {
                // EXPERIMENTAL: Could apply parser flush strategy here
                // if (parser && parser->shouldFlushNow(pendingPackets, timeSinceLastFlush)) {
                //     senders[i]->forceFlush();
                // }
                senders[i]->processSendQueue();
            }
        }
    }
    
    void handleBackpressure() {
        // Check if any sender is overwhelmed
        for (size_t i = 0; i < senderCount; i++) {
            if (senders[i] && senders[i]->getQueueDepth() > 15) {
                log_msg(LOG_WARNING, "%s sender queue depth: %d",
                        senders[i]->getName(),
                        senders[i]->getQueueDepth());
                
            }
        }
    }
    
    void getStats(char* buffer, size_t bufSize) {
        size_t offset = 0;
        offset += snprintf(buffer + offset, bufSize - offset,
                          "Parser: %s\n", parser ? parser->getName() : "None");
        
        for (size_t i = 0; i < senderCount; i++) {
            if (senders[i]) {
                offset += snprintf(buffer + offset, bufSize - offset,
                                  "%s: Sent=%u Dropped=%u Queue=%zu\n",
                                  senders[i]->getName(),
                                  senders[i]->getSentCount(),
                                  senders[i]->getDroppedCount(),
                                  senders[i]->getQueueDepth());
            }
        }
    }
    
    // Get detailed pipeline statistics as string 
    void getStatsString(char* buffer, size_t bufSize) {
        size_t offset = 0;
        
        // Parser info
        offset += snprintf(buffer + offset, bufSize - offset,
                          "[%s] ", parser ? parser->getName() : "None");
        
        // Buffer status
        if (inputBuffer) {
            offset += snprintf(buffer + offset, bufSize - offset,
                              "Buf: %zu/%zu ", 
                              inputBuffer->available(), inputBuffer->getCapacity());
        }
        
        // Sender statistics
        for (size_t i = 0; i < senderCount; i++) {
            if (senders[i]) {
                offset += snprintf(buffer + offset, bufSize - offset,
                                  "%s: Sent=%u Dropped=%u Queue=%zu Max=%zu ",
                                  senders[i]->getName(),
                                  senders[i]->getSentCount(),
                                  senders[i]->getDroppedCount(),
                                  senders[i]->getQueueDepth(),
                                  senders[i]->getMaxQueueDepth());
            }
        }
    }
    
    // Method for distributing parsed packets (used by pipeline task)
    void distributeParsedPackets(ParseResult* result) {
        if (result && result->count > 0) {
            broadcastPackets(result->packets, result->count);
        }
    }
    
    // Method for processing senders (used by pipeline task)
    void processSenders() {
        for (size_t i = 0; i < senderCount; i++) {
            if (senders[i]) {
                senders[i]->processSendQueue();
            }
        }
    }
    
    // Get parser instance for external access (e.g., adaptive timeouts)
    ProtocolParser* getParser() const {
        return parser;
    }
    
    // EXPERIMENTAL: Apply parser-specific batching strategies to senders
    void applyBatchingStrategies() {
        if (!parser) return;
        
        uint32_t batchTimeout = parser->getBatchTimeoutMs();
        
        // TODO: Apply to UDP sender if available
        // For now just expose timeout value for future use
        // UdpSender could check this value in processSendQueue()
    }
    
    // Protocol statistics for web interface
    void appendStatsToJson(JsonDocument& doc) {
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
        stats["parserName"] = parser ? parser->getName() : "None";
        
        // Parser statistics (universal for all protocols)
        if (ctx->protocol.stats) {
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
                    parserStats["packetsDetected"] = ctx->protocol.stats->packetsDetected;
                    parserStats["detectionErrors"] = ctx->protocol.stats->detectionErrors;
                    parserStats["resyncEvents"] = ctx->protocol.stats->resyncEvents;
                    parserStats["packetsTransmitted"] = ctx->protocol.stats->packetsTransmitted;
                    parserStats["bytesProcessed"] = ctx->protocol.stats->totalBytes;
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
        for (size_t i = 0; i < senderCount; i++) {
            if (senders[i]) {  // Check sender pointer
                JsonObject sender = sendersArray.createNestedObject();
                sender["name"] = senders[i]->getName();
                sender["sent"] = senders[i]->getSentCount();
                sender["dropped"] = senders[i]->getDroppedCount();
                
                
                sender["queueDepth"] = senders[i]->getQueueDepth();
                sender["maxQueueDepth"] = senders[i]->getMaxQueueDepth();
            }
        }
        
        // Buffer statistics - with null check
        if (inputBuffer) {
            JsonObject buffer = stats["buffer"].to<JsonObject>();
            buffer["used"] = inputBuffer->available();
            buffer["capacity"] = inputBuffer->getCapacity();

            // Avoid division by zero
            size_t capacity = inputBuffer->getCapacity();
            if (capacity > 0) {
                buffer["utilizationPercent"] = (inputBuffer->available() * 100) / capacity;
            } else {
                buffer["utilizationPercent"] = 0;
            }
        }
    }
    
private:
    void createSenders(Config* config) {
        senderCount = 0;
        
        // Device2 (USB) - always present, pass TX counter
        if (ctx->interfaces.usbInterface) {
            senders[senderCount++] = new UsbSender(
                ctx->interfaces.usbInterface,
                ctx->stats.device2TxBytes  // Pass TX counter pointer
            );
        }
        
        // Device3 (UART2) - optional, pass TX counter
        if (config->device3.role != D3_NONE && 
            config->device3.role != D3_UART3_LOG &&
            ctx->interfaces.device3Serial) {
            senders[senderCount++] = new UartSender(
                ctx->interfaces.device3Serial,
                ctx->stats.device3TxBytes  // Pass TX counter pointer
            );
        }
        
        // Device4 (UDP) - optional, pass TX counter
        if (config->device4.role == D4_NETWORK_BRIDGE) {
            senders[senderCount++] = new UdpSender(
                ctx->stats.device4TxBytes  // Pass TX counter pointer
            );
        }
    }
    
    void broadcastPackets(ParsedPacket* packets, size_t count) {
        // Send copies to all active senders
        for (size_t i = 0; i < count; i++) {
            for (size_t j = 0; j < senderCount; j++) {
                if (senders[j]) {
                    senders[j]->enqueue(packets[i]);
                }
            }
        }
    }
    
    void cleanup() {
        if (parser) {
            delete parser;
            parser = nullptr;
        }
        
        for (size_t i = 0; i < 3; i++) {
            if (senders[i]) {
                delete senders[i];
                senders[i] = nullptr;
            }
        }
        senderCount = 0;
    }
};

#endif // PROTOCOL_PIPELINE_H