#include "usb_host.h"
#include "logging.h"
#include "types.h"
#include "soc/usb_wrap_reg.h"
#include "soc/usb_wrap_struct.h"

// Global USB mode variable
extern UsbMode usbMode;

// Static instance pointer
UsbHost* UsbHost::instance = nullptr;

// Constructor
UsbHost::UsbHost(uint32_t baudrate) : UsbBase(baudrate), device_handle(nullptr), interface_num(0), client_handle(nullptr), bulk_in_endpoint(0), bulk_out_endpoint(0), in_transfer(nullptr), out_transfer(nullptr), out_transfer_busy(false) {
    instance = this;
}

UsbHost::~UsbHost() {
    end();
}
// Initialize USB Host interface
void UsbHost::init() {
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    esp_err_t err = usb_host_install(&host_config);
    if (err != ESP_OK) {
        log_msg(LOG_ERROR, "USB Host: Failed to install driver: %s", esp_err_to_name(err));
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    USB_WRAP.otg_conf.pad_pull_override = 1;
    USB_WRAP.otg_conf.dp_pullup = 0;
    USB_WRAP.otg_conf.dp_pulldown = 0;
    USB_WRAP.otg_conf.dm_pullup = 0;
    USB_WRAP.otg_conf.dm_pulldown = 0;

    USB_WRAP.otg_conf.vrefh = 2;
    USB_WRAP.otg_conf.vrefl = 2;

    const usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = clientEventCallback,
            .callback_arg = this
        }
    };

    err = usb_host_client_register(&client_config, &this->client_handle);
    if (err != ESP_OK) {
        log_msg(LOG_ERROR, "USB Host: Failed to register client: %s", esp_err_to_name(err));
        usb_host_uninstall();
        return;
    }

    BaseType_t task_created = xTaskCreatePinnedToCore(
        usbHostTask,
        USB_HOST_TASK_NAME,
        USB_HOST_STACK_SIZE,
        this,
        USB_HOST_PRIORITY,
        &task_handle,
        USB_HOST_TASK_CORE
    );

    if (task_created != pdPASS || task_handle == nullptr) {
        log_msg(LOG_ERROR, "USB Host: Failed to create task");
        usb_host_client_deregister(this->client_handle);
        usb_host_uninstall();
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(200));

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
    log_msg(LOG_INFO, "USB Host: Initialized");
}

// Shutdown USB Host interface
void UsbHost::end() {
    log_msg(LOG_INFO, "USB Host: Shutting down...");

    is_connected = false;

    if (task_handle) {
        vTaskDelete(task_handle);
        task_handle = nullptr;
    }

    if (device_handle) {
        usb_host_device_close(nullptr, device_handle);
        device_handle = nullptr;
    }

    if (initialized) {
        if (client_handle) {
            usb_host_client_deregister(client_handle);
            client_handle = nullptr;
        }
        usb_host_uninstall();
    }

    initialized = false;
}

