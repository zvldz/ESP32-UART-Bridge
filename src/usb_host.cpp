#include "usb_interface.h"
#include "logging.h"
#include "types.h"
#include <Arduino.h>

// ESP-IDF USB Host includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb/usb_types_ch9.h"

// Global USB mode variable
extern UsbMode usbMode;

#define USB_HOST_PRIORITY 20
#define USB_HOST_STACK_SIZE 4096
#define USB_CDC_DATA_INTERFACE_CLASS 0x0A

// USB Host implementation using ESP-IDF
class UsbHost : public UsbInterface {
private:
    uint32_t baudrate;
    bool initialized;
    bool is_connected;  // Renamed to avoid conflict with method
    usb_device_handle_t device_handle;
    
    // Ring buffers for data
    static const size_t RX_BUFFER_SIZE = 1024;
    static const size_t TX_BUFFER_SIZE = 1024;
    uint8_t rx_buffer[RX_BUFFER_SIZE];
    uint8_t tx_buffer[TX_BUFFER_SIZE];
    volatile size_t rx_head;
    volatile size_t rx_tail;
    volatile size_t tx_head;
    volatile size_t tx_tail;
    
    // Synchronization
    SemaphoreHandle_t rx_mutex;
    SemaphoreHandle_t tx_mutex;
    
    // USB Host task handle
    TaskHandle_t usb_host_task_handle;
    
    // Static instance for callbacks
    static UsbHost* instance;
    
    // USB endpoints
    uint8_t bulk_in_ep;
    uint8_t bulk_out_ep;
    usb_transfer_t *in_transfer;
    usb_transfer_t *out_transfer;

public:
    UsbHost(uint32_t baud) : baudrate(baud), initialized(false), is_connected(false),
                             device_handle(NULL), rx_head(0), rx_tail(0), 
                             tx_head(0), tx_tail(0), bulk_in_ep(0), bulk_out_ep(0),
                             in_transfer(NULL), out_transfer(NULL) {
        instance = this;
        rx_mutex = xSemaphoreCreateMutex();
        tx_mutex = xSemaphoreCreateMutex();
    }
    
    ~UsbHost() {
        end();
        if (rx_mutex) vSemaphoreDelete(rx_mutex);
        if (tx_mutex) vSemaphoreDelete(tx_mutex);
    }
    
    void init() override {
        log_msg("USB Host: Initializing...");
        
        // Install USB Host Library
        const usb_host_config_t host_config = {
            .skip_phy_setup = false,
            .intr_flags = ESP_INTR_FLAG_LEVEL1,
        };
        
        esp_err_t err = usb_host_install(&host_config);
        if (err != ESP_OK) {
            log_msg("USB Host: Failed to install driver: " + String(esp_err_to_name(err)));
            return;
        }
        
        // Register client
        const usb_host_client_config_t client_config = {
            .is_synchronous = false,
            .max_num_event_msg = 5,
            .async = {
                .client_event_callback = client_event_callback,
                .callback_arg = this
            }
        };
        
        usb_host_client_handle_t client_handle;
        err = usb_host_client_register(&client_config, &client_handle);
        if (err != ESP_OK) {
            log_msg("USB Host: Failed to register client: " + String(esp_err_to_name(err)));
            usb_host_uninstall();
            return;
        }
        
        // Create USB Host task
        xTaskCreate(usb_host_task, "usb_host", USB_HOST_STACK_SIZE, client_handle, 
                    USB_HOST_PRIORITY, &usb_host_task_handle);
        
        initialized = true;
        log_msg("USB Host: Initialized successfully");
    }
    
    int available() override {
        if (!is_connected) return 0;
        
        xSemaphoreTake(rx_mutex, portMAX_DELAY);
        int count = (rx_head >= rx_tail) ? 
                    (rx_head - rx_tail) : 
                    (RX_BUFFER_SIZE - rx_tail + rx_head);
        xSemaphoreGive(rx_mutex);
        
        return count;
    }
    
