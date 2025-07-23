#ifndef FLOW_CONTROL_H
#define FLOW_CONTROL_H

#include <Arduino.h>
#include "types.h"

// Flow control detection and status
void detectFlowControl();
String getFlowControlStatus();

#endif // FLOW_CONTROL_H