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
#include "soc/usb_wrap_reg.h"
#include "soc/usb_wrap_struct.h"

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
    bool is_connected;
    usb_device_handle_t device_handle;
    uint8_t interface_num;  // Track which interface we claimed
    
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
    usb_host_client_handle_t client_handle;
    
    // Static instance for callbacks
    static UsbHost* instance;
    
    // USB endpoints
    uint8_t bulk_in_ep;
    uint8_t bulk_out_ep;
    usb_transfer_t *in_transfer;
    usb_transfer_t *out_transfer;

public:
    UsbHost(uint32_t baud) : baudrate(baud), initialized(false), is_connected(false),
                             device_handle(NULL), interface_num(0), rx_head(0), rx_tail(0), 
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
        // Install USB Host Library
        const usb_host_config_t host_config = {
            .skip_phy_setup = false,
            .intr_flags = ESP_INTR_FLAG_LEVEL1,
        };
        
        esp_err_t err = usb_host_install(&host_config);
        if (err != ESP_OK) {
            log_msg("USB Host: Failed to install driver: " + String(esp_err_to_name(err)), LOG_ERROR);
            return;
        }
        
        // Small delay for stabilization
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Configure PHY for Host mode
        USB_WRAP.otg_conf.pad_pull_override = 1;
        USB_WRAP.otg_conf.dp_pullup = 0;
        USB_WRAP.otg_conf.dp_pulldown = 0;
        USB_WRAP.otg_conf.dm_pullup = 0;
        USB_WRAP.otg_conf.dm_pulldown = 0;
        
        // Enable VBUS sensing
        USB_WRAP.otg_conf.vrefh = 2;
        USB_WRAP.otg_conf.vrefl = 2;
        
        // Register client
        const usb_host_client_config_t client_config = {
            .is_synchronous = false,
            .max_num_event_msg = 5,
            .async = {
                .client_event_callback = client_event_callback,
                .callback_arg = this
            }
        };
        
        err = usb_host_client_register(&client_config, &this->client_handle);
        if (err != ESP_OK) {
            log_msg("USB Host: Failed to register client: " + String(esp_err_to_name(err)), LOG_ERROR);
            usb_host_uninstall();
            return;
        }
        
        // Create USB Host task
        BaseType_t task_created = xTaskCreate(usb_host_task, "usb_host", USB_HOST_STACK_SIZE, this, 
                                             USB_HOST_PRIORITY, &usb_host_task_handle);
        
        if (task_created != pdPASS || usb_host_task_handle == NULL) {
            log_msg("USB Host: Failed to create task", LOG_ERROR);
            usb_host_client_deregister(this->client_handle);
            usb_host_uninstall();
            return;
        }
        
        // Give task time to start
        vTaskDelay(pdMS_TO_TICKS(200));
        
        // Check for already connected devices
        uint8_t dev_addr_list[16];
        int num_devs = 16;
        err = usb_host_device_addr_list_fill(num_devs, dev_addr_list, &num_devs);
        if (err == ESP_OK && num_devs > 0) {
            for (int i = 0; i < num_devs; i++) {
                if (!this->is_connected) {
                    handleDeviceConnection(dev_addr_list[i]);
                }
            }
        }
        
        initialized = true;
        log_msg("USB Host: Initialized", LOG_INFO);
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
        log_msg("USB Host: Shutting down...", LOG_INFO);
        
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
            usb_host_client_deregister(this->client_handle);
            usb_host_uninstall();
        }
        
        initialized = false;
    }

