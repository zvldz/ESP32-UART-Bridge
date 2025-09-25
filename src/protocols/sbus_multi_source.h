#ifndef SBUS_MULTI_SOURCE_H
#define SBUS_MULTI_SOURCE_H

#include "sbus_common.h"
#include "../logging.h"
#include <stdint.h>  // for uint32_t, uint8_t
#include <string.h>  // for memset, memcpy

// Note: Use millis() from existing project infrastructure
// If not available, include appropriate timer headers

// Source types for SBUS data
enum SbusSourceType {
    SBUS_SOURCE_LOCAL = 0,   // Physical SBUS input on same ESP
    SBUS_SOURCE_UART = 1,    // UART transport from another ESP
    SBUS_SOURCE_UDP = 2,     // UDP/WiFi from another ESP
    SBUS_SOURCE_NONE = 3     // No source/failsafe
};

// Source state tracking
struct SbusSourceState {
    bool hasData;
    uint32_t lastUpdateTime;
    uint16_t channels[16];
    uint8_t flags;

    // Metrics for quality calculation
    uint32_t framesReceived;    // Total frames from this source
    uint32_t framesMissed;      // Estimated missed frames (gaps)
    uint32_t lastFrameTime;     // For timeout detection

    // Quality calculation
    uint8_t getQuality() const {
        if (!hasData) return 0;

        // Simple quality based on recent activity
        uint32_t timeSinceFrame = millis() - lastFrameTime;
        if (timeSinceFrame > 5000) return 0;  // No data for 5s = 0%
        if (timeSinceFrame > 2000) return 25; // Old data = 25%
        if (timeSinceFrame > 1000) return 50; // Stale = 50%

        // Calculate based on loss rate if we have enough data
        if (framesReceived > 100) {
            uint32_t total = framesReceived + framesMissed;
            uint8_t successRate = (framesReceived * 100) / total;
            return successRate;
        }

        return 90; // Fresh data, not enough samples yet
    }

    const char* getStateString() const {
        if (!hasData) return "no_signal";
        uint32_t timeSinceFrame = millis() - lastFrameTime;
        if (timeSinceFrame < 100) return "active";
        if (timeSinceFrame < 1000) return "standby";
        return "offline";
    }
};

// Configuration for multi-source (will be expanded in Phase 9.3)
struct SbusMultiSourceConfig {
    SbusSourceType forcedSource;     // Manual override source
    bool manualMode;                  // true = manual, false = auto
    uint32_t timeoutMs;              // Source timeout (Phase 9.3)
    uint32_t hysteresisMs;           // Switch stability (Phase 9.3)
    uint8_t priorities[3];           // Source priorities (Phase 9.3)
};

class SbusMultiSource {
private:
    SbusSourceState sources[3];      // LOCAL, UART, UDP
    SbusSourceType activeSource;     // Currently selected source
    SbusMultiSourceConfig config;

    // Statistics
    uint32_t switchCount;            // Total source switches
    uint32_t lastSwitchTime;         // When last switch occurred

public:
    SbusMultiSource() :
        activeSource(SBUS_SOURCE_NONE),
        switchCount(0),
        lastSwitchTime(0) {

        // Initialize states
        memset(sources, 0, sizeof(sources));

        // Default config
        config.forcedSource = SBUS_SOURCE_NONE;
        config.manualMode = false;
        config.timeoutMs = 1000;
        config.hysteresisMs = 100;
        config.priorities[0] = SBUS_SOURCE_LOCAL;
        config.priorities[1] = SBUS_SOURCE_UART;
        config.priorities[2] = SBUS_SOURCE_UDP;

        log_msg(LOG_INFO, "SBUS MultiSource initialized");
    }

