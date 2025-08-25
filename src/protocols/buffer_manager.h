#pragma once
#include "../types.h"
#include "../circular_buffer.h"

void initProtocolBuffers(BridgeContext* ctx, Config* config);
void freeProtocolBuffers(BridgeContext* ctx);