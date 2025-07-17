#ifndef SYSTEM_UTILS_H
#define SYSTEM_UTILS_H

// Disable brownout detector (called before setup)
void disableBrownout();

// Disable USB Serial/JTAG interrupts to prevent spurious resets
void disableUsbJtagInterrupts();

// Clear serial buffer from bootloader messages
void clearBootloaderSerialBuffer();

#endif // SYSTEM_UTILS_H