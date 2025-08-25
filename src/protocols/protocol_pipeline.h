#ifndef PROTOCOL_PIPELINE_H
#define PROTOCOL_PIPELINE_H

#include "protocol_types.h"
#include "protocol_parser.h"
#include "packet_sender.h"
#include "raw_parser.h"
#include "mavlink_parser.h"
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
    const char* name;           // Flow name for diagnostics
    ProtocolParser* parser;     // Parser for this flow
    CircularBuffer* inputBuffer;// Input buffer for this flow
    PacketSource source;        // Source type for packet tagging
    uint8_t senderMask;         // Which senders should receive packets from this flow
    
    DataFlow() : name(nullptr), parser(nullptr), inputBuffer(nullptr), 
                 source(SOURCE_TELEMETRY), senderMask(0xFF) {}
};

class ProtocolPipeline {
private:
    // Fixed sender indices (no more shifting arrays!)
    enum SenderIndex {
        IDX_USB = 0,
        IDX_UART3 = 1, 
        IDX_UDP = 2,
        MAX_SENDERS = 3
    };
    
    // Sender masks for routing
    enum SenderMask {
        SENDER_USB = (1 << IDX_USB),
        SENDER_UART3 = (1 << IDX_UART3),
        SENDER_UDP = (1 << IDX_UDP),
        SENDER_ALL = 0xFF
    };
    
    // Maximum flows supported
    static constexpr size_t MAX_FLOWS = 2;  // Telemetry + Logs
    
    // Data flows array
    DataFlow flows[MAX_FLOWS];
    size_t activeFlows;
    
    // Fixed sender slots (can be nullptr)
    PacketSender* senders[MAX_SENDERS];
    
    // Bridge context
    BridgeContext* ctx;
    
    // Private methods
    void setupFlows(Config* config);
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
    
    // Statistics and diagnostics
    void getStats(char* buffer, size_t bufSize);
    void getStatsString(char* buffer, size_t bufSize);
    void appendStatsToJson(JsonDocument& doc);
    
    // Access methods for compatibility
    ProtocolParser* getParser() const;  // Returns first parser or nullptr
    CircularBuffer* getInputBuffer() const;  // Returns first buffer or nullptr
    
    // Sender access for statistics
    PacketSender* getSender(size_t index) const;
    size_t getSenderCount() const { return MAX_SENDERS; }  // Fixed count
    
    // Packet distribution (used by external components)
    void distributeParsedPackets(ParseResult* result);
    void processSenders();
};

#endif // PROTOCOL_PIPELINE_H