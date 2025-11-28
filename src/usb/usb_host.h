#ifndef USB_HOST_H
#define USB_HOST_H

#include "usb_base.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb/usb_host.h"
#include "usb/usb_types_ch9.h"

// USB Host configuration constants
#define USB_HOST_PRIORITY 20
#define USB_HOST_STACK_SIZE 4096
#define USB_CDC_DATA_INTERFACE_CLASS 0x0A
#define USB_TRANSFER_SIZE 64
#define USB_HOST_TASK_CORE 0
#define USB_HOST_TASK_NAME "USB_Host_Task"

/**
 * USB Host implementation using ESP-IDF
 * Provides CDC ACM device communication through USB Host stack
 */
class UsbHost : public UsbBase {
private:
    // USB device management
    usb_device_handle_t device_handle;
    uint8_t interface_num;
    usb_host_client_handle_t client_handle;
    
    // USB endpoints
    uint8_t bulk_in_endpoint;      // IN endpoint address
    uint8_t bulk_out_endpoint;     // OUT endpoint address
    usb_transfer_t* in_transfer;   // IN transfer structure
    usb_transfer_t* out_transfer;  // OUT transfer structure

    // Transfer state tracking
    volatile bool out_transfer_busy;  // Track if OUT transfer is in progress

    // Static instance for callbacks
    static UsbHost* instance;

public:
    explicit UsbHost(uint32_t baudrate);
    ~UsbHost();
    
    // UsbBase interface implementation
    void init() override;
    void end() override;

protected:
    void flushHardware() override;

private:
    // Device management methods
    bool openDevice(uint8_t device_address);
    bool getDeviceInfo();
    bool findCDCInterface(const usb_config_desc_t* config_desc);
    bool findBulkEndpoints(const usb_config_desc_t* config_desc, const uint8_t* interface_desc, int start_offset);
    bool claimInterface();
    bool setupTransfers();
    void closeDevice();
    
    // Event handling methods
    void handleDeviceConnection(uint8_t device_address);
    void handleDeviceDisconnection();
    
    // Transfer management methods  
    void transmitPendingData();
    bool submitInTransfer();
    bool submitOutTransfer(const uint8_t* data, size_t length);
    
    // Static callback methods
    static void usbHostTask(void* parameter);
    static void clientEventCallback(const usb_host_client_event_msg_t* event_msg, void* arg);
    static void inTransferCallback(usb_transfer_t* transfer);
    static void outTransferCallback(usb_transfer_t* transfer);
    
    // Cleanup methods
    void cleanup();
};

// Factory function declaration
UsbInterface* createUsbHost(uint32_t baudrate);

#endif // USB_HOST_H