#include "uart1_sender.h"
#include "../logging.h"

Uart1Sender::Uart1Sender() : 
    PacketSender(0, 0) {  // CRITICAL: Zero queue to avoid double-buffering!
    txService = Uart1TxService::getInstance();
}

bool Uart1Sender::enqueue(const ParsedPacket& packet) {
    if (!txService) return false;
    
    // Direct pass-through to TX service, no local queuing
    bool result = txService->enqueue(packet.data, packet.size);
    
    if (result) {
        totalSent++;
    } else {
        totalDropped++;
    }
    
    // Free packet immediately since we don't queue it
    const_cast<ParsedPacket&>(packet).free();
    
    return result;
}

void Uart1Sender::processSendQueue(bool bulkMode) {
    // Empty - no local queue to process
    // TX service handles actual sending in uartBridgeTask
}

bool Uart1Sender::isReady() const {
    return txService != nullptr;
}