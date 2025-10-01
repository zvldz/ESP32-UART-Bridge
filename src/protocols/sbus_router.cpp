#include "sbus_router.h"
#include "../uart/uart_interface.h"
#include "../device_stats.h"

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

    // Fixed priorities: Device1 > Device2 > UDP
    priorities[0] = SBUS_SOURCE_DEVICE1;
    priorities[1] = SBUS_SOURCE_DEVICE2;
    priorities[2] = SBUS_SOURCE_UDP;

    log_msg(LOG_INFO, "SbusRouter singleton created");
}

bool SbusRouter::routeFrame(const uint8_t* frame, uint8_t sourceId) {
    // FAST PATH: This is called directly from SbusFastParser::tryFastProcess()
    // bypassing normal packet queue/distribution system used by other protocols

    // Validation
    if (!frame || sourceId >= 3) return false;
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

    // Save last valid frame (preserve failsafe flag for now)
    memcpy(lastValidFrame, frame, 25);

    // Count configured sources for single-source optimization
    int configuredCount = 0;
    for (int i = 0; i < 3; i++) {
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

    // Route frame if this is active source
    if (sourceId == activeSource) {
        writeToOutputs(lastValidFrame);
        return true;
    }

    return false;
}

void SbusRouter::registerOutput(UartInterface* uart) {
    if (!uart) {
        log_msg(LOG_ERROR, "Attempted to register null UART output");
        return;
    }
    outputs.push_back(uart);
    log_msg(LOG_INFO, "SBUS output registered, total outputs: %zu", outputs.size());
}

void SbusRouter::registerSource(uint8_t sourceId, uint8_t priority) {
    if (sourceId >= 3) {
        log_msg(LOG_ERROR, "Invalid source ID: %d", sourceId);
        return;
    }
    sourceConfigured[sourceId] = true;
    priorities[priority] = sourceId;
    log_msg(LOG_INFO, "SBUS source %d registered with priority %d", sourceId, priority);
}

void SbusRouter::writeToOutputs(uint8_t* frame) {
    if (outputs.empty()) {
        // No outputs registered - this is normal during init
        return;
    }

    // FAST PATH: Direct UART write, no packet queues
    // This is different from other protocols that use sender queues

    // Compare pointers to identify which device for statistics
    extern UartInterface* device2Serial;
    extern UartInterface* device3Serial;

    for (auto* uart : outputs) {
        size_t written = uart->write(frame, 25);

        // Update device statistics (same as in UartSender)
        if (written > 0) {
            if (uart == device2Serial) {
                g_deviceStats.device2.txBytes.fetch_add(written, std::memory_order_relaxed);
            } else if (uart == device3Serial) {
                g_deviceStats.device3.txBytes.fetch_add(written, std::memory_order_relaxed);
            }
        }
    }

    framesRouted++;
}

void SbusRouter::tick() {
    // Only repeat for UDP source with Timing Keeper enabled
    if (activeSource != SBUS_SOURCE_UDP || !timingKeeperEnabled) {
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
    if (sourceId >= 3 || !sourceConfigured[sourceId]) return 0;

    uint32_t age = millis() - sources[sourceId].lastFrameTime;

    if (age > 100) return 0;    // No signal
    if (age > 50)  return 25;   // Weak
    if (age > 30)  return 50;   // Medium
    if (age > 20)  return 75;   // Good
    return 100;                 // Excellent
}

uint8_t SbusRouter::getSourcePriority(uint8_t sourceId) const {
    for (int i = 0; i < 3; i++) {
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
    for (int i = 0; i < 3; i++) {
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
            log_msg(LOG_DEBUG, "Anti-flap: blocking return to source %d", bestSource);
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