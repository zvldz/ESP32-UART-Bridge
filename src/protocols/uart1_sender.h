#pragma once
#include "packet_sender.h"
#include "../uart/uart1_tx_service.h"

// Thin wrapper - no local queue, direct pass-through to TX service
class Uart1Sender : public PacketSender {
private:
    Uart1TxService* txService;
    
public:
    Uart1Sender();
    
    // Override to bypass local queue
    bool enqueue(const ParsedPacket& packet) override;
    void processSendQueue(bool bulkMode = false) override;
    bool isReady() const override;
    const char* getName() const override { return "UART1"; }
};