private:
    // USB Host task
    static void usb_host_task(void* arg) {
        UsbHost* self = (UsbHost*)arg;
        
        while (true) {
            // Handle client events
            uint32_t event_flags;
            esp_err_t err = usb_host_client_handle_events(self->client_handle, pdMS_TO_TICKS(10));
            
            if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
                log_msg("USB Host: Event error: " + String(esp_err_to_name(err)), LOG_ERROR);
            }
            
            // Process USB Host library events
            usb_host_lib_handle_events(pdMS_TO_TICKS(10), &event_flags);
            
            // Handle TX data - this is important for transmitting data!
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
                log_msg("USB Host: Device connected", LOG_INFO);
                self->handleDeviceConnection(event_msg->new_dev.address);
                break;
                
            case USB_HOST_CLIENT_EVENT_DEV_GONE:
                log_msg("USB Host: Device disconnected", LOG_INFO);
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
        err = usb_host_device_open(this->client_handle, dev_addr, &device_handle);
        if (err != ESP_OK) {
            log_msg("USB Host: Failed to open device: " + String(esp_err_to_name(err)), LOG_ERROR);
            return;
        }
        
        // Get device info
        usb_device_info_t dev_info;
        err = usb_host_device_info(device_handle, &dev_info);
        if (err != ESP_OK) {
            log_msg("USB Host: Failed to get device info", LOG_ERROR);
            usb_host_device_close(this->client_handle, device_handle);
            device_handle = NULL;
            return;
        }
        
        // Get device descriptor for logging
        const usb_device_desc_t *device_desc;
        err = usb_host_get_device_descriptor(device_handle, &device_desc);
        if (err == ESP_OK) {
            log_msg("USB Host: VID=0x" + String(device_desc->idVendor, HEX) + 
                    " PID=0x" + String(device_desc->idProduct, HEX), LOG_INFO);
        }
        
        // Get configuration descriptor
        const usb_config_desc_t *config_desc;
        err = usb_host_get_active_config_descriptor(device_handle, &config_desc);
        if (err != ESP_OK) {
            log_msg("USB Host: Failed to get config descriptor", LOG_ERROR);
            usb_host_device_close(this->client_handle, device_handle);
            device_handle = NULL;
            return;
        }
        
        // Find CDC interface and endpoints
        if (!findCDCInterface(config_desc)) {
            log_msg("USB Host: No CDC interface found", LOG_WARNING);
            usb_host_device_close(this->client_handle, device_handle);
            device_handle = NULL;
            return;
        }
        
        // Claim interface with the correct interface number
        err = usb_host_interface_claim(this->client_handle, device_handle, interface_num, 0);
        if (err != ESP_OK) {
            log_msg("USB Host: Failed to claim interface " + String(interface_num) + ": " + String(esp_err_to_name(err)), LOG_ERROR);
            usb_host_device_close(this->client_handle, device_handle);
            device_handle = NULL;
            return;
        }
        
        log_msg("USB Host: Successfully claimed interface " + String(interface_num), LOG_INFO);
        
        // Allocate transfers with appropriate buffer size
        const size_t transfer_size = 64;  // Standard CDC bulk endpoint size
        
        err = usb_host_transfer_alloc(transfer_size, 0, &in_transfer);
        if (err != ESP_OK) {
            log_msg("USB Host: Failed to allocate IN transfer", LOG_ERROR);
            cleanup();
            return;
        }
        
        err = usb_host_transfer_alloc(transfer_size, 0, &out_transfer);
        if (err != ESP_OK) {
            log_msg("USB Host: Failed to allocate OUT transfer", LOG_ERROR);
            cleanup();
            return;
        }
        
        // Setup IN transfer
        in_transfer->device_handle = device_handle;
        in_transfer->bEndpointAddress = bulk_in_ep;
        in_transfer->callback = in_transfer_callback;
        in_transfer->context = this;
        in_transfer->num_bytes = transfer_size;
        in_transfer->timeout_ms = 0;  // No timeout for IN transfers
        
        // Submit first IN transfer
        err = usb_host_transfer_submit(in_transfer);
        if (err != ESP_OK) {
            log_msg("USB Host: Failed to submit IN transfer: " + String(esp_err_to_name(err)), LOG_ERROR);
            cleanup();
            return;
        }
        
        is_connected = true;
        log_msg("USB Host: Connected", LOG_INFO);
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
                uint8_t bInterfaceNumber = p[2];
                uint8_t bInterfaceClass = p[5];
                
                // Check for CDC Data interface (0x0A) or Communications interface (0x02)
                if (bInterfaceClass == USB_CDC_DATA_INTERFACE_CLASS) {
                    
                    log_msg("USB Host: Found interface " + String(bInterfaceNumber) + 
                            " with class 0x" + String(bInterfaceClass, HEX), LOG_DEBUG);
                    
                    interface_num = bInterfaceNumber;  // Save interface number
                    
                    // Find endpoints for this interface
                    p += bLength;
                    i += bLength;
                    
                    while (i < config_desc->wTotalLength) {
                        bLength = p[0];
                        if (bLength == 0) break;
                        
                        bDescriptorType = p[1];
                        
                        if (bDescriptorType == USB_B_DESCRIPTOR_TYPE_ENDPOINT) {
                            uint8_t bEndpointAddress = p[2];
                            uint8_t bmAttributes = p[3];
                            
                            // Look for bulk endpoints
                            if ((bmAttributes & 0x03) == 0x02) {
                                if (bEndpointAddress & 0x80) {
                                    bulk_in_ep = bEndpointAddress;
                                } else {
                                    bulk_out_ep = bEndpointAddress;
                                }
                            }
                        } else if (bDescriptorType == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
                            // Next interface, stop looking
                            break;
                        }
                        
                        p += bLength;
                        i += bLength;
                    }
                    
                    // If we found both endpoints, we're done
                    if (bulk_in_ep != 0 && bulk_out_ep != 0) {
                        return true;
                    }
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
                esp_err_t err = usb_host_transfer_submit(transfer);
                if (err != ESP_OK) {
                    // Connection lost, don't log to avoid spam
                    self->is_connected = false;
                }
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
                // Connection lost or endpoint error
                is_connected = false;
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
            usb_host_interface_release(this->client_handle, device_handle, interface_num);
            usb_host_device_close(this->client_handle, device_handle);
            device_handle = NULL;
        }
        
        bulk_in_ep = 0;
        bulk_out_ep = 0;
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
        log_msg("USB Auto: defaulting to Device mode (VBUS detection not implemented)", LOG_INFO);
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