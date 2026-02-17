// CRSF/ELRS Frame Parser with text and binary output
// Parses CRSF frames from CircularBuffer, sends to registered outputs:
//   - Text outputs: human-readable format with per-output RC rate limiting
//   - Binary outputs: raw CRSF frames forwarded as-is (no rate limiting)
#pragma once

#include "protocol_parser.h"
#include "crsf_protocol.h"
#include "rc_channels.h"
#include "packet_sender.h"
#include "../config.h"
#include "../device_stats.h"
#include "../logging.h"
#include <vector>
#include <stdio.h>

// Forward declarations
extern Config config;

// Text output buffer: longest line is RC with 16 channels
// "RC " + 16*5 + 15 commas + "\r\n" + null = ~100 bytes, round up
static constexpr size_t CRSF_TEXT_BUFFER_SIZE = 200;

class CrsfParser : public ProtocolParser {
private:
    uint32_t validFrames;
    uint32_t invalidFrames;
    uint32_t crcErrors;
    uint32_t lastFrameTime;

    // Per-output rate limiting for RC channels (telemetry passes unrestricted)
    struct CrsfOutput {
        PacketSender* sender;
        uint32_t rateIntervalMs;
        uint32_t lastRcSendMs;
    };
    std::vector<CrsfOutput> textOutputs;

    // Binary outputs: raw CRSF frames forwarded without conversion (no rate limiting)
    std::vector<PacketSender*> binaryOutputs;

    // Text output buffer
    char textBuf[CRSF_TEXT_BUFFER_SIZE];

    // Format RC channels frame to text: "RC 1500,1500,...\r\n"
    size_t formatRcChannels(const uint8_t* payload, size_t payloadLen) {
        if (payloadLen < CRSF_RC_PAYLOAD_SIZE) return 0;

        uint16_t channels[CRSF_RC_CHANNELS];
        unpackCrsfChannels(payload, channels);

        int len = snprintf(textBuf, sizeof(textBuf),
            "RC %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
            crsfToUs(channels[0]),  crsfToUs(channels[1]),
            crsfToUs(channels[2]),  crsfToUs(channels[3]),
            crsfToUs(channels[4]),  crsfToUs(channels[5]),
            crsfToUs(channels[6]),  crsfToUs(channels[7]),
            crsfToUs(channels[8]),  crsfToUs(channels[9]),
            crsfToUs(channels[10]), crsfToUs(channels[11]),
            crsfToUs(channels[12]), crsfToUs(channels[13]),
            crsfToUs(channels[14]), crsfToUs(channels[15]));

        return (len > 0 && (size_t)len < sizeof(textBuf)) ? len : 0;
    }

    // Format Link Statistics: "LQ upRSSI,upLQ,upSNR,rfMode,txPower,dnRSSI,dnLQ,dnSNR\r\n"
    size_t formatLinkStats(const uint8_t* payload, size_t payloadLen) {
        if (payloadLen < 10) return 0;

        // CRSF Link Statistics payload (10 bytes):
        // [0] uplink RSSI Ant1 (dBm, value is negative, stored unsigned)
        // [1] uplink RSSI Ant2
        // [2] uplink Link Quality (%)
        // [3] uplink SNR (dB, signed)
        // [4] active antenna
        // [5] RF mode
        // [6] uplink TX power (index)
        // [7] downlink RSSI (dBm, unsigned)
        // [8] downlink Link Quality (%)
        // [9] downlink SNR (dB, signed)
        int len = snprintf(textBuf, sizeof(textBuf),
            "LQ -%d,%d,%d,%d,%d,-%d,%d,%d\r\n",
            payload[0], payload[2], (int8_t)payload[3],
            payload[5], payload[6],
            payload[7], payload[8], (int8_t)payload[9]);

        return (len > 0 && (size_t)len < sizeof(textBuf)) ? len : 0;
    }

