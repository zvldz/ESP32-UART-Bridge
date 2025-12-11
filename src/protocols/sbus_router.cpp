#include "sbus_router.h"
#include "../device_stats.h"
#include "packet_sender.h"

// Static instance initialization
SbusRouter* SbusRouter::instance = nullptr;

SbusRouter::SbusRouter() :
    activeSource(SBUS_SOURCE_DEVICE1),  // Default to Device1
    timingKeeperEnabled(false),
    switchDelayMs(500),
    framesRouted(0),
    framesRepeated(0),
    sourceSwitches(0),
    currentMode(MODE_AUTO),
    currentState(State::OK),
    previousSourceId(SBUS_SOURCE_NONE),
    recoveryFrameCount(0),
    lastRepeatMs(0) {

    memset(sources, 0, sizeof(sources));
    memset(lastValidFrame, 0, sizeof(lastValidFrame));
    memset(sourceConfigured, 0, sizeof(sourceConfigured));

    // Fixed priorities: Device1 > Device2 > Device3 > UDP
    priorities[0] = SBUS_SOURCE_DEVICE1;
    priorities[1] = SBUS_SOURCE_DEVICE2;
    priorities[2] = SBUS_SOURCE_DEVICE3;
    priorities[3] = SBUS_SOURCE_UDP;

    log_msg(LOG_INFO, "SbusRouter singleton created");
}

bool SbusRouter::routeFrame(const uint8_t* frame, uint8_t sourceId) {
    // Called directly from SbusFastParser, bypasses packet queue

    // Validation
    if (!frame || sourceId >= 4) return false;
    if (frame[0] != 0x0F) return false;
    if (!sourceConfigured[sourceId]) return false;

    // Update source state
    SourceState& src = sources[sourceId];
    src.hasData = true;
    src.lastFrameTime = millis();
    src.framesReceived++;

    // Check source failsafe bit
    bool sourceInFailsafe = (frame[23] & 0x08) != 0;
    bool frameLost = (frame[23] & 0x04) != 0;

    if (sourceInFailsafe || frameLost) {
        // Source itself reports failsafe - reduce trust
        log_msg(LOG_DEBUG, "Source %d reports failsafe/lost", sourceId);
    }

    src.hasFailsafe = sourceInFailsafe;

    // Count configured sources for single-source optimization
    int configuredCount = 0;
    for (int i = 0; i < 4; i++) {
        if (sourceConfigured[i]) configuredCount++;
    }

    // Single-source mode: skip all failover logic
    if (configuredCount == 1) {
        // Only one source - always use it, no switching logic
        // We are a transport - pass through all data including failsafe bits
        activeSource = sourceId;
    } else {
        // Multi-source mode: intelligent failover
        if (currentMode == MODE_MANUAL) {
            // Manual mode - stay on forced source (set via Web UI)
            // activeSource already set by setManualSource()
        } else {
            // Auto mode - intelligent failover
            selectBestSource();
        }

        // If this is active source in failsafe - try to switch
        if (sourceId == activeSource && sourceInFailsafe) {
            selectBestSource();
        }

        // Update global failsafe state based on active source
        updateFailsafeState();
    }

    // Save frame from UDP source for Timing Keeper (regardless of active source)
    // When WiFi lags and activeSource is UDP, tick() will repeat this frame
    if (sourceId == SBUS_SOURCE_UDP) {
        memcpy(lastValidFrame, frame, 25);
    }

    // Route frame if this is active source
    if (sourceId == activeSource) {
        writeToOutputs(frame);
        return true;
    }

    return false;
}

void SbusRouter::registerOutput(PacketSender* sender) {
    if (!sender) {
        log_msg(LOG_ERROR, "Attempted to register null sender output");
        return;
    }
    outputs.push_back(sender);
    log_msg(LOG_INFO, "SBUS output registered: %s (total outputs: %zu)", sender->getName(), outputs.size());
}

void SbusRouter::registerSource(uint8_t sourceId, uint8_t priority) {
    if (sourceId >= 4) {
        log_msg(LOG_ERROR, "Invalid source ID: %d", sourceId);
        return;
    }
    sourceConfigured[sourceId] = true;
    priorities[priority] = sourceId;
    log_msg(LOG_INFO, "SBUS source %d registered with priority %d", sourceId, priority);
}

void SbusRouter::writeToOutputs(const uint8_t* frame) {
    if (outputs.empty()) {
        // No outputs registered - this is normal during init
        return;
    }

    // sendDirect() bypasses queue, stats updated inside sender
    for (auto* sender : outputs) {
        sender->sendDirect(frame, 25);
    }

    framesRouted++;
}

// Timeout for UDP source before stopping frame repeat (let FC detect signal loss)
// TODO: Make configurable via Web UI
static constexpr uint32_t UDP_SOURCE_TIMEOUT_MS = 1000;

