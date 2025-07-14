#ifndef USB_HOST_HELPERS_H
#define USB_HOST_HELPERS_H

// Helper macros for ESP-IDF USB Host
#ifdef __cplusplus
extern "C" {
#endif

// USB Host configuration helpers
#define USB_HOST_DEFAULT_CONFIG() { \
    .skip_phy_setup = false, \
    .intr_flags = ESP_INTR_FLAG_LEVEL1, \
}

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