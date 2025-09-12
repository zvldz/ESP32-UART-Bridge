#ifndef SBUS_HUB_H
#define SBUS_HUB_H

#include "packet_sender.h"
#include "protocol_types.h"
#include "sbus_common.h"
#include "../uart/uart_interface.h"
#include "../logging.h"
#include "../device_stats.h"

// SBUS failsafe timeout configuration
// TODO Phase 5.2: Make configurable via web interface
// Typical values: 50ms (racing), 100ms (standard), 200ms (long range)
const uint32_t SBUS_FAILSAFE_TIMEOUT_US = 100000;  // 100ms standard

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
        lastChannelHash(0) {
        
        // Initialize channels to center (1024 = ~1500us)
        for (int i = 0; i < 16; i++) {
            channels[i] = 1024;
        }
        
        log_msg(LOG_INFO, "SbusHub initialized for device index %d", devIdx);
    }
    
    // Update state from incoming SBUS frame
    bool enqueue(const ParsedPacket& packet) override {
        if (packet.format == DataFormat::FORMAT_SBUS) {
            // Unpack channels from SBUS frame
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
        return false;
    }
    
    // Generate SBUS output at fixed rate (continuous generation)
    void processSendQueue(bool bulkMode) override {
        uint32_t now = micros();
        
        // === TEMPORARY DIAGNOSTIC START ===
        // Missed windows diagnostics
        static uint32_t checkCount = 0;
        static uint32_t windowsMissed = 0;
        checkCount++;
        // === TEMPORARY DIAGNOSTIC END ===
        
        // Generate frame every 14ms (standard SBUS timing)
        if (now - lastOutputTime >= 14000) {
            
            // === TEMPORARY DIAGNOSTIC START ===
            // Check for missed window
            if (now - lastOutputTime > 21000) {  // > 1.5 windows
                windowsMissed++;
            }
            // === TEMPORARY DIAGNOSTIC END ===
            
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
            uint8_t frame[25];
            frame[0] = 0x0F;  // Start byte
            packSbusChannels(channels, frame + 1);
            frame[23] = flags;
            frame[24] = 0x00;  // End byte
            
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
                    
                    // === TEMPORARY DIAGNOSTIC START ===
                    // Log proper statistics every 1000 frames
                    if (framesGenerated % 1000 == 0) {
                        float generatedPercent = (framesGenerated > 0) ? 
                            (100.0f * framesWithoutNewData / framesGenerated) : 0;
                        float unchangedPercent = (framesGenerated > 0) ? 
                            (100.0f * unchangedFrames / framesGenerated) : 0;
                        
                        log_msg(LOG_INFO, "SbusHub stats: Rx=%u Tx=%u Real=%u Gen=%u (%.1f%%) Unchanged=%u (%.1f%%) FS=%u",
                                framesReceived,           // Received from parser
                                framesGenerated,          // Total sent
                                framesWithNewData,        // With real data
                                framesWithoutNewData,     // Artificially generated
                                generatedPercent,         // % artificial
                                unchangedFrames,          // Unchanged channels
                                unchangedPercent,         // % unchanged
                                failsafeEvents);          // Failsafe events
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
        
        // === TEMPORARY DIAGNOSTIC START ===
        // Log timing diagnostics every 1000 checks
        if (checkCount % 1000 == 0) {
            log_msg(LOG_INFO, "Hub timing: checks=%u missed=%u", 
                    checkCount, windowsMissed);
            checkCount = 0;
        }
        // === TEMPORARY DIAGNOSTIC END ===
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
};

#endif // SBUS_HUB_H