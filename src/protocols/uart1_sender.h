#pragma once
#include "packet_sender.h"
#include "../uart/uart1_tx_service.h"

// Thin wrapper - no local queue, direct pass-through to TX service
class Uart1Sender : public PacketSender {
private:
    Uart1TxService* txService;
    
public:
    Uart1Sender();

    // Direct send NOT supported for UART1 - always use TX service queue
    size_t sendDirect(const uint8_t* data, size_t size) override;

    // Override to bypass local queue
    bool enqueue(const ParsedPacket& packet) override;
    void processSendQueue(bool bulkMode = false) override;
    bool isReady() const override;
    const char* getName() const override { return "Device1"; }
};