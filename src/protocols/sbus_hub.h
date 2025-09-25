#ifndef SBUS_HUB_H
#define SBUS_HUB_H

#include "packet_sender.h"
#include "protocol_types.h"
#include "sbus_common.h"
#include "sbus_multi_source.h"
#include "../uart/uart_interface.h"
#include "../logging.h"
#include "../device_stats.h"
#include "../config.h"
#include <Arduino.h>  // For ESP.getFreeHeap()

// External config declaration
extern Config config;

// SBUS failsafe timeout configuration
// TODO Phase 5.2: Make configurable via web interface
// Typical values: 50ms (racing), 100ms (standard), 200ms (long range)
const uint32_t SBUS_FAILSAFE_TIMEOUT_US = 100000;  // 100ms standard
//const uint32_t SBUS_FAILSAFE_TIMEOUT_US = 200000;  // 100ms standard

class SbusHub : public PacketSender {
private:
    // State storage
    uint16_t channels[16];
    uint8_t flags;
    uint32_t lastInputTime;
    uint32_t lastOutputTime;
    UartInterface* outputUart;
    int deviceIndex;
    
    // Control flags
    bool inFailsafe;                    // Track failsafe state
    bool hadNewDataSinceLastSend;       // NEW: Flag for tracking fresh data
    
    // Statistics - Proper counters
    uint32_t framesReceived;            // Received from SBUS parser
    uint32_t framesGenerated;           // Total sent by Hub
    uint32_t framesWithNewData;         // NEW: Sent with real data
    uint32_t framesWithoutNewData;      // NEW: Sent without new data (artificial)
    uint32_t unchangedFrames;           // NEW: Frames with unchanged channels
    uint32_t failsafeEvents;            // Count failsafe activations
    
    // Diagnostics
    uint32_t lastChannelHash;           // Hash for detecting channel changes

    // Multi-source manager (for SBUS_OUT devices only)
    SbusMultiSource* multiSource;

    // Calculate simple hash of channel values for change detection
    uint32_t calculateChannelHash() {
        uint32_t hash = 0;
        for (int i = 0; i < 16; i++) {
            hash = hash * 31 + channels[i];
        }
        return hash;
    }
    
public:
    SbusHub(UartInterface* uart, int devIdx) : 
        PacketSender(0, 0),  // No queue needed
        outputUart(uart),
        deviceIndex(devIdx),
        lastInputTime(0),
        lastOutputTime(0),
        flags(0),
        inFailsafe(false),
        hadNewDataSinceLastSend(false),
        framesReceived(0),
        framesGenerated(0),
        framesWithNewData(0),
        framesWithoutNewData(0),
        unchangedFrames(0),
        failsafeEvents(0),
        lastChannelHash(0),
        multiSource(nullptr) {
        
        // Initialize channels to center (1024 = ~1500us)
        for (int i = 0; i < 16; i++) {
            channels[i] = 1024;
        }

        // Create multi-source manager for SBUS_OUT devices only
        if ((devIdx == IDX_DEVICE2_UART2 && config.device2.role == D2_SBUS_OUT) ||
            (devIdx == IDX_DEVICE3 && config.device3.role == D3_SBUS_OUT)) {
            // --- TEMPORARY DIAGNOSTICS START ---
            log_msg(LOG_INFO, "[SBUS] Before MultiSource, free: %d", ESP.getFreeHeap());
            // --- TEMPORARY DIAGNOSTICS END ---
            multiSource = new SbusMultiSource();
            // --- TEMPORARY DIAGNOSTICS START ---
            log_msg(LOG_INFO, "[SBUS] After MultiSource, free: %d", ESP.getFreeHeap());
            // --- TEMPORARY DIAGNOSTICS END ---

            // Load saved configuration
            SbusMultiSourceConfig msConfig;
            msConfig.forcedSource = (SbusSourceType)config.sbusSettings.forcedSource;
            msConfig.manualMode = config.sbusSettings.manualMode;
            msConfig.timeoutMs = config.sbusSettings.timeoutMs;
            msConfig.hysteresisMs = config.sbusSettings.hysteresisMs;
            memcpy(msConfig.priorities, config.sbusSettings.priorities, sizeof(msConfig.priorities));

            multiSource->setConfig(msConfig);

            log_msg(LOG_INFO, "SbusHub with MultiSource for device %d: mode=%s, source=%s",
                    devIdx, msConfig.manualMode ? "manual" : "auto",
                    SbusMultiSource::getSourceName(msConfig.forcedSource));
        } else {
            log_msg(LOG_INFO, "SbusHub initialized for device index %d", devIdx);
        }
    }
    
