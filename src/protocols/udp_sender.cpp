#include "udp_sender.h"
#include "udp_tx_queue.h"

// Static member definition
UdpTxQueue* UdpSender::txQueue = nullptr;