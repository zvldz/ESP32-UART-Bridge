#include "udp_sender.h"

// Static member definitions
unsigned long UdpSender::udpTxBytes = 0;
unsigned long UdpSender::udpTxPackets = 0;
unsigned long UdpSender::udpRxBytes = 0;
unsigned long UdpSender::udpRxPackets = 0;
unsigned long UdpSender::device1TxBytesFromDevice4 = 0;