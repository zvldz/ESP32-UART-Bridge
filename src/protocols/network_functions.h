// network_functions.h - Network utility functions
#ifndef NETWORK_FUNCTIONS_H
#define NETWORK_FUNCTIONS_H

#include <stdint.h>
#include <stddef.h>

// Check if Device4 is active
extern bool isDevice4Active();

// Get Device4 configuration
extern uint32_t getDevice4Role();

#endif // NETWORK_FUNCTIONS_H