    // Format Battery: "BAT voltage,current,mAh,remaining\r\n"
    size_t formatBattery(const uint8_t* payload, size_t payloadLen) {
        if (payloadLen < 8) return 0;

        // Battery payload (8 bytes):
        // [0-1] voltage (big-endian, in 0.1V)
        // [2-3] current (big-endian, in 0.1A)
        // [4-6] capacity used (big-endian, in mAh)
        // [7]   remaining (%)
        uint16_t voltage = (payload[0] << 8) | payload[1];
        uint16_t current = (payload[2] << 8) | payload[3];
        uint32_t capacity = (payload[4] << 16) | (payload[5] << 8) | payload[6];

        int len = snprintf(textBuf, sizeof(textBuf),
            "BAT %d.%d,%d.%d,%lu,%d\r\n",
            voltage / 10, voltage % 10,
            current / 10, current % 10,
            (unsigned long)capacity, payload[7]);

        return (len > 0 && (size_t)len < sizeof(textBuf)) ? len : 0;
    }

    // Format GPS: "GPS lat,lon,groundspeed,heading,alt,sats\r\n"
    size_t formatGps(const uint8_t* payload, size_t payloadLen) {
        if (payloadLen < 15) return 0;

        // GPS payload (15 bytes, big-endian):
        // [0-3]   latitude (degrees * 1e7, signed)
        // [4-7]   longitude (degrees * 1e7, signed)
        // [8-9]   groundspeed (km/h * 10)
        // [10-11]  heading (degrees * 100)
        // [12-13]  altitude (meters + 1000m offset)
        // [14]     satellites
        int32_t lat = (int32_t)((payload[0] << 24) | (payload[1] << 16) | (payload[2] << 8) | payload[3]);
        int32_t lon = (int32_t)((payload[4] << 24) | (payload[5] << 16) | (payload[6] << 8) | payload[7]);
        uint16_t speed = (payload[8] << 8) | payload[9];
        uint16_t heading = (payload[10] << 8) | payload[11];
        int16_t alt = ((payload[12] << 8) | payload[13]) - 1000;

        int len = snprintf(textBuf, sizeof(textBuf),
            "GPS %ld.%07ld,%ld.%07ld,%d.%d,%d.%02d,%d,%d\r\n",
            (long)(lat / 10000000L), (long)(abs(lat) % 10000000L),
            (long)(lon / 10000000L), (long)(abs(lon) % 10000000L),
            speed / 10, speed % 10,
            heading / 100, heading % 100,
            alt, payload[14]);

        return (len > 0 && (size_t)len < sizeof(textBuf)) ? len : 0;
    }

    // Format Attitude: "ATT pitch,roll,yaw\r\n"
    size_t formatAttitude(const uint8_t* payload, size_t payloadLen) {
        if (payloadLen < 6) return 0;

        // Attitude payload (6 bytes, big-endian):
        // [0-1] pitch (radians * 10000, signed)
        // [2-3] roll (radians * 10000, signed)
        // [4-5] yaw (radians * 10000, signed)
        int16_t pitch = (int16_t)((payload[0] << 8) | payload[1]);
        int16_t roll  = (int16_t)((payload[2] << 8) | payload[3]);
        int16_t yaw   = (int16_t)((payload[4] << 8) | payload[5]);

        // Convert radians*10000 to degrees*10 (rad * 180/pi * 10000 ≈ 5729.6)
        int pitchDeg10 = (int)(pitch * 573L / 1000);
        int rollDeg10  = (int)(roll * 573L / 1000);
        int yawDeg10   = (int)(yaw * 573L / 1000);

        int len = snprintf(textBuf, sizeof(textBuf),
            "ATT %d.%d,%d.%d,%d.%d\r\n",
            pitchDeg10 / 10, abs(pitchDeg10) % 10,
            rollDeg10 / 10, abs(rollDeg10) % 10,
            yawDeg10 / 10, abs(yawDeg10) % 10);

        return (len > 0 && (size_t)len < sizeof(textBuf)) ? len : 0;
    }

    // Format Flight Mode: "FM modename\r\n"
    size_t formatFlightMode(const uint8_t* payload, size_t payloadLen) {
        if (payloadLen < 1) return 0;

        // Flight mode is a null-terminated string
        size_t maxStr = (payloadLen < 20) ? payloadLen : 20;

        int len = snprintf(textBuf, sizeof(textBuf), "FM ");
        for (size_t i = 0; i < maxStr && payload[i] != 0; i++) {
            if (len < (int)sizeof(textBuf) - 3) {
                textBuf[len++] = payload[i];
            }
        }
        textBuf[len++] = '\r';
        textBuf[len++] = '\n';
        textBuf[len] = '\0';

        return len;
    }

