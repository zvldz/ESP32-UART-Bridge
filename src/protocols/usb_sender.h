#ifndef USB_SENDER_H
#define USB_SENDER_H

#include "packet_sender.h"
#include "../usb/usb_interface.h"

class UsbSender : public PacketSender {
private:
    UsbInterface* usbInterface;
    uint32_t lastSendAttempt;
    uint32_t backoffDelay;  // Exponential backoff for USB congestion
    
public:
    UsbSender(UsbInterface* usb, unsigned long* txCounter = nullptr) : 
        PacketSender(30, 10240, txCounter),  // Pass TX counter to base class
        usbInterface(usb),
        lastSendAttempt(0),
        backoffDelay(0) {
        log_msg(LOG_INFO, "UsbSender initialized");
    }
    
    void processSendQueue() override {
        // Backoff if USB was congested
        if (backoffDelay > 0) {
            uint32_t now = micros();
            if (now - lastSendAttempt < backoffDelay) {
                return;  // Still in backoff period
            }
            backoffDelay = 0;  // Reset backoff
        }
        
        while (!packetQueue.empty() && isReady()) {
            QueuedPacket& item = packetQueue.front();
            
            // Check available space (handle negative values)
            int space_raw = usbInterface->availableForWrite();
            size_t space = (space_raw > 0) ? (size_t)space_raw : 0;
            
            if (space == 0) {
                // USB full - apply exponential backoff
                lastSendAttempt = micros();
                backoffDelay = (backoffDelay == 0) ? 1000U : (backoffDelay * 2);
                if (backoffDelay > 5000U) backoffDelay = 5000U;
                break;  // Can't send now
            }
            
            // Calculate how much to send (partial send support)
            size_t remaining = item.packet.size - item.sendOffset;
            size_t toSend = min((size_t)remaining, (size_t)space);
            
            if (toSend == 0) {
                break;  // Can't make progress
            }
            
            // Send what we can
            size_t sent = usbInterface->write(
                item.packet.data + item.sendOffset, 
                toSend
            );
            
            if (sent > 0) {
                item.sendOffset += sent;
                backoffDelay = 0;  // Reset backoff on successful send
                
                // Update global TX counter
                if (globalTxBytesCounter) {
                    *globalTxBytesCounter += sent;
                }
                
                // Check if packet fully sent
                if (item.sendOffset >= item.packet.size) {
                    totalSent++;
                    currentQueueBytes -= item.packet.size;
                    
                    // CRITICAL: Free packet memory
                    item.packet.free();
                    packetQueue.pop();
                }
            } else {
                // Send failed - apply backoff
                lastSendAttempt = micros();
                backoffDelay = (backoffDelay == 0) ? 1000U : (backoffDelay * 2);
                if (backoffDelay > 5000U) backoffDelay = 5000U;
                break;
            }
        }
    }
    
    bool isReady() const override {
        if (!usbInterface) return false;
        int space = usbInterface->availableForWrite();
        return space > 0;
    }
    
    const char* getName() const override {
        return "USB";
    }
};

#endif // USB_SENDER_H