void SbusRouter::tick() {
    // Only repeat for UDP source with Timing Keeper enabled
    if (activeSource != SBUS_SOURCE_UDP || !timingKeeperEnabled) {
        return;
    }

    // Stop repeating if UDP source lost - let FC detect signal loss and activate failsafe
    uint32_t age = millis() - sources[SBUS_SOURCE_UDP].lastFrameTime;
    if (age > UDP_SOURCE_TIMEOUT_MS) {
        return;
    }

    uint32_t now = millis();
    if (now - lastRepeatMs >= 20) {
        writeToOutputs(lastValidFrame);
        framesRepeated++;
        lastRepeatMs = now;
    }
}

uint8_t SbusRouter::getSourceQuality(uint8_t sourceId) const {
    if (sourceId >= 4 || !sourceConfigured[sourceId]) return 0;

    uint32_t age = millis() - sources[sourceId].lastFrameTime;

    if (age > 100) return 0;    // No signal
    if (age > 50)  return 25;   // Weak
    if (age > 30)  return 50;   // Medium
    if (age > 20)  return 75;   // Good
    return 100;                 // Excellent
}

uint8_t SbusRouter::getSourcePriority(uint8_t sourceId) const {
    for (int i = 0; i < 4; i++) {
        if (priorities[i] == sourceId) return i;
    }
    return 255;  // Not found
}

void SbusRouter::updateFailsafeState() {
    uint32_t age = millis() - sources[activeSource].lastFrameTime;
    State oldState = currentState;

    switch (currentState) {
        case State::OK:
            if (age > 40) {
                currentState = State::HOLD;
                recoveryFrameCount = 0;
                log_msg(LOG_WARNING, "SBUS Router: OK → HOLD (active source %d, age=%ums)",
                        activeSource, age);
            }
            break;

        case State::HOLD:
            if (age < 40) {
                // Recovery
                currentState = State::OK;
                recoveryFrameCount = 0;
                log_msg(LOG_INFO, "SBUS Router: HOLD → OK");
            } else if (age > 100) {
                // Degradation to failsafe
                currentState = State::FAILSAFE;
                recoveryFrameCount = 0;
                lastValidFrame[23] |= 0x08;  // Set failsafe bit
                lastValidFrame[23] |= 0x04;  // Set frame lost bit
                log_msg(LOG_ERROR, "SBUS Router: HOLD → FAILSAFE (age=%ums)", age);
            }
            break;

        case State::FAILSAFE:
            if (age < 40) {
                // Require 2 valid frames for recovery
                if (++recoveryFrameCount >= 2) {
                    currentState = State::OK;
                    recoveryFrameCount = 0;
                    lastValidFrame[23] &= ~0x0C;  // Clear failsafe and frame lost bits
                    log_msg(LOG_INFO, "SBUS Router: FAILSAFE → OK (after %d valid frames)",
                            recoveryFrameCount);
                }
            } else {
                // Still in failsafe - reset recovery counter
                recoveryFrameCount = 0;
            }
            break;
    }

    // Log state changes
    if (oldState != currentState) {
        log_msg(LOG_INFO, "SBUS Router state changed: %d → %d", oldState, currentState);
    }
}

uint8_t SbusRouter::selectBestSource() {
    if (currentMode == MODE_MANUAL) {
        return activeSource;  // Manual mode - don't change
    }

    uint8_t bestSource = SBUS_SOURCE_NONE;
    uint8_t bestPriority = 255;
    uint8_t bestQuality = 0;

    // Find best source by priority, then quality
    for (int i = 0; i < 4; i++) {
        uint8_t src = priorities[i];
        if (!sourceConfigured[src]) continue;

        uint8_t quality = getSourceQuality(src);

        // Skip sources with failsafe flag
        if (sources[src].hasFailsafe) {
            quality = quality / 2;  // Reduce trust
        }

        if (quality > 25 && i < bestPriority) {
            bestSource = src;
            bestPriority = i;
            bestQuality = quality;
        }
    }

    // Switch if found better source
    if (bestSource != SBUS_SOURCE_NONE && bestSource != activeSource) {
        // Anti-flapping protection
        if (bestSource == previousSourceId &&
            millis() - lastSwitchMs < switchDelayMs) {
            // Rate-limited logging to avoid spam during startup
            static uint32_t lastAntiFlapLog = 0;
            if (millis() - lastAntiFlapLog > 5000) {  // Log once per 5 seconds
                log_msg(LOG_DEBUG, "Anti-flap: blocking return to source %d", bestSource);
                lastAntiFlapLog = millis();
            }
            return activeSource;
        }

        // Switch source
        previousSourceId = activeSource;
        activeSource = bestSource;
        lastSwitchMs = millis();
        sourceSwitches++;
        recoveryFrameCount = 0;  // Reset recovery counter

        log_msg(LOG_INFO, "SBUS source switch: %d → %d (quality %d%% → %d%%, total switches: %u)",
                previousSourceId, activeSource,
                getSourceQuality(previousSourceId), bestQuality, sourceSwitches);
    }

    return activeSource;
}