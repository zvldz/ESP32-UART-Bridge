#pragma once
#include "protocol_types.h"

class ProtocolRouter {
public:
    virtual ~ProtocolRouter() = default;
    virtual void process(ParsedPacket* packets, size_t count) = 0;
    virtual void reset() = 0;
    virtual void getStats(uint32_t& hits, uint32_t& broadcasts) = 0;
};