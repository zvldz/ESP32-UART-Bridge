#ifndef LOGGING_H
#define LOGGING_H

#include <Arduino.h>
#include "defines.h"

// Logging system interface
void logging_init();
void log_msg(String message);
void logging_get_recent_logs(String* buffer, int maxCount, int* actualCount);
void logging_clear();

#endif // LOGGING_H