#ifndef UART_SENDER_H
#define UART_SENDER_H

#include "packet_sender.h"
#include "sbus_router.h"
#include "../uart/uart_interface.h"
#include "../device_types.h"
#include "../types.h"

// Base UART sender class
class UartSender : public PacketSender {
protected:
    UartInterface* uartInterface;  // Using UartInterface
    uint32_t lastSendTime;
    
public:
    UartSender(UartInterface* uart) :
        PacketSender(DEFAULT_MAX_PACKETS, DEFAULT_MAX_BYTES),
        uartInterface(uart),
        lastSendTime(0) {
        log_msg(LOG_DEBUG, "UartSender initialized");
    }

    // Direct send without queue (for fast path - SBUS)
    size_t sendDirect(const uint8_t* data, size_t size) override {
        if (!uartInterface) return 0;

        const uint8_t* sendData = data;
        size_t sendSize = size;

        // Convert SBUS binary to text if TEXT format selected (MAVLink not supported for UART)
        if (sbusOutputFormat == SBUS_FMT_TEXT && size == SBUS_FRAME_SIZE && data[0] == SBUS_START_BYTE) {
            char* buf = SbusRouter::getInstance()->getConvertBuffer();
            if (!buf) return 0;  // Buffer not allocated - skip
            sendSize = sbusFrameToText(data, buf, SBUS_OUTPUT_BUFFER_SIZE);
            if (sendSize == 0) return 0;  // Conversion failed
            sendData = (const uint8_t*)buf;
        }

        return uartInterface->write(sendData, sendSize);
    }

    void processSendQueue(bool bulkMode = false) override {
        // UART sender may ignore bulkMode parameter
        uint32_t now = micros();
        
        // Process packets from simple FIFO queue
        while (isReady() && !packetQueue.empty()) {
            QueuedPacket* item = &packetQueue.front();
            
            // Check inter-packet gap if specified
            if (item->packet.hints.interPacketGap > 0 && item->sendOffset == 0) {
                if (now - lastSendTime < item->packet.hints.interPacketGap) {
                    break;  // Wait for inter-packet gap
                }
            }
            
            // Check available space
            int space_raw = uartInterface->availableForWrite();
            size_t space = (space_raw > 0) ? (size_t)space_raw : 0;
            
            if (space == 0) {
                break;  // UART buffer full
            }
            
            // Calculate how much to send (partial send support)
            size_t remaining = item->packet.size - item->sendOffset;
            size_t toSend = (remaining < space) ? remaining : space;
            
            if (toSend == 0) {
                break;  // Can't make progress
            }
            
            // Send what we can (reuse sendDirect for consistency)
            size_t sent = sendDirect(
                item->packet.data + item->sendOffset,
                toSend
            );
            
            if (sent > 0) {
                item->sendOffset += sent;
                
                // Update activity time
                g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
                
                // Check if packet fully sent
                if (item->sendOffset >= item->packet.size) {
                    totalSent++;
                    lastSendTime = now;
                    currentQueueBytes -= item->packet.size;
                    item->packet.free();
                    packetQueue.pop_front();
                }
            } else {
                break;  // Send failed
            }
        }
    }
    
    bool isReady() const override {
        if (!uartInterface) return false;
        int space = uartInterface->availableForWrite();
        return space > 0;
    }

    const char* getName() const override = 0;
};

// Device2 UART2 sender
class Uart2Sender : public UartSender {
public:
    using UartSender::UartSender;
    const char* getName() const override { return "UART2"; }

    // Override sendDirect to update Device2 statistics
    size_t sendDirect(const uint8_t* data, size_t size) override {
        size_t sent = UartSender::sendDirect(data, size);
        if (sent > 0) {
            g_deviceStats.device2.txBytes.fetch_add(sent, std::memory_order_relaxed);
            g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
        }
        return sent;
    }

    void processSendQueue(bool bulkMode = false) override {
        size_t bytesBefore = currentQueueBytes;
        UartSender::processSendQueue(bulkMode);

        // Update Device2 TX statistics if bytes were sent
        size_t bytesSent = bytesBefore - currentQueueBytes;
        if (bytesSent > 0) {
            g_deviceStats.device2.txBytes.fetch_add(bytesSent, std::memory_order_relaxed);
        }
    }
};

// Device3 UART3 sender
class Uart3Sender : public UartSender {
public:
    using UartSender::UartSender;
    const char* getName() const override { return "UART3"; }

    // Override sendDirect to update Device3 statistics
    size_t sendDirect(const uint8_t* data, size_t size) override {
        size_t sent = UartSender::sendDirect(data, size);
        if (sent > 0) {
            g_deviceStats.device3.txBytes.fetch_add(sent, std::memory_order_relaxed);
            g_deviceStats.lastGlobalActivity.store(millis(), std::memory_order_relaxed);
        }
        return sent;
    }

    void processSendQueue(bool bulkMode = false) override {
        size_t bytesBefore = currentQueueBytes;
        UartSender::processSendQueue(bulkMode);

        // Update Device3 TX statistics if bytes were sent
        size_t bytesSent = bytesBefore - currentQueueBytes;
        if (bytesSent > 0) {
            g_deviceStats.device3.txBytes.fetch_add(bytesSent, std::memory_order_relaxed);
        }
    }
};

#endif // UART_SENDER_H