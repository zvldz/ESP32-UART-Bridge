#include "uart1_sender.h"
#include "../logging.h"
#include "protocol_types.h"

Uart1Sender::Uart1Sender() : 
    PacketSender(0, 0) {  // CRITICAL: Zero queue to avoid double-buffering!
    txService = Uart1TxService::getInstance();
}

bool Uart1Sender::enqueue(const ParsedPacket& packet) {
    if (!txService) return false;
    
    // === TEMPORARY DIAGNOSTIC START ===
    // Log SBUS frames being sent to UART1 (once)
    if (packet.format == DataFormat::FORMAT_SBUS) {
        static bool firstSbusFrame = true;
        if (firstSbusFrame) {
            log_msg(LOG_INFO, "UART1: Transmitting SBUS frames (25 bytes) as raw UART data");
            log_msg(LOG_INFO, "UART1: Format allows SBUS transport over any baudrate");
            firstSbusFrame = false;
        }
        
        // Log every 100th SBUS frame for monitoring
        static uint32_t sbusFrameCount = 0;
        if (++sbusFrameCount % 100 == 0) {
            log_msg(LOG_DEBUG, "UART1: Sent %u SBUS frames", sbusFrameCount);
        }
    }
    // === TEMPORARY DIAGNOSTIC END ===
    
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