    // Format Baro Altitude: "ALT altitude,vario\r\n"
    size_t formatBaroAlt(const uint8_t* payload, size_t payloadLen) {
        if (payloadLen < 4) return 0;

        // Baro altitude payload (4 bytes, big-endian):
        // [0-1] altitude (dm + 10000dm offset)
        // [2-3] vario (cm/s, signed)
        int16_t altDm = ((payload[0] << 8) | payload[1]) - 10000;
        int16_t varioCms = (int16_t)((payload[2] << 8) | payload[3]);

        int len = snprintf(textBuf, sizeof(textBuf),
            "ALT %d.%d,%d.%02d\r\n",
            altDm / 10, abs(altDm) % 10,
            varioCms / 100, abs(varioCms) % 100);

        return (len > 0 && (size_t)len < sizeof(textBuf)) ? len : 0;
    }

    // Send telemetry text to all text outputs (no rate limiting)
    void sendTextToOutputs(size_t textLen) {
        if (textLen == 0 || textOutputs.empty()) return;

        for (auto& out : textOutputs) {
            out.sender->sendDirect((const uint8_t*)textBuf, textLen);
        }
    }

    // Send RC channels text with per-output rate limiting
    void sendTextRcToOutputs(size_t textLen) {
        if (textLen == 0 || textOutputs.empty()) return;

        uint32_t now = millis();
        for (auto& out : textOutputs) {
            if (out.rateIntervalMs > 0 && (now - out.lastRcSendMs) < out.rateIntervalMs) {
                continue;
            }
            out.lastRcSendMs = now;
            out.sender->sendDirect((const uint8_t*)textBuf, textLen);
        }
    }

    // Send raw CRSF frame to all binary outputs (no rate limiting, full throughput)
    void sendRawToOutputs(const uint8_t* data, size_t len) {
        if (len == 0 || binaryOutputs.empty()) return;

        for (auto* sender : binaryOutputs) {
            sender->sendDirect(data, len);
        }
    }

    // Process a valid CRSF frame (type + payload already validated)
    void processFrame(uint8_t type, const uint8_t* payload, size_t payloadLen) {
        size_t textLen = 0;

        switch (type) {
            case CRSF_TYPE_RC_CHANNELS: {
                // Update shared RC channel data for web UI monitor
                if (payloadLen >= CRSF_RC_PAYLOAD_SIZE) {
                    uint16_t raw[16];
                    unpackCrsfChannels(payload, raw);
                    for (int i = 0; i < 16; i++) rcChannels.channels[i] = crsfToUs(raw[i]);
                    rcChannels.lastUpdateMs = millis();
                }
                textLen = formatRcChannels(payload, payloadLen);
                sendTextRcToOutputs(textLen);  // Per-output rate limiting
                return;
            }
            case CRSF_TYPE_LINK_STATS:
                textLen = formatLinkStats(payload, payloadLen);
                break;
            case CRSF_TYPE_BATTERY:
                textLen = formatBattery(payload, payloadLen);
                break;
            case CRSF_TYPE_GPS:
                textLen = formatGps(payload, payloadLen);
                break;
            case CRSF_TYPE_ATTITUDE:
                textLen = formatAttitude(payload, payloadLen);
                break;
            case CRSF_TYPE_FLIGHT_MODE:
                textLen = formatFlightMode(payload, payloadLen);
                break;
            case CRSF_TYPE_BARO_ALT:
                textLen = formatBaroAlt(payload, payloadLen);
                break;
            default:
                // Unknown frame type — skip for now
                break;
        }

        sendTextToOutputs(textLen);
    }

public:
    CrsfParser() :
        validFrames(0),
        invalidFrames(0),
        crcErrors(0),
        lastFrameTime(0) {
        log_msg(LOG_INFO, "CrsfParser created (420000 baud, CRC8 DVB-S2)");
    }