    // Update state from incoming SBUS frame
    bool enqueue(const ParsedPacket& packet) override {
        if (packet.format == DataFormat::FORMAT_SBUS) {
            // Determine source based on physical interface and device roles
            SbusSourceType sourceType = SBUS_SOURCE_LOCAL;

            if (packet.physicalInterface == PHYS_UART2) {
                // Device2 может быть SBUS_IN или UART2
                if (config.device2.role == D2_SBUS_IN) {
                    sourceType = SBUS_SOURCE_LOCAL;  // Physical SBUS input
                } else if (config.device2.role == D2_UART2) {
                    sourceType = SBUS_SOURCE_UART;   // UART transport
                }
            } else if (packet.physicalInterface == PHYS_UART3) {
                // Device3 может быть SBUS_IN или UART3
                if (config.device3.role == D3_SBUS_IN) {
                    sourceType = SBUS_SOURCE_LOCAL;  // Physical SBUS input
                } else if (config.device3.role == D3_UART3_BRIDGE) {
                    sourceType = SBUS_SOURCE_UART;   // UART transport
                }
            } else if (packet.physicalInterface == PHYS_UDP) {
                sourceType = SBUS_SOURCE_UDP;        // UDP/WiFi transport
            } else {
                // Unknown source - log for debugging
                log_msg(LOG_WARNING, "Unknown packet source: phys=%d", packet.physicalInterface);
                return false;
            }

            // If we have MultiSource, route through it
            if (multiSource) {
                uint16_t tempChannels[16];
                unpackSbusChannels(packet.data + 1, tempChannels);
                uint8_t tempFlags = packet.data[23];

                multiSource->updateSource(sourceType, tempChannels, tempFlags);

                // Update basic stats
                framesReceived++;

                return true;
            } else {
                // Original behavior for non-MultiSource hubs
                unpackSbusChannels(packet.data + 1, channels);
                flags = packet.data[23];

                // Update timing
                lastInputTime = micros();
                framesReceived++;

                // Mark that we have new data
                hadNewDataSinceLastSend = true;

                // Clear failsafe if we were in failsafe mode
                if (inFailsafe) {
                    log_msg(LOG_INFO, "SbusHub: Signal restored after failsafe");
                    inFailsafe = false;
                    flags &= ~0x10;  // Clear failsafe flag
                }

                // === TEMPORARY DIAGNOSTIC START ===
                // Log every 100th frame for monitoring
                if (framesReceived % 100 == 0) {
                    log_msg(LOG_DEBUG, "SbusHub: Received frame %u, Ch1=%u Ch2=%u",
                            framesReceived, channels[0], channels[1]);
                }
                // === TEMPORARY DIAGNOSTIC END ===

                return true;
            }
        }
        return false;
    }
    
