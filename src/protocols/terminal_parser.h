#ifndef TERMINAL_PARSER_H
#define TERMINAL_PARSER_H

#include "protocol_parser.h"
#include "../logging.h"
#include "../circular_buffer.h"
#include <Arduino.h>

// Terminal buffer for WebSocket streaming (allocated once, read by web server)
// Ring buffer that supports multiple readers (history + live streaming)
class TerminalBuffer {
private:
    uint8_t* buf;
    size_t capacity;
    size_t writePos;      // Total bytes written (monotonic)
    size_t wsSentPos;     // Total bytes sent via WebSocket
    portMUX_TYPE lock;
    static TerminalBuffer* instance;

public:
    TerminalBuffer() : buf(nullptr), capacity(0), writePos(0), wsSentPos(0) {
        portMUX_INITIALIZE(&lock);
    }

    static TerminalBuffer* getInstance() {
        if (!instance) {
            instance = new TerminalBuffer();
        }
        return instance;
    }

    void init() {
        if (buf) return;

        // Try PSRAM for larger buffer, fallback to internal RAM
        size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        if (psramFree > 65536) {
            capacity = 32768;  // 32 KB with PSRAM
            buf = static_cast<uint8_t*>(heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM));
        }
        if (!buf) {
            capacity = 4096;   // 4 KB without PSRAM
            buf = static_cast<uint8_t*>(malloc(capacity));
        }
        if (!buf) {
            log_msg(LOG_ERROR, "Terminal buffer allocation failed");
            capacity = 0;
            return;
        }
        log_msg(LOG_INFO, "Terminal buffer: %zu bytes", capacity);
    }

    void write(const uint8_t* data, size_t len) {
        if (!buf || len == 0) return;
        portENTER_CRITICAL(&lock);
        for (size_t i = 0; i < len; i++) {
            buf[writePos % capacity] = data[i];
            writePos++;
        }
        // If WebSocket reader fell behind (buffer wrapped), catch it up
        if (writePos > wsSentPos + capacity) {
            wsSentPos = writePos - capacity;
        }
        portEXIT_CRITICAL(&lock);
    }

    // Get new data for WebSocket poll (since last read)
    size_t readNew(uint8_t* dst, size_t maxLen) {
        if (!buf) return 0;
        portENTER_CRITICAL(&lock);
        size_t avail = writePos - wsSentPos;
        if (avail == 0) {
            portEXIT_CRITICAL(&lock);
            return 0;
        }
        size_t toRead = min(avail, maxLen);
        for (size_t i = 0; i < toRead; i++) {
            dst[i] = buf[(wsSentPos + i) % capacity];
        }
        wsSentPos += toRead;
        portEXIT_CRITICAL(&lock);
        return toRead;
    }

    // Get buffered history chunk (for new WebSocket client connect)
    // offset: byte offset from start of history, returns 0 when done
    size_t readHistory(uint8_t* dst, size_t maxLen, size_t offset) {
        if (!buf) return 0;
        portENTER_CRITICAL(&lock);
        size_t totalInBuf = min(writePos, capacity);
        if (offset >= totalInBuf) {
            portEXIT_CRITICAL(&lock);
            return 0;
        }
        size_t remaining = totalInBuf - offset;
        size_t toRead = min(remaining, maxLen);
        size_t startPos = writePos - totalInBuf + offset;
        for (size_t i = 0; i < toRead; i++) {
            dst[i] = buf[(startPos + i) % capacity];
        }
        portEXIT_CRITICAL(&lock);
        return toRead;
    }

    // Reset WebSocket reader to current position (after sending history)
    void resetWsReader() {
        portENTER_CRITICAL(&lock);
        wsSentPos = writePos;
        portEXIT_CRITICAL(&lock);
    }

    size_t available() const {
        return writePos - wsSentPos;
    }
};

// Terminal parser: line-based with timeout fallback + tap to terminal buffer
class TerminalParser : public ProtocolParser {
private:
    // Short timeout to collect burst bytes before flush (e.g. escape sequences)
    // Note: parse() receives millis() as currentTime
    static constexpr uint32_t FLUSH_TIMEOUT_MS = 5;  // 5ms
    static constexpr size_t MAX_CHUNK = 512;

    TerminalBuffer* termBuf;
    uint32_t bufferStartTime;

public:
    TerminalParser() : bufferStartTime(0) {
        termBuf = TerminalBuffer::getInstance();
        termBuf->init();
        log_msg(LOG_INFO, "TerminalParser initialized");
    }

    ParseResult parse(CircularBuffer* buffer, uint32_t currentTime) override {
        ParseResult result;
        result.count = 0;
        result.bytesConsumed = 0;

        size_t avail = buffer->available();
        if (avail == 0) {
            bufferStartTime = 0;
            return result;
        }

        if (bufferStartTime == 0) {
            bufferStartTime = currentTime;
        }

        // Wait briefly to collect burst (escape sequences, multi-byte output)
        uint32_t timeInBuffer = currentTime - bufferStartTime;
        if (timeInBuffer < FLUSH_TIMEOUT_MS && avail < MAX_CHUNK) {
            return result;  // Wait for more data or timeout
        }

        // Flush all available data (up to MAX_CHUNK)
        size_t chunkLen = std::min(avail, MAX_CHUNK);

        // Create packet and copy data from ring buffer segments
        result.packets = new ParsedPacket[1];
        result.count = 1;

        result.packets[0].data = new uint8_t[chunkLen];
        result.packets[0].size = chunkLen;
        result.packets[0].allocSize = chunkLen;
        result.packets[0].format = DataFormat::FORMAT_RAW;
        result.packets[0].hints.keepWhole = true;
        result.packets[0].parseTimeMicros = micros();

        auto segments = buffer->getReadSegments();
        size_t copied = 0;
        if (segments.first.size > 0) {
            size_t n = std::min((size_t)segments.first.size, chunkLen);
            memcpy(result.packets[0].data, segments.first.data, n);
            copied += n;
        }
        if (copied < chunkLen && segments.second.size > 0) {
            size_t n = std::min((size_t)segments.second.size, chunkLen - copied);
            memcpy(result.packets[0].data + copied, segments.second.data, n);
        }

        // Tap: copy to terminal buffer for WebSocket streaming
        termBuf->write(result.packets[0].data, chunkLen);

        result.bytesConsumed = chunkLen;
        bufferStartTime = 0;

        if (stats) {
            stats->packetsDetected++;
            stats->totalBytes += chunkLen;
        }

        return result;
    }

    void reset() override {
        bufferStartTime = 0;
        if (stats) {
            stats->reset();
        }
        log_msg(LOG_DEBUG, "TerminalParser reset");
    }

    const char* getName() const override {
        return "TERMINAL";
    }

    size_t getMinimumBytes() const override {
        return 1;  // Can flush on timeout even with 1 byte
    }
};

// Static member definition
inline TerminalBuffer* TerminalBuffer::instance = nullptr;

#endif // TERMINAL_PARSER_H
