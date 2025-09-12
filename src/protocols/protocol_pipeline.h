#ifndef PROTOCOL_PIPELINE_H
#define PROTOCOL_PIPELINE_H

#include "protocol_types.h"
#include "protocol_parser.h"
#include "protocol_router.h"
#include "packet_sender.h"
#include "raw_parser.h"
#include "mavlink_parser.h"
#include "mavlink_router.h"
#include "line_based_parser.h"
#include "usb_sender.h"
#include "uart_sender.h"
#include "udp_sender.h"
#include "../types.h"
#include "../circular_buffer.h"
#include "../logging.h"
#include <ArduinoJson.h>

// Forward declaration
class CircularBuffer;

// Data flow structure for multi-stream processing
struct DataFlow {
    const char* name;               // Flow name for diagnostics
    ProtocolParser* parser;         // Parser for this flow
    ProtocolRouter* router;         // Will point to sharedRouter for MAVLink
    CircularBuffer* inputBuffer;    // Input buffer for this flow
    PacketSource source;            // Semantic source
    PhysicalInterface physInterface;// NEW: Physical source
    uint8_t senderMask;            // Which senders should receive packets from this flow
    bool isInputFlow;              // NEW: Explicit marker for device→FC flows
    
    DataFlow() : name(nullptr), parser(nullptr), router(nullptr), inputBuffer(nullptr), 
                 source(SOURCE_TELEMETRY), physInterface(PHYS_NONE), senderMask(0xFF), isInputFlow(false) {}
};

class ProtocolPipeline {
private:
    // Use SenderIndex from protocol_types.h
    // MAX_SENDERS is now defined in protocol_types.h
    
    // Sender mask constants (bit positions)
    enum SenderMask {
        SENDER_USB = (1 << IDX_DEVICE2_USB),
        SENDER_UART2 = (1 << IDX_DEVICE2_UART2),
        SENDER_UART3 = (1 << IDX_DEVICE3),
        SENDER_UDP = (1 << IDX_DEVICE4),
        SENDER_ALL = 0xFF
    };
    
    // Maximum flows supported
    static constexpr size_t MAX_FLOWS = 8;  // Support more flows
    
    // Add shared router
    MavlinkRouter* sharedRouter;
    
    // Data flows array
    DataFlow flows[MAX_FLOWS];
    size_t activeFlows;
    
    // Fixed sender slots (can be nullptr)
    PacketSender* senders[MAX_SENDERS];
    
    // Bridge context
    BridgeContext* ctx;
    
    // Private methods
    void setupFlows(Config* config);
    uint8_t calculateSbusInputRouting(Config* config);
    void createSenders(Config* config);
    void processFlow(DataFlow& flow);
    void distributePackets(ParsedPacket* packets, size_t count, PacketSource source, uint8_t senderMask);
    void cleanup();
    
public:
    ProtocolPipeline(BridgeContext* context);
    ~ProtocolPipeline();
    
    // Main interface
    void init(Config* config);
    void process();
    void handleBackpressure();
    
    // Add methods for separate processing
    void processInputFlows();   // Process GCS→FC flows
    void processTelemetryFlow(); // Process FC→GCS flow
    
    // Check if any input flow has data (optimization for main loop)
    bool hasInputData() const {
        for (size_t i = 0; i < activeFlows; i++) {
            if (flows[i].isInputFlow && flows[i].inputBuffer && 
                flows[i].inputBuffer->available() > 0) {
                return true;
            }
        }
        return false;
    }
    
    // Statistics and diagnostics
    void getStats(char* buffer, size_t bufSize);
    void getStatsString(char* buffer, size_t bufSize);
    void appendStatsToJson(JsonDocument& doc);
    
    // Access methods for compatibility
    ProtocolParser* getParser() const;  // Returns first parser or nullptr
    CircularBuffer* getInputBuffer() const;  // Returns first buffer or nullptr
    
    MavlinkRouter* getSharedRouter() { return sharedRouter; }
    
    // Sender access for statistics
    PacketSender* getSender(size_t index) const;
    size_t getSenderCount() const { return MAX_SENDERS; }  // Fixed count
    
    // Packet distribution (used by external components)
    void distributeParsedPackets(ParseResult* result);
    void processSenders();
};

#endif // PROTOCOL_PIPELINE_H