    // Generate SBUS output at fixed rate (continuous generation)
    void processSendQueue(bool bulkMode) override {
        uint32_t now = micros();
        
        
        // Generate frame every 14ms (standard SBUS timing)
        if (now - lastOutputTime >= 14000) {
            uint8_t frame[25];

            // Get data from MultiSource if available
            if (multiSource) {
                uint16_t msChannels[16];
                uint8_t msFlags;

                if (multiSource->getActiveChannels(msChannels, &msFlags)) {
                    // Got data from MultiSource
                    frame[0] = 0x0F;
                    packSbusChannels(msChannels, frame + 1);
                    frame[23] = msFlags;
                    frame[24] = 0x00;
                    inFailsafe = false;
                } else {
                    // No valid source - generate failsafe
                    if (!inFailsafe) {
                        inFailsafe = true;
                        failsafeEvents++;
                        log_msg(LOG_WARNING, "SbusHub: Failsafe - no valid source");
                    }

                    // Generate failsafe frame
                    for (int i = 0; i < 16; i++) {
                        channels[i] = 1024; // Center position
                    }
                    frame[0] = 0x0F;
                    packSbusChannels(channels, frame + 1);
                    frame[23] = 0x10; // Failsafe flag
                    frame[24] = 0x00;
                }
            } else {
                // Original behavior (no MultiSource)
                // Check for failsafe condition (no data for 100ms)
                if (lastInputTime > 0 && (now - lastInputTime > SBUS_FAILSAFE_TIMEOUT_US)) {
                    if (!inFailsafe) {
                        // Enter failsafe mode
                        inFailsafe = true;
                        flags |= 0x10;  // Set failsafe flag
                        failsafeEvents++;

                        // === TEMPORARY DIAGNOSTIC START ===
                        log_msg(LOG_WARNING, "SbusHub: Failsafe activated (no data for %ums)",
                                SBUS_FAILSAFE_TIMEOUT_US / 1000);
                        // === TEMPORARY DIAGNOSTIC END ===
                    }
                } else if (inFailsafe) {
                    // Already handled in enqueue when new data arrives
                }

                // Generate SBUS frame (ALWAYS, regardless of new data availability)
                frame[0] = 0x0F;  // Start byte
                packSbusChannels(channels, frame + 1);
                frame[23] = flags;
                frame[24] = 0x00;  // End byte
            }
            
            // Send to UART
            if (outputUart) {
                size_t written = outputUart->write(frame, 25);
                if (written == 25) {
                    totalSent++;
                    framesGenerated++;
                    
                    // === TEMPORARY DIAGNOSTIC START ===
                    // Proper statistics tracking
                    if (hadNewDataSinceLastSend) {
                        framesWithNewData++;
                        hadNewDataSinceLastSend = false;  // Reset flag
                    } else {
                        framesWithoutNewData++;  // Artificially generated
                    }
                    
                    // Check for channel changes (for diagnostics)
                    uint32_t currentHash = calculateChannelHash();
                    if (currentHash == lastChannelHash && framesGenerated > 1) {
                        unchangedFrames++;  // Channels unchanged (sticks not moving)
                    }
                    lastChannelHash = currentHash;
                    // === TEMPORARY DIAGNOSTIC END ===
                    
                    // Update device statistics
                    if (deviceIndex == IDX_DEVICE3) {
                        g_deviceStats.device3.txBytes.fetch_add(25, std::memory_order_relaxed);
                    } else if (deviceIndex == IDX_DEVICE2_UART2) {
                        g_deviceStats.device2.txBytes.fetch_add(25, std::memory_order_relaxed);
                    }
                    
                    // === TEMPORARY DIAGNOSTIC START === (Remove after SBUS stabilization)
                    // Reset statistics every 30 seconds for cleaner monitoring
                    static uint32_t lastResetTime = millis();
                    if (millis() - lastResetTime > 30000) {
                        float genPercent = (framesGenerated > 0) ? (100.0f * framesWithoutNewData / framesGenerated) : 0;

                        log_msg(LOG_INFO, "SbusHub 30s: Rx=%u Tx=%u Real=%u Gen=%u (%.1f%%)",
                                framesReceived, framesGenerated, framesWithNewData, framesWithoutNewData, genPercent);

                        // Reset all counters for next interval
                        framesReceived = 0;
                        framesGenerated = 0;
                        framesWithNewData = 0;
                        framesWithoutNewData = 0;

                        lastResetTime = millis();
                    }
                    // === TEMPORARY DIAGNOSTIC END ===
                } else {
                    totalDropped++;
                    // === TEMPORARY DIAGNOSTIC START ===
                    log_msg(LOG_WARNING, "SbusHub: Failed to write frame (wrote %zu/25)", written);
                    // === TEMPORARY DIAGNOSTIC END ===
                }
            }
            
            lastOutputTime = now;
        }
        
    }
    
    bool isReady() const override {
        return outputUart != nullptr;
    }
    
    const char* getName() const override {
        return "SBUS_Hub";
    }
    
    // Get statistics for monitoring
    uint32_t getFramesReceived() const { return framesReceived; }
    uint32_t getFramesGenerated() const { return framesGenerated; }
    uint32_t getFramesWithNewData() const { return framesWithNewData; }
    uint32_t getFramesWithoutNewData() const { return framesWithoutNewData; }
    uint32_t getUnchangedFrames() const { return unchangedFrames; }
    uint32_t getFailsafeEvents() const { return failsafeEvents; }
    bool isInFailsafe() const { return inFailsafe; }

    // Destructor
    ~SbusHub() {
        if (multiSource) {
            delete multiSource;
        }
    }

    // MultiSource access (for Web API)
    SbusMultiSource* getMultiSource() { return multiSource; }
};

#endif // SBUS_HUB_H