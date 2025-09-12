#ifndef SBUS_HUB_H
#define SBUS_HUB_H

#include "packet_sender.h"
#include "protocol_types.h"  // For FORMAT_SBUS_FRAME
#include "sbus_common.h"
#include "../uart/uart_interface.h"
#include "../logging.h"
#include "../device_stats.h"  // For device statistics

class SbusHub : public PacketSender {
private:
    // State storage instead of queue
    uint16_t channels[16];
    uint8_t flags;
    uint32_t lastInputTime;
    uint32_t lastOutputTime;
    UartInterface* outputUart;
    int deviceIndex;  // For updating correct device statistics
    
    // Statistics
    uint32_t framesReceived;
    uint32_t framesGenerated;
    
public:
    SbusHub(UartInterface* uart, int devIdx) : 
        PacketSender(0, 0),  // No queue needed
        outputUart(uart),
        deviceIndex(devIdx),
        lastInputTime(0),
        lastOutputTime(0),
        flags(0),
        framesReceived(0),
        framesGenerated(0) {
        // Initialize channels to center (1024 = ~1500us)
        for (int i = 0; i < 16; i++) {
            channels[i] = 1024;
        }
    }
    
    // Update state from incoming SBUS frame
    bool enqueue(const ParsedPacket& packet) override {
        if (packet.format == DataFormat::FORMAT_SBUS) {
            // Unpack channels from SBUS frame
            unpackSbusChannels(packet.data + 1, channels);
            flags = packet.data[23];
            lastInputTime = micros();
            framesReceived++;
            
            // Note: We don't queue the packet, just update state
            // The packet will be freed by the caller
            return true;
        }
        return false;
    }
    
    // Generate SBUS output at fixed rate
    void processSendQueue(bool bulkMode) override {
        uint32_t now = micros();
        
        // SBUS requires 14ms update rate (71.43 FPS)
        if (now - lastOutputTime >= 14000) {
            // Check if we have fresh data (failsafe after 100ms)
            if (lastInputTime > 0 && (now - lastInputTime > 100000)) {
                // Set failsafe flag if input is stale
                flags |= 0x10;  // Failsafe bit
            }
            
            // Generate SBUS frame
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
                    
                    // Update device statistics
                    if (deviceIndex == IDX_DEVICE3) {
                        g_deviceStats.device3.txBytes.fetch_add(25, std::memory_order_relaxed);
                    } else if (deviceIndex == IDX_DEVICE2_UART2) {
                        g_deviceStats.device2.txBytes.fetch_add(25, std::memory_order_relaxed);
                    }
                } else {
                    totalDropped++;
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
    
    // Future expansion point: add second input
    // void updateFromSecondaryInput(ParsedPacket* packet);
    
    // Future expansion point: network input
    // void updateFromNetwork(uint8_t* data, size_t len);
};

#endif // SBUS_HUB_H