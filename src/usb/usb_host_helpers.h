#ifndef USB_HOST_HELPERS_H
#define USB_HOST_HELPERS_H

// Helper macros and functions for ESP-IDF USB Host implementation
#ifdef __cplusplus
extern "C" {
#endif

#include "usb/usb_types_ch9.h"
#include "usb/usb_host.h"

// USB Host configuration helpers
#define USB_HOST_DEFAULT_CONFIG() { \
    .skip_phy_setup = false, \
    .intr_flags = ESP_INTR_FLAG_LEVEL1, \
}

// CDC ACM driver configuration helper
#define CDC_ACM_DEFAULT_CONFIG() { \
    .driver_task_stack_size = 4096, \
    .driver_task_priority = 10, \
    .xCoreID = 0, \
    .new_dev_cb = NULL, \
}

#ifdef __cplusplus
}
#endif

#endif // USB_HOST_HELPERS_H