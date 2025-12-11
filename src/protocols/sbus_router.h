#ifndef SBUS_ROUTER_H
#define SBUS_ROUTER_H

#include "sbus_common.h"
#include "../types.h"
#include "../logging.h"
#include <stdint.h>
#include <vector>

// Forward declarations
class PacketSender;

// Source identifiers
enum SbusSourceType {
    SBUS_SOURCE_DEVICE1 = 0, // Device1 SBUS input (GPIO4)
    SBUS_SOURCE_DEVICE2 = 1, // Device2 SBUS input (GPIO8)
    SBUS_SOURCE_DEVICE3 = 2, // Device3 SBUS input (GPIO6/7)
    SBUS_SOURCE_UDP = 3,     // UDP/WiFi from network (Device4)
    SBUS_SOURCE_NONE = 4     // No source/invalid
};

class SbusRouter {
public:
    // State machine (must be public for Web API)
    enum State { OK = 0, HOLD = 1, FAILSAFE = 2 };

    // Mode control (must be public for Web API)
    enum Mode { MODE_AUTO = 0, MODE_MANUAL = 1 };

private:
    // Minimal state per source
    struct SourceState {
        bool hasData;
        uint32_t lastFrameTime;
        uint32_t framesReceived;
        bool hasFailsafe;        // Source reported failsafe

        // Simple valid/stale check (no complex quality)
        bool isValid() const {
            if (!hasData) return false;
            uint32_t age = millis() - lastFrameTime;
            return (age < 100);  // Valid if < 100ms old
        }
    };

    SourceState sources[4];      // DEVICE1, DEVICE2, DEVICE3, UDP
    uint8_t activeSource;        // Current active source
    uint8_t lastValidFrame[25];  // Last valid frame for repeating

    // Auto-failover config
    uint8_t priorities[4];       // Source priorities for auto mode
    uint32_t switchDelayMs;      // Delay before switching sources

    bool timingKeeperEnabled;

    // Statistics
    uint32_t framesRouted;
    uint32_t framesRepeated;
    uint32_t sourceSwitches;

    // Singleton pattern
    static SbusRouter* instance;
    SbusRouter();  // Private constructor

    // Delete copy constructor and assignment
    SbusRouter(const SbusRouter&) = delete;
    SbusRouter& operator=(const SbusRouter&) = delete;

    // Output registration (unified - can be UART, UDP, USB senders)
    std::vector<PacketSender*> outputs;

    // Source configuration
    bool sourceConfigured[4] = {false, false, false, false};

    // Mode control
    Mode currentMode = MODE_AUTO;
    State currentState = State::OK;

    // Anti-flapping
    uint8_t previousSourceId = SBUS_SOURCE_NONE;
    uint32_t lastSwitchMs = 0;

    // Recovery from failsafe
    uint8_t recoveryFrameCount = 0;

    // Timing keeper
    uint32_t lastRepeatMs = 0;

public:
    // Singleton accessor
    static SbusRouter* getInstance() {
        if (!instance) {
            instance = new SbusRouter();
        }
        return instance;
    }

    // Fast routing without parsing channels
    bool routeFrame(const uint8_t* frame, uint8_t sourceId);

    // Output registration (unified interface for all sender types)
    void registerOutput(PacketSender* sender);

    // Source registration with priority
    void registerSource(uint8_t sourceId, uint8_t priority);

    // Mode control
    void setMode(Mode mode) { currentMode = mode; }
    Mode getMode() const { return currentMode; }

    // Manual source control
    void setManualSource(uint8_t source) {
        if (source >= 4) {
            log_msg(LOG_ERROR, "Invalid manual source: %d", source);
            return;
        }
        currentMode = MODE_MANUAL;
        activeSource = source;
        log_msg(LOG_INFO, "Manual source set to %d", source);
    }

    // Timing keeper control
    void setTimingKeeper(bool enabled) {
        timingKeeperEnabled = enabled;
    }

    // Active source query
    uint8_t getActiveSource() const {
        return activeSource;
    }

    // State queries for Web UI
    bool isSourceConfigured(uint8_t sourceId) const {
        return (sourceId < 4) ? sourceConfigured[sourceId] : false;
    }
    uint8_t getSourceQuality(uint8_t sourceId) const;
    uint8_t getSourcePriority(uint8_t sourceId) const;
    State getState() const { return currentState; }

    // Source state queries for Web UI
    bool getSourceHasData(uint8_t sourceId) const {
        return (sourceId < 4) ? sources[sourceId].hasData : false;
    }
    bool getSourceIsValid(uint8_t sourceId) const {
        return (sourceId < 4) ? sources[sourceId].isValid() : false;
    }
    bool getSourceHasFailsafe(uint8_t sourceId) const {
        return (sourceId < 4) ? sources[sourceId].hasFailsafe : false;
    }

    // Statistics
    uint32_t getFramesRouted() const { return framesRouted; }
    uint32_t getRepeatedFrames() const { return framesRepeated; }

    // Timing keeper tick (called from TaskScheduler)
    void tick();

    // Write to all registered outputs
    void writeToOutputs(const uint8_t* frame);

    // Source selection
    uint8_t selectBestSource();

    // Failsafe state management
    void updateFailsafeState();
};

#endif