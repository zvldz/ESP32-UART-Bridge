#include "flow_control.h"
#include "defines.h"
#include "logging.h"
#include <Arduino.h>

// External objects from main.cpp
extern Config config;
extern FlowControlStatus flowControlStatus;

// Detect flow control hardware presence
void detectFlowControl() {
  if (!config.flowcontrol) {
    flowControlStatus.flowControlDetected = false;
    flowControlStatus.flowControlActive = false;
    return;
  }

  // Configure RTS/CTS pins for testing
  pinMode(RTS_PIN, OUTPUT);
  pinMode(CTS_PIN, INPUT_PULLUP);

  // Test RTS/CTS functionality
  digitalWrite(RTS_PIN, HIGH);
  delay(1);
  bool ctsHigh = digitalRead(CTS_PIN);

  digitalWrite(RTS_PIN, LOW);
  delay(1);
  bool ctsLow = digitalRead(CTS_PIN);

  // If CTS responds to RTS changes, flow control is connected
  flowControlStatus.flowControlDetected = (ctsHigh != ctsLow);

  if (flowControlStatus.flowControlDetected) {
    // Flow control is configured in UartDMA::begin()
    flowControlStatus.flowControlActive = true;
    log_msg(LOG_INFO, "Flow control detected and activated");
  } else {
    log_msg(LOG_WARNING, "Flow control enabled but no RTS/CTS detected");
    flowControlStatus.flowControlActive = false;
  }
}

// Get flow control status string
String getFlowControlStatus() {
  if (!config.flowcontrol) {
    return "Disabled";
  } else if (flowControlStatus.flowControlDetected && flowControlStatus.flowControlActive) {
    return "Enabled (Active)";
  } else {
    return "Enabled (No RTS/CTS detected)";
  }
}