#ifndef SBUS_FAST_PARSER_H
#define SBUS_FAST_PARSER_H

#include "protocol_parser.h"
#include "sbus_router.h"
#include "sbus_common.h"
#include "../config.h"
#include "../device_stats.h"
#include "../uart/uart_interface.h"

// Forward declarations
extern Config config;

class SbusFastParser : public ProtocolParser {
private:
    SbusRouter* router;         // Pointer to singleton
    uint8_t sourceId;           // CRITICAL: Store source ID for this parser
    uint32_t invalidFrames;     // Statistics
    uint32_t validFrames;

public:
    // CRITICAL: Constructor with source ID
    explicit SbusFastParser(uint8_t src = SBUS_SOURCE_DEVICE1) :
        sourceId(src),
        invalidFrames(0),
        validFrames(0) {

        // Get singleton router instance
        router = SbusRouter::getInstance();
        log_msg(LOG_INFO, "SbusFastParser created for source %d", src);
    }

    // Fast path implementation
    bool tryFastProcess(CircularBuffer* buffer, BridgeContext* ctx) override {
        // Check if we have complete SBUS frame
        if (buffer->available() < 25) return false;

        // Get contiguous view for SBUS frame
        ContiguousView view = buffer->getContiguousForParser(25);
        if (view.safeLen < 25) return false;  // Not enough contiguous data

        // Check first byte
        if (view.ptr[0] != 0x0F) {
            buffer->consume(1);  // Resync: slide window by 1 byte
            return true;  // Handled
        }

        // Validate end byte (all 4 valid SBUS end bytes)
        if (view.ptr[24] != 0x00 && view.ptr[24] != 0x04 &&
            view.ptr[24] != 0x14 && view.ptr[24] != 0x24) {
            // Invalid end byte - resync by consuming only 1 byte
            buffer->consume(1);  // Slide window
            invalidFrames++;
            return true;  // Handled
        }

        // Valid frame - copy it
        uint8_t frame[25];
        memcpy(frame, view.ptr, 25);
        buffer->consume(25);

        validFrames++;

        // Route through singleton router
        // Router will handle:
        // - Source selection (Auto/Manual)
        // - Failsafe state management
        // - Writing to all registered outputs
        router->routeFrame(frame, sourceId);

        return true;  // Processed via fast path
    }

    // Normal parse (fallback for partial frames)
    ParseResult parse(CircularBuffer* buffer, uint32_t currentTime) override {
        ParseResult result;

        // This is called when tryFastProcess returns false
        // Usually means we don't have enough data yet (< 25 bytes)
        // This is normal during startup or when data arrives slowly

        // Just wait for more data - return empty result
        result.packets = nullptr;
        result.count = 0;
        result.bytesConsumed = 0;

        return result;
    }

    void reset() override {
        invalidFrames = 0;
        validFrames = 0;
    }

    const char* getName() const override {
        return "SBUS_Fast";
    }

    size_t getMinimumBytes() const override {
        return 25;
    }

    // Getters for statistics
    uint32_t getValidFrames() const { return validFrames; }
    uint32_t getInvalidFrames() const { return invalidFrames; }
};

#endif