    int availableForWrite() override {
        if (!is_connected) return 0;
        
        xSemaphoreTake(tx_mutex, portMAX_DELAY);
        int space = (tx_tail > tx_head) ? 
                    (tx_tail - tx_head - 1) : 
                    (TX_BUFFER_SIZE - tx_head + tx_tail - 1);
        xSemaphoreGive(tx_mutex);
        
        return space;
    }
    
    int read() override {
        if (!is_connected || available() == 0) return -1;
        
        xSemaphoreTake(rx_mutex, portMAX_DELAY);
        uint8_t data = rx_buffer[rx_tail];
        rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;
        xSemaphoreGive(rx_mutex);
        
        return data;
    }
    
    size_t write(uint8_t data) override {
        if (!is_connected || availableForWrite() == 0) return 0;
        
        xSemaphoreTake(tx_mutex, portMAX_DELAY);
        tx_buffer[tx_head] = data;
        tx_head = (tx_head + 1) % TX_BUFFER_SIZE;
        xSemaphoreGive(tx_mutex);
        
        return 1;
    }
    
    size_t write(const uint8_t* buffer, size_t size) override {
        if (!is_connected) return 0;
        
        size_t written = 0;
        for (size_t i = 0; i < size; i++) {
            if (write(buffer[i]) == 0) break;
            written++;
        }
        
        return written;
    }
    
    bool connected() override {
        return initialized && is_connected;
    }
    