    // Update source data
    void updateSource(SbusSourceType type, uint16_t* channels, uint8_t flags) {
        if (type >= SBUS_SOURCE_NONE) return;

        SbusSourceState& src = sources[type];

        // Track frame timing for quality metrics
        uint32_t now = millis();
        if (src.hasData && src.lastFrameTime > 0) {
            // Estimate missed frames (expecting ~50Hz)
            uint32_t elapsed = now - src.lastFrameTime;
            uint32_t expectedFrames = elapsed / 20; // 20ms per frame
            if (expectedFrames > 1) {
                src.framesMissed += (expectedFrames - 1);
            }
        }

        // Update state
        memcpy(src.channels, channels, sizeof(src.channels));
        src.flags = flags;
        src.hasData = true;
        src.lastUpdateTime = now;
        src.lastFrameTime = now;
        src.framesReceived++;

        // Log significant events
        static uint32_t lastLog[3] = {0, 0, 0};
        if (now - lastLog[type] > 5000) { // Log every 5s per source
            log_msg(LOG_DEBUG, "SBUS source %s: %u frames, quality %u%%",
                    getSourceName(type), src.framesReceived, src.getQuality());
            lastLog[type] = now;
        }
    }

    // Get active channels (called by SbusHub)
    bool getActiveChannels(uint16_t* channels, uint8_t* flags) {
        // In manual mode, use forced source
        SbusSourceType source = config.manualMode ? config.forcedSource : selectBestSource();

        // Switch source if needed
        if (source != activeSource) {
            log_msg(LOG_INFO, "SBUS source switch: %s -> %s",
                    getSourceName(activeSource), getSourceName(source));
            activeSource = source;
            switchCount++;
            lastSwitchTime = millis();
        }

        // Get data from active source
        if (source < SBUS_SOURCE_NONE && sources[source].hasData) {
            memcpy(channels, sources[source].channels, 16 * sizeof(uint16_t));
            *flags = sources[source].flags;
            return true;
        }

        return false; // No valid source
    }

    // Manual control
    void forceSource(SbusSourceType type) {
        config.forcedSource = type;
        config.manualMode = true;
        log_msg(LOG_INFO, "SBUS: Manual mode, forced to %s", getSourceName(type));
    }

    void setAutoMode() {
        config.manualMode = false;
        log_msg(LOG_INFO, "SBUS: Switched to AUTO mode");
    }

    // Configuration
    void setConfig(const SbusMultiSourceConfig& cfg) {
        config = cfg;
        log_msg(LOG_INFO, "SBUS config updated: timeout=%u, hysteresis=%u",
                config.timeoutMs, config.hysteresisMs);
    }

    const SbusMultiSourceConfig& getConfig() const {
        return config;
    }

    // Status for Web UI
    SbusSourceType getActiveSource() const { return activeSource; }
    bool isManualMode() const { return config.manualMode; }
    const SbusSourceState& getSourceState(SbusSourceType type) const {
        static SbusSourceState empty;
        if (type >= SBUS_SOURCE_NONE) return empty;
        return sources[type];
    }

    // Statistics
    uint32_t getSwitchCount() const { return switchCount; }
    uint32_t getTimeSinceSwitch() const {
        return lastSwitchTime ? (millis() - lastSwitchTime) : 0;
    }

    static const char* getSourceName(SbusSourceType type) {
        switch(type) {
            case SBUS_SOURCE_LOCAL: return "LOCAL";
            case SBUS_SOURCE_UART: return "UART";
            case SBUS_SOURCE_UDP: return "UDP";
            default: return "NONE";
        }
    }

private:
    // Simple source selection for Phase 9.2 (will be enhanced in 9.3)
    SbusSourceType selectBestSource() {
        // For now, just pick first available source
        // Phase 9.3 will add priorities and quality-based selection

        for (int i = 0; i < 3; i++) {
            SbusSourceType type = (SbusSourceType)config.priorities[i];
            if (type < SBUS_SOURCE_NONE && sources[type].hasData) {
                // Check if not timed out (simple timeout for now)
                uint32_t age = millis() - sources[type].lastFrameTime;
                if (age < config.timeoutMs) {
                    return type;
                }
            }
        }

        return SBUS_SOURCE_NONE;
    }
};

#endif // SBUS_MULTI_SOURCE_H