#ifndef QUICK_RESET_H
#define QUICK_RESET_H

// Quick Reset Detection - only for boards without BOOT button
#if defined(BOARD_MINIKIT_ESP32)

void quickResetInit();           // Call early in setup(), BEFORE led_init()
bool quickResetDetected();       // Returns true if 3 quick resets detected
void quickResetUpdateUptime();   // Call periodically in loop()

#endif // BOARD_MINIKIT_ESP32

#endif // QUICK_RESET_H