    void flush() override {
        if (!is_connected) return;
        
        // Wait for TX buffer to empty
        while (tx_head != tx_tail) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    
    void end() override {
        log_msg("USB Host: Shutting down...");
        
        is_connected = false;
        
        if (usb_host_task_handle) {
            vTaskDelete(usb_host_task_handle);
            usb_host_task_handle = NULL;
        }
        
        if (device_handle) {
            usb_host_device_close(NULL, device_handle);
            device_handle = NULL;
        }
        
        if (initialized) {
            // Note: client deregister and uninstall should be done carefully
            usb_host_uninstall();
        }
        
        initialized = false;
    }

private:
    // USB Host task
    static void usb_host_task(void* arg) {
        usb_host_client_handle_t client_handle = (usb_host_client_handle_t)arg;
        
        while (true) {
            // Handle USB Host Library events
            usb_host_client_handle_events(client_handle, portMAX_DELAY);
            
            // Handle TX data
            if (instance && instance->is_connected && instance->tx_head != instance->tx_tail) {
                instance->transmitPendingData();
            }
            
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    
    // Client event callback
    static void client_event_callback(const usb_host_client_event_msg_t *event_msg, void *arg) {
        UsbHost* self = (UsbHost*)arg;
        
        switch (event_msg->event) {
            case USB_HOST_CLIENT_EVENT_NEW_DEV:
                log_msg("USB Host: New device connected");
                self->handleDeviceConnection(event_msg->new_dev.address);
                break;
                
            case USB_HOST_CLIENT_EVENT_DEV_GONE:
                log_msg("USB Host: Device disconnected");
                self->handleDeviceDisconnection();
                break;
                
            default:
                break;
        }
    }
    
    // Handle new device connection
    void handleDeviceConnection(uint8_t dev_addr) {
        esp_err_t err;
        
        // Open device
        err = usb_host_device_open(NULL, dev_addr, &device_handle);
        if (err != ESP_OK) {
            log_msg("USB Host: Failed to open device");
            return;
        }
        
        // Get device info
        usb_device_info_t dev_info;
        err = usb_host_device_info(device_handle, &dev_info);
        if (err != ESP_OK) {
            log_msg("USB Host: Failed to get device info");
            usb_host_device_close(NULL, device_handle);
            device_handle = NULL;
            return;
        }
        
        // Get device descriptor for VID/PID
        const usb_device_desc_t *device_desc;
        err = usb_host_get_device_descriptor(device_handle, &device_desc);
        if (err == ESP_OK) {
            log_msg("USB Host: Device VID=0x" + String(device_desc->idVendor, HEX) + 
                    " PID=0x" + String(device_desc->idProduct, HEX));
        }
        
        // Get configuration descriptor
        const usb_config_desc_t *config_desc;
        err = usb_host_get_active_config_descriptor(device_handle, &config_desc);
        if (err != ESP_OK) {
            log_msg("USB Host: Failed to get config descriptor");
            usb_host_device_close(NULL, device_handle);
            device_handle = NULL;
            return;
        }
        
        // Find CDC interface and endpoints
        if (!findCDCInterface(config_desc)) {
            log_msg("USB Host: No CDC interface found");
            usb_host_device_close(NULL, device_handle);
            device_handle = NULL;
            return;
        }
        
        // Claim interface
        err = usb_host_interface_claim(NULL, device_handle, 0, 0);
        if (err != ESP_OK) {
            log_msg("USB Host: Failed to claim interface");
            usb_host_device_close(NULL, device_handle);
            device_handle = NULL;
            return;
        }
        
        // Allocate transfers
        err = usb_host_transfer_alloc(64, 0, &in_transfer);
        if (err != ESP_OK) {
            log_msg("USB Host: Failed to allocate IN transfer");
            cleanup();
            return;
        }
        
        err = usb_host_transfer_alloc(64, 0, &out_transfer);
        if (err != ESP_OK) {
            log_msg("USB Host: Failed to allocate OUT transfer");
            cleanup();
            return;
        }
        
        // Setup IN transfer
        in_transfer->device_handle = device_handle;
        in_transfer->bEndpointAddress = bulk_in_ep;
        in_transfer->callback = in_transfer_callback;
        in_transfer->context = this;
        in_transfer->num_bytes = 64;
        
        // Submit first IN transfer
        err = usb_host_transfer_submit(in_transfer);
        if (err != ESP_OK) {
            log_msg("USB Host: Failed to submit IN transfer");
            cleanup();
            return;
        }
        
        is_connected = true;
        log_msg("USB Host: CDC device connected and configured");
    }
    
    // Handle device disconnection
    void handleDeviceDisconnection() {
        is_connected = false;
        cleanup();
    }
    
    // Find CDC interface in configuration
    bool findCDCInterface(const usb_config_desc_t *config_desc) {
        const uint8_t *p = &config_desc->val[0];
        uint8_t bLength;
        
        for (int i = 0; i < config_desc->wTotalLength; i += bLength, p += bLength) {
            bLength = p[0];
            if (bLength == 0) break;
            
            uint8_t bDescriptorType = p[1];
            
            if (bDescriptorType == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
                uint8_t bInterfaceClass = p[5];
                
                if (bInterfaceClass == USB_CDC_DATA_INTERFACE_CLASS) {
                    // Found CDC Data interface, now find endpoints
                    p += bLength;
                    i += bLength;
                    
                    while (i < config_desc->wTotalLength) {
                        bLength = p[0];
                        bDescriptorType = p[1];
                        
                        if (bDescriptorType == USB_B_DESCRIPTOR_TYPE_ENDPOINT) {
                            uint8_t bEndpointAddress = p[2];
                            uint8_t bmAttributes = p[3];
                            
                            if ((bmAttributes & 0x03) == 0x02) { // Bulk endpoint
                                if (bEndpointAddress & 0x80) {
                                    bulk_in_ep = bEndpointAddress;
                                    log_msg("USB Host: Found IN endpoint 0x" + String(bulk_in_ep, HEX));
                                } else {
                                    bulk_out_ep = bEndpointAddress;
                                    log_msg("USB Host: Found OUT endpoint 0x" + String(bulk_out_ep, HEX));
                                }
                            }
                        } else if (bDescriptorType == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
                            break; // Next interface
                        }
                        
                        p += bLength;
                        i += bLength;
                    }
                    
                    return (bulk_in_ep != 0 && bulk_out_ep != 0);
                }
            }
        }
        
        return false;
    }
    
    // IN transfer callback
    static void in_transfer_callback(usb_transfer_t *transfer) {
        UsbHost* self = (UsbHost*)transfer->context;
        
        if (transfer->status == USB_TRANSFER_STATUS_COMPLETED) {
            // Copy data to ring buffer
            xSemaphoreTake(self->rx_mutex, portMAX_DELAY);
            
            for (int i = 0; i < transfer->actual_num_bytes; i++) {
                size_t next_head = (self->rx_head + 1) % RX_BUFFER_SIZE;
                if (next_head != self->rx_tail) {
                    self->rx_buffer[self->rx_head] = transfer->data_buffer[i];
                    self->rx_head = next_head;
                }
            }
            
            xSemaphoreGive(self->rx_mutex);
            
            // Resubmit transfer
            if (self->is_connected) {
                transfer->num_bytes = 64;
                usb_host_transfer_submit(transfer);
            }
        }
    }
    
    // Transmit pending data
    void transmitPendingData() {
        if (!out_transfer || tx_head == tx_tail) return;
        
        xSemaphoreTake(tx_mutex, portMAX_DELAY);
        
        // Copy data to transfer buffer
        size_t count = 0;
        while (tx_tail != tx_head && count < 64) {
            out_transfer->data_buffer[count++] = tx_buffer[tx_tail];
            tx_tail = (tx_tail + 1) % TX_BUFFER_SIZE;
        }
        
        xSemaphoreGive(tx_mutex);
        
        if (count > 0) {
            out_transfer->device_handle = device_handle;
            out_transfer->bEndpointAddress = bulk_out_ep;
            out_transfer->num_bytes = count;
            out_transfer->callback = out_transfer_callback;
            out_transfer->context = this;
            
            esp_err_t err = usb_host_transfer_submit(out_transfer);
            if (err != ESP_OK) {
                log_msg("USB Host: Failed to submit OUT transfer");
            }
        }
    }
    
    // OUT transfer callback
    static void out_transfer_callback(usb_transfer_t *transfer) {
        // Transfer completed
    }
    
    // Cleanup
    void cleanup() {
        if (in_transfer) {
            usb_host_transfer_free(in_transfer);
            in_transfer = NULL;
        }
        
        if (out_transfer) {
            usb_host_transfer_free(out_transfer);
            out_transfer = NULL;
        }
        
        if (device_handle) {
            usb_host_interface_release(NULL, device_handle, 0);
            usb_host_device_close(NULL, device_handle);
            device_handle = NULL;
        }
    }
};

// Static instance pointer
UsbHost* UsbHost::instance = nullptr;

// Factory function
UsbInterface* createUsbHost(uint32_t baudrate) {
    return new UsbHost(baudrate);
}

// Auto-detect implementation
class UsbAuto : public UsbInterface {
private:
    UsbInterface* activeInterface;
    uint32_t baudrate;

public:
    UsbAuto(uint32_t baud) : baudrate(baud), activeInterface(nullptr) {}
    
    ~UsbAuto() {
        if (activeInterface) {
            delete activeInterface;
        }
    }
    
    void init() override {
        // TODO: Detect VBUS to determine mode
        // For now, default to Device mode
        log_msg("USB Auto: defaulting to Device mode (VBUS detection not implemented)");
        activeInterface = createUsbDevice(baudrate);
        if (activeInterface) {
            activeInterface->init();
        }
    }
    
    int available() override {
        return activeInterface ? activeInterface->available() : 0;
    }
    
    int availableForWrite() override {
        return activeInterface ? activeInterface->availableForWrite() : 0;
    }
    
    int read() override {
        return activeInterface ? activeInterface->read() : -1;
    }
    
    size_t write(uint8_t data) override {
        return activeInterface ? activeInterface->write(data) : 0;
    }
    
    size_t write(const uint8_t* buffer, size_t size) override {
        return activeInterface ? activeInterface->write(buffer, size) : 0;
    }
    
    bool connected() override {
        return activeInterface ? activeInterface->connected() : false;
    }
    
    void flush() override {
        if (activeInterface) activeInterface->flush();
    }
    
    void end() override {
        if (activeInterface) {
            activeInterface->end();
            delete activeInterface;
            activeInterface = nullptr;
        }
    }
};

// Factory function
UsbInterface* createUsbAuto(uint32_t baudrate) {
    return new UsbAuto(baudrate);
}