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
        ctx(context) {
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
        
        if (result.count > 0) {
            // Distribute packets to all senders
            broadcastPackets(result.packets, result.count);
            
            // Consume processed bytes from input buffer
            if (result.bytesConsumed > 0) {
                inputBuffer->consume(result.bytesConsumed);
            }
            
            // Clean up parse result
            result.free();
        }
        
        // Process send queues for all senders
        for (size_t i = 0; i < senderCount; i++) {
            if (senders[i]) {
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
                
                // Could implement priority dropping here
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
    
private:
    void createSenders(Config* config) {
        senderCount = 0;
        
        // Device2 (USB) - always present
        if (ctx->interfaces.usbInterface) {
            senders[senderCount++] = new UsbSender(ctx->interfaces.usbInterface);
        }
        
        // Device3 (UART2) - optional
        if (config->device3.role != D3_NONE && 
            config->device3.role != D3_UART3_LOG &&
            ctx->interfaces.device3Serial) {
            senders[senderCount++] = new UartSender(ctx->interfaces.device3Serial);
        }
        
        // Device4 (UDP) - optional
        if (config->device4.role == D4_NETWORK_BRIDGE) {
            senders[senderCount++] = new UdpSender();
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