    // Register text output with independent RC rate (called during pipeline setup)
    void registerTextOutput(PacketSender* sender, uint8_t rateHz = 50) {
        if (!sender) return;
        CrsfOutput out;
        out.sender = sender;
        out.rateIntervalMs = (rateHz > 0 && rateHz <= 100) ? (1000 / rateHz) : 0;
        out.lastRcSendMs = 0;
        textOutputs.push_back(out);
        log_msg(LOG_INFO, "CRSF text output: %s (%d Hz, total: %zu)", sender->getName(), rateHz, textOutputs.size());
    }

    // Register binary output for raw CRSF frame forwarding (no rate limiting)
    void registerBinaryOutput(PacketSender* sender) {
        if (!sender) return;
        binaryOutputs.push_back(sender);
        log_msg(LOG_INFO, "CRSF binary output: %s (total: %zu)", sender->getName(), binaryOutputs.size());
    }

    // Fast path: parse CRSF frames from circular buffer
    bool tryFastProcess(CircularBuffer* buffer, BridgeContext* ctx) override {
        size_t avail = buffer->available();
        if (avail < CRSF_MIN_FRAME_SIZE) return false;

        // Peek at first 2 bytes: address + length
        ContiguousView view = buffer->getContiguousForParser(
            (avail < CRSF_MAX_FRAME_SIZE) ? avail : CRSF_MAX_FRAME_SIZE);
        if (view.safeLen < 2) return false;

        uint8_t addr = view.ptr[0];
        uint8_t frameLen = view.ptr[1];  // Length includes type + payload + CRC

        // Validate address
        if (!crsfIsValidAddress(addr)) {
            buffer->consume(1);  // Resync
            return true;
        }

        // Validate length (min 2: type + CRC, max 62: fits in 64-byte frame)
        if (frameLen < 2 || frameLen > 62) {
            buffer->consume(1);  // Resync
            invalidFrames++;
            return true;
        }

        // Total frame size: addr + len byte + (type + payload + CRC)
        size_t totalSize = 2 + frameLen;

        // Wait for complete frame
        if (avail < totalSize) return false;

        // Re-get view with exact frame size
        if (view.safeLen < totalSize) {
            view = buffer->getContiguousForParser(totalSize);
            if (view.safeLen < totalSize) return false;
        }

        // CRC8 over type + payload (bytes 2..totalSize-2)
        uint8_t crc = crsfCrc8(&view.ptr[2], frameLen - 1);
        uint8_t expectedCrc = view.ptr[totalSize - 1];

        if (crc != expectedCrc) {
            buffer->consume(1);  // Resync on CRC error
            crcErrors++;
            invalidFrames++;
            return true;
        }

        // Valid frame — extract type and payload
        uint8_t type = view.ptr[2];
        const uint8_t* payload = &view.ptr[3];
        size_t payloadLen = frameLen - 2;  // Subtract type and CRC

        // Forward raw frame to binary outputs BEFORE consume (view.ptr still valid)
        sendRawToOutputs(view.ptr, totalSize);

        // Consume frame from buffer
        buffer->consume(totalSize);

        validFrames++;
        lastFrameTime = millis();

        // Update device stats
        g_deviceStats.device1.rxBytes.fetch_add(totalSize, std::memory_order_relaxed);
        g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);

        // Process and output as text
        processFrame(type, payload, payloadLen);

        return true;
    }

    // Fallback parse (not enough data — wait)
    ParseResult parse(CircularBuffer* buffer, uint32_t currentTime) override {
        ParseResult result;
        result.packets = nullptr;
        result.count = 0;
        result.bytesConsumed = 0;
        return result;
    }

    void reset() override {
        validFrames = 0;
        invalidFrames = 0;
        crcErrors = 0;
        lastFrameTime = 0;
    }

    const char* getName() const override { return "CRSF"; }
    size_t getMinimumBytes() const override { return CRSF_MIN_FRAME_SIZE; }

    // Statistics
    uint32_t getValidFrames() const { return validFrames; }
    uint32_t getInvalidFrames() const { return invalidFrames; }
    uint32_t getCrcErrors() const { return crcErrors; }
    uint32_t getLastFrameTime() const { return lastFrameTime; }
};
