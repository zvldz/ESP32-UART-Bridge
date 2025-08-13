#ifndef UART_SENDER_H
#define UART_SENDER_H

#include "packet_sender.h"
#include "../uart/uart_interface.h"  // Correct include

class UartSender : public PacketSender {
private:
    UartInterface* uartInterface;  // Using UartInterface
    uint32_t lastSendTime;
    
public:
    UartSender(UartInterface* uart) :  // Takes UartInterface
        PacketSender(20, 8192),
        uartInterface(uart),
        lastSendTime(0) {
        log_msg(LOG_INFO, "UartSender initialized");
    }
    
    void processSendQueue() override {
        while (!packetQueue.empty() && isReady()) {
            QueuedPacket& item = packetQueue.front();
            
            // Check inter-packet gap if specified
            if (item.packet.hints.interPacketGap > 0 && item.sendOffset == 0) {
                uint32_t now = micros();
                if (now - lastSendTime < item.packet.hints.interPacketGap) {
                    break;
                }
            }
            
            // Check available space
            int space_raw = uartInterface->availableForWrite();
            size_t space = (space_raw > 0) ? (size_t)space_raw : 0;
            
            if (space == 0) {
                break;
            }
            
            // Calculate how much to send
            size_t remaining = item.packet.size - item.sendOffset;
            size_t toSend = (remaining < space) ? remaining : space;
            
            if (toSend == 0) {
                break;
            }
            
            // Send what we can
            size_t sent = uartInterface->write(
                item.packet.data + item.sendOffset,
                toSend
            );
            
            if (sent > 0) {
                item.sendOffset += sent;
                
                // Check if packet fully sent
                if (item.sendOffset >= item.packet.size) {
                    totalSent++;
                    currentQueueBytes -= item.packet.size;
                    lastSendTime = micros();
                    
                    // Free packet memory
                    item.packet.free();
                    packetQueue.pop();
                }
            } else {
                break;
            }
        }
    }
    
    bool isReady() const override {
        if (!uartInterface) return false;
        int space = uartInterface->availableForWrite();
        return space > 0;
    }
    
    const char* getName() const override {
        return "UART";
    }
};

#endif // UART_SENDER_H