// Implementation of abstract method from base class
void UsbHost::flushHardware() {
    // USB Host doesn't have specific hardware flush, data is sent immediately via transfers
}
// USB Host task implementation
void UsbHost::usbHostTask(void* parameter) {
    UsbHost* usb_host = static_cast<UsbHost*>(parameter);

    log_msg(LOG_INFO, "USB Host task started on core %d", xPortGetCoreID());

    while (1) {
        esp_err_t err = usb_host_client_handle_events(usb_host->client_handle, pdMS_TO_TICKS(10));

        if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
            log_msg(LOG_ERROR, "USB Host: Client event error: %s", esp_err_to_name(err));
        }

        uint32_t event_flags;
        usb_host_lib_handle_events(pdMS_TO_TICKS(10), &event_flags);

        if (usb_host->is_connected && usb_host->availableForWrite() < TX_BUFFER_SIZE) {
            usb_host->transmitPendingData();
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
// USB Host client event callback
void UsbHost::clientEventCallback(const usb_host_client_event_msg_t* event_msg, void* arg) {
    UsbHost* usb_host = static_cast<UsbHost*>(arg);

    switch (event_msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            log_msg(LOG_INFO, "USB Host: Device connected");
            usb_host->handleDeviceConnection(event_msg->new_dev.address);
            break;

        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            log_msg(LOG_INFO, "USB Host: Device disconnected");
            usb_host->handleDeviceDisconnection();
            break;

        default:
            break;
    }
}
// Handle new device connection - main coordination method
void UsbHost::handleDeviceConnection(uint8_t device_address) {
    log_msg(LOG_INFO, "USB Host: Processing device connection");

    if (!openDevice(device_address)) {
        return;
    }

    if (!getDeviceInfo()) {
        closeDevice();
        return;
    }

    const usb_config_desc_t* config_desc;
    esp_err_t err = usb_host_get_active_config_descriptor(device_handle, &config_desc);
    if (err != ESP_OK) {
        log_msg(LOG_ERROR, "USB Host: Failed to get config descriptor");
        closeDevice();
        return;
    }

    if (!findCDCInterface(config_desc)) {
        log_msg(LOG_WARNING, "USB Host: No CDC interface found");
        closeDevice();
        return;
    }

    if (!claimInterface()) {
        closeDevice();
        return;
    }

    if (!setupTransfers()) {
        closeDevice();
        return;
    }

    is_connected = true;
    log_msg(LOG_INFO, "USB Host: Device connected successfully");
}

// Open USB device by address
bool UsbHost::openDevice(uint8_t device_address) {
  esp_err_t err = usb_host_device_open(client_handle, device_address, &device_handle);
  if (err != ESP_OK) {
    log_msg(LOG_ERROR, "USB Host: Failed to open device: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

// Get device information and log device descriptor
bool UsbHost::getDeviceInfo() {
  usb_device_info_t device_info;
  esp_err_t err = usb_host_device_info(device_handle, &device_info);
  if (err != ESP_OK) {
    log_msg(LOG_ERROR, "USB Host: Failed to get device info");
    return false;
  }

  const usb_device_desc_t* device_desc;
  err = usb_host_get_device_descriptor(device_handle, &device_desc);
  if (err == ESP_OK) {
    log_msg(LOG_INFO, "USB Host: VID=0x%04X PID=0x%04X", device_desc->idVendor, device_desc->idProduct);
  }

  return true;
}

// Claim the CDC interface
bool UsbHost::claimInterface() {
  esp_err_t err = usb_host_interface_claim(client_handle, device_handle, interface_num, 0);
  if (err != ESP_OK) {
    log_msg(LOG_ERROR, "USB Host: Failed to claim interface %d: %s", interface_num, esp_err_to_name(err));
    return false;
  }

  log_msg(LOG_INFO, "USB Host: Successfully claimed interface %d", interface_num);
  return true;
}

// Setup USB transfers for IN and OUT endpoints
bool UsbHost::setupTransfers() {
  esp_err_t err = usb_host_transfer_alloc(USB_TRANSFER_SIZE, 0, &in_transfer);
  if (err != ESP_OK) {
    log_msg(LOG_ERROR, "USB Host: Failed to allocate IN transfer");
    return false;
  }

  err = usb_host_transfer_alloc(USB_TRANSFER_SIZE, 0, &out_transfer);
  if (err != ESP_OK) {
    log_msg(LOG_ERROR, "USB Host: Failed to allocate OUT transfer");
    return false;
  }

  in_transfer->device_handle = device_handle;
  in_transfer->bEndpointAddress = bulk_in_endpoint;
  in_transfer->callback = inTransferCallback;
  in_transfer->context = this;
  in_transfer->num_bytes = USB_TRANSFER_SIZE;
  in_transfer->timeout_ms = 0;

  return submitInTransfer();
}

// Submit IN transfer
bool UsbHost::submitInTransfer() {
  esp_err_t err = usb_host_transfer_submit(in_transfer);
  if (err != ESP_OK) {
    log_msg(LOG_ERROR, "USB Host: Failed to submit IN transfer: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

void UsbHost::closeDevice() {
  if (device_handle) {
    usb_host_device_close(client_handle, device_handle);
    device_handle = nullptr;
  }
}

void UsbHost::handleDeviceDisconnection() {
  is_connected = false;
  cleanup();
}
// Find CDC Data interface and extract bulk endpoints
// Searches configuration descriptor for CDC Data interface (class 0x0A)
// Returns true if both IN and OUT bulk endpoints are found
bool UsbHost::findCDCInterface(const usb_config_desc_t* config_desc) {
  const uint8_t* descriptor_ptr = &config_desc->val[0];
  uint8_t descriptor_length;

  log_msg(LOG_DEBUG, "USB Host: Searching for CDC interface");

  for (int offset = 0; offset < config_desc->wTotalLength; offset += descriptor_length, descriptor_ptr += descriptor_length) {
    descriptor_length = descriptor_ptr[0];
    if (descriptor_length == 0) {
      break;
    }

    uint8_t descriptor_type = descriptor_ptr[1];

    if (descriptor_type == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
      uint8_t interface_number = descriptor_ptr[2];
      uint8_t interface_class = descriptor_ptr[5];

      if (interface_class == USB_CDC_DATA_INTERFACE_CLASS) {
        log_msg(LOG_DEBUG, "USB Host: Found CDC interface %d with class 0x%02X", interface_number, interface_class);

        interface_num = interface_number;

        if (findBulkEndpoints(config_desc, descriptor_ptr, offset)) {
          log_msg(LOG_INFO, "USB Host: Found CDC interface with bulk endpoints");
          return true;
        }
      }
    }
  }

  log_msg(LOG_WARNING, "USB Host: No suitable CDC interface found");
  return false;
}

// Find bulk endpoints for the CDC interface
bool UsbHost::findBulkEndpoints(const usb_config_desc_t* config_desc, const uint8_t* interface_desc, int start_offset) {
  const uint8_t* descriptor_ptr = interface_desc + interface_desc[0];
  int offset = start_offset + interface_desc[0];

  bulk_in_endpoint = 0;
  bulk_out_endpoint = 0;

  while (offset < config_desc->wTotalLength) {
    uint8_t descriptor_length = descriptor_ptr[0];
    if (descriptor_length == 0) {
      break;
    }

    uint8_t descriptor_type = descriptor_ptr[1];

    if (descriptor_type == USB_B_DESCRIPTOR_TYPE_ENDPOINT) {
      uint8_t endpoint_address = descriptor_ptr[2];
      uint8_t endpoint_attributes = descriptor_ptr[3];

      if ((endpoint_attributes & 0x03) == 0x02) {
        if (endpoint_address & 0x80) {
          bulk_in_endpoint = endpoint_address;
          log_msg(LOG_DEBUG, "USB Host: Found IN endpoint 0x%02X", endpoint_address);
        } else {
          bulk_out_endpoint = endpoint_address;
          log_msg(LOG_DEBUG, "USB Host: Found OUT endpoint 0x%02X", endpoint_address);
        }
      }
    } else if (descriptor_type == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
      break;
    }

    descriptor_ptr += descriptor_length;
    offset += descriptor_length;
  }

  bool endpoints_found = (bulk_in_endpoint != 0 && bulk_out_endpoint != 0);
  if (endpoints_found) {
    log_msg(LOG_INFO, "USB Host: Found bulk endpoints IN=0x%02X OUT=0x%02X", bulk_in_endpoint, bulk_out_endpoint);
  }

  return endpoints_found;
}
// IN transfer callback - handles received data
void UsbHost::inTransferCallback(usb_transfer_t* transfer) {
  UsbHost* usb_host = static_cast<UsbHost*>(transfer->context);

  if (transfer->status == USB_TRANSFER_STATUS_COMPLETED) {
    if (!usb_host->addToRxBuffer(transfer->data_buffer, transfer->actual_num_bytes)) {
      log_msg(LOG_WARNING, "USB Host: RX buffer overflow");
    }

    if (usb_host->is_connected) {
      transfer->num_bytes = USB_TRANSFER_SIZE;
      esp_err_t err = usb_host_transfer_submit(transfer);
      if (err != ESP_OK) {
        usb_host->is_connected = false;
      }
    }
  }
}
// Transmit pending data from TX buffer
void UsbHost::transmitPendingData() {
  if (!out_transfer || !is_connected) {
    return;
  }

  // Check if previous transfer is still in progress
  if (out_transfer_busy) {
    return;  // Wait for callback to clear the flag
  }

  uint8_t transmission_buffer[USB_TRANSFER_SIZE];
  size_t bytes_to_send = getFromTxBuffer(transmission_buffer, sizeof(transmission_buffer));

  if (bytes_to_send > 0) {
    memcpy(out_transfer->data_buffer, transmission_buffer, bytes_to_send);
    out_transfer->device_handle = device_handle;
    out_transfer->bEndpointAddress = bulk_out_endpoint;
    out_transfer->num_bytes = bytes_to_send;
    out_transfer->callback = outTransferCallback;
    out_transfer->context = this;

    out_transfer_busy = true;  // Mark as busy before submit

    esp_err_t err = usb_host_transfer_submit(out_transfer);
    if (err != ESP_OK) {
      log_msg(LOG_DEBUG, "USB Host: Failed to submit OUT transfer: %s", esp_err_to_name(err));
      out_transfer_busy = false;  // Clear flag on error
      is_connected = false;
    }
  }
}
// OUT transfer callback - handles transmission completion
void UsbHost::outTransferCallback(usb_transfer_t* transfer) {
  // Clear busy flag when transfer completes
  if (instance) {
    instance->out_transfer_busy = false;
  }
  // OUT transfer completed successfully - no additional processing needed
}
// Cleanup USB resources and reset state
void UsbHost::cleanup() {
  if (in_transfer) {
    usb_host_transfer_free(in_transfer);
    in_transfer = nullptr;
  }

  if (out_transfer) {
    usb_host_transfer_free(out_transfer);
    out_transfer = nullptr;
  }

  if (device_handle) {
    usb_host_interface_release(client_handle, device_handle, interface_num);
    usb_host_device_close(client_handle, device_handle);
    device_handle = nullptr;
  }

  bulk_in_endpoint = 0;
  bulk_out_endpoint = 0;
}

// Factory function implementation
UsbInterface* createUsbHost(uint32_t baudrate) {
  return new UsbHost(baudrate);
}

