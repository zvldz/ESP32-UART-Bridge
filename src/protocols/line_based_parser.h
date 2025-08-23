#ifndef LINE_BASED_PARSER_H
#define LINE_BASED_PARSER_H

#include "protocol_parser.h"

class LineBasedParser : public ProtocolParser {
private:
    static constexpr size_t MAX_LINE_LENGTH = 256;
    
    size_t findNewline(CircularBuffer* buffer) {
        size_t avail = buffer->available();
        if (avail == 0) return 0;
        
        size_t searchLen = std::min(avail, MAX_LINE_LENGTH);
        ContiguousView view = buffer->getContiguousForParser(searchLen);
        
        for (size_t i = 0; i < view.safeLen; i++) {
            if (view.ptr[i] == '\n') {
                // Check if preceded by \r for \r\n support
                if (i > 0 && view.ptr[i-1] == '\r') {
                    return i + 1;  // Include both \r\n
                }
                return i + 1;  // Include \n
            }
            // Also handle standalone \r as line ending (old Mac style)
            if (view.ptr[i] == '\r') {
                // Check if followed by \n
                if (i + 1 < view.safeLen && view.ptr[i+1] == '\n') {
                    return i + 2;  // Include \r\n
                }
                return i + 1;  // Just \r
            }
        }
        return 0;  // No line ending found
    }
    
public:
    ParseResult parse(CircularBuffer* buffer, uint32_t currentTime) override {
        ParseResult result;
        result.count = 0;
        result.bytesConsumed = 0;
        
        size_t avail = buffer->available();
        if (avail == 0) return result;
        
        // Find line ending
        size_t lineLen = findNewline(buffer);
        
        // If no line ending found and we have MAX_LINE_LENGTH bytes
        if (lineLen == 0 && avail >= MAX_LINE_LENGTH) {
            // Treat as broken line - consume without creating packet
            result.bytesConsumed = MAX_LINE_LENGTH;
            log_msg(LOG_WARNING, "[LineBased] Skipping %zu bytes without line ending", MAX_LINE_LENGTH);
            return result;
        }
        
        // If no complete line yet, wait for more data
        if (lineLen == 0) {
            return result;
        }
        
        // Get line data
        ContiguousView view = buffer->getContiguousForParser(lineLen);
        
        // Create packet for complete line
        result.packets = new ParsedPacket[1];
        result.count = 1;
        
        result.packets[0].data = new uint8_t[lineLen];
        result.packets[0].size = lineLen;
        result.packets[0].allocSize = lineLen;
        result.packets[0].timestamp = currentTime;
        result.packets[0].hints.keepWhole = true;
        
        // Initialize MAVLink fields to prevent garbage in diagnostics
        result.packets[0].mavlinkMsgId = 0;
        result.packets[0].seqNum = 0;
        result.packets[0].parseTimeMicros = 0;
        result.packets[0].enqueueTimeMicros = 0;
        
        memcpy(result.packets[0].data, view.ptr, lineLen);
        
        result.bytesConsumed = lineLen;
        
        if (stats) {
            stats->packetsDetected++;
            stats->totalBytes += lineLen;
        }
        
        return result;
    }
    
    void reset() override {}
    const char* getName() const override { return "LineBased"; }
    size_t getMinimumBytes() const override { return 2; }
};

#endif