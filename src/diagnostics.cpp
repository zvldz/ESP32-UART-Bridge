#include "diagnostics.h"
#include "logging.h"
#include "crashlog.h"
#include "esp_system.h"
#include "uart/uart_dma.h"
#include "types.h"
#include "protocols/udp_sender.h"
#include <Arduino.h>
#include "esp_heap_caps.h"

// External object from main.cpp
extern BridgeMode bridgeMode;

// Global access to context (for TaskScheduler callbacks)
static BridgeContext* g_bridgeContext = nullptr;

void setBridgeContext(BridgeContext* ctx) {
    g_bridgeContext = ctx;
}

BridgeContext* getBridgeContext() {
    return g_bridgeContext;
}

// Print boot information to Serial (only for critical reset reasons)
void printBootInfo() {
    // Get reset reason
    esp_reset_reason_t reason = esp_reset_reason();

    // Only output to Serial for critical reset reasons
    if (reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT ||
        reason == ESP_RST_TASK_WDT || reason == ESP_RST_BROWNOUT) {

#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
        // S3 boards with USB CDC - output to USB Serial (original behavior)
        Serial.begin(115200);
        vTaskDelay(pdMS_TO_TICKS(100));

        Serial.println("\n\n=== BOOT INFO ===");
        Serial.print("CRASH DETECTED! Reset reason: ");
        Serial.println(crashlog_get_reset_reason_string(reason));
        Serial.println("===================\n");
        Serial.flush();

        Serial.end();
        vTaskDelay(pdMS_TO_TICKS(50));
#else
        // MiniKit/WROOM - output to UART0 (CP2104 USB-Serial)
        Serial0.begin(115200);
        vTaskDelay(pdMS_TO_TICKS(100));

        Serial0.println("\n\n=== BOOT INFO ===");
        Serial0.print("CRASH DETECTED! Reset reason: ");
        Serial0.println(crashlog_get_reset_reason_string(reason));
        Serial0.println("===================\n");
        Serial0.flush();

        Serial0.end();
        vTaskDelay(pdMS_TO_TICKS(50));
#endif
    }
}

// System diagnostics - merged into runAllStacksDiagnostics() (called every 10s)

// Helper function for device1 role names
const char* getDevice1RoleName(uint8_t role) {
  switch(role) {
    case D1_UART1: return "UART Bridge";
    case D1_SBUS_IN: return "SBUS Input";
    case D1_CRSF_IN: return "CRSF Input";
    default: return "Unknown";
  }
}

// Helper function for device2 role names
const char* getDevice2RoleName(uint8_t role) {
  switch(role) {
    case D2_NONE: return "Disabled";
    case D2_UART2: return "UART2";
    case D2_USB: return "USB";
    case D2_SBUS_IN: return "SBUS Input";
    case D2_SBUS_OUT: return "SBUS Output";
    case D2_USB_SBUS_TEXT: return "USB SBUS Text";
    case D2_USB_LOG: return "USB Logger";
    case D2_USB_CRSF_TEXT: return "USB CRSF Text";
    case D2_USB_CRSF_BRIDGE: return "USB CRSF Bridge";
    default: return "Unknown";
  }
}

// Helper function for device3 role names
const char* getDevice3RoleName(uint8_t role) {
  switch(role) {
    case D3_NONE: return "Disabled";
    case D3_UART3_MIRROR: return "UART3 Mirror";
    case D3_UART3_BRIDGE: return "UART3 Bridge";
    case D3_UART3_LOG: return "UART3 Logger";
    case D3_SBUS_IN: return "SBUS Input";
    case D3_SBUS_OUT: return "SBUS Output";
    case D3_CRSF_BRIDGE: return "CRSF Bridge";
    default: return "Unknown";
  }
}

// Helper function for device4 role names
const char* getDevice4RoleName(uint8_t role) {
  switch(role) {
    case D4_NONE: return "Disabled";
    case D4_NETWORK_BRIDGE: return "Network Bridge";
    case D4_LOG_NETWORK: return "Network Logger";
    case D4_SBUS_UDP_TX: return "SBUS→UDP (TX only)";
    case D4_SBUS_UDP_RX: return "UDP→SBUS (RX only)";
    case D4_CRSF_TEXT: return "CRSF Text Output";
    default: return "Unknown";
  }
}

#if defined(MINIKIT_BT_ENABLED)
// Helper function for device5 role names (Bluetooth SPP)
const char* getDevice5RoleName(uint8_t role) {
  switch(role) {
    case D5_NONE: return "Disabled";
    case D5_BT_BRIDGE: return "Bluetooth Bridge";
    case D5_BT_SBUS_TEXT: return "BT SBUS Text";
    default: return "Unknown";
  }
}
#endif

#if defined(BLE_ENABLED)
// Helper function for device5 role names (BLE)
const char* getDevice5RoleName(uint8_t role) {
  switch(role) {
    case D5_NONE: return "Disabled";
    case D5_BT_BRIDGE: return "BLE Bridge";
    case D5_BT_SBUS_TEXT: return "BLE SBUS Text";
    case D5_BT_CRSF_TEXT: return "BLE CRSF Text";
    default: return "Unknown";
  }
}
#endif

// Separate diagnostic functions for TaskScheduler

void runBridgeActivityLog() {
    if (!g_bridgeContext) return;

    const char* mode = (*g_bridgeContext->system.bridgeMode == BRIDGE_NET) ? "Network" : "Standalone";

    if (*g_bridgeContext->diagnostics.totalDroppedBytes > 0) {
        log_msg(LOG_DEBUG, "UART bridge alive [%s mode]: D1 RX=%lu TX=%lu bytes, dropped=%lu",
                mode,
                g_deviceStats.device1.rxBytes.load(std::memory_order_relaxed),
                g_deviceStats.device1.txBytes.load(std::memory_order_relaxed),
                *g_bridgeContext->diagnostics.totalDroppedBytes);
    } else {
        log_msg(LOG_DEBUG, "UART bridge alive [%s mode]: D1 RX=%lu TX=%lu bytes",
                mode,
                g_deviceStats.device1.rxBytes.load(std::memory_order_relaxed),
                g_deviceStats.device1.txBytes.load(std::memory_order_relaxed));
    }
}

void runStackDiagnostics() {
    if (!g_bridgeContext) return;

    UBaseType_t stackFree = uxTaskGetStackHighWaterMark(NULL);
    size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psramTotal = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);

    log_msg(LOG_DEBUG, "UART task: Stack free=%u bytes, Heap free=%u bytes, Largest block=%u bytes, PSRAM: %zu/%zu KB",
            stackFree, ESP.getFreeHeap(), ESP.getMaxAllocHeap(), psramFree/1024, psramTotal/1024);

    // Add DMA diagnostics if available
    UartDMA* dma = static_cast<UartDMA*>(g_bridgeContext->interfaces.uartBridgeSerial);
    if (dma && dma->isInitialized()) {
        log_msg(LOG_DEBUG, "DMA stats: RX=%lu TX=%lu, Overruns=%lu",
                dma->getRxBytesTotal(), dma->getTxBytesTotal(), dma->getOverrunCount());
    }
}

void runDroppedDataStats() {
    if (!g_bridgeContext) return;

    // Only log if there's something to report
    if (*g_bridgeContext->diagnostics.droppedBytes > 0) {
        // For regular drops (buffer full)
        if (*g_bridgeContext->diagnostics.maxDropSize > 0) {
            log_msg(LOG_INFO, "USB buffer full: dropped %lu bytes in %lu events (total: %lu bytes), max packet: %d bytes",
                    *g_bridgeContext->diagnostics.droppedBytes, *g_bridgeContext->diagnostics.dropEvents,
                    *g_bridgeContext->diagnostics.totalDroppedBytes, *g_bridgeContext->diagnostics.maxDropSize);
            *g_bridgeContext->diagnostics.maxDropSize = 0;  // Reset for next period
        }

        // For timeout drops
        int* timeoutDropSizes = g_bridgeContext->diagnostics.timeoutDropSizes;
        bool hasTimeoutDrops = false;
        for (int i = 0; i < 10; i++) {
            if (timeoutDropSizes[i] > 0) {
                hasTimeoutDrops = true;
                break;
            }
        }

        if (hasTimeoutDrops) {
            // Build string with last 10 timeout drop sizes
            String sizes = "Sizes: ";
            int sizeCount = 0;
            for (int i = 0; i < 10; i++) {
                if (timeoutDropSizes[i] > 0) {
                    if (sizeCount > 0) sizes += ", ";
                    sizes += String(timeoutDropSizes[i]);
                    sizeCount++;
                }
            }

            log_msg(LOG_INFO, "USB timeout: dropped %lu bytes in %lu events (total: %lu bytes). %s",
                    *g_bridgeContext->diagnostics.droppedBytes, *g_bridgeContext->diagnostics.dropEvents,
                    *g_bridgeContext->diagnostics.totalDroppedBytes, sizes.c_str());

            // Clear timeout sizes for next period
            for (int i = 0; i < 10; i++) {
                timeoutDropSizes[i] = 0;
            }
        }

        *g_bridgeContext->timing.lastDropLog = millis();
        *g_bridgeContext->diagnostics.droppedBytes = 0;
        *g_bridgeContext->diagnostics.dropEvents = 0;
    }
}

void runAllStacksDiagnostics() {
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t minFreeHeap = ESP.getMinFreeHeap();

    // Determine log level based on memory thresholds
    LogLevel logLevel = LOG_DEBUG;
    if (freeHeap < 10000) logLevel = LOG_ERROR;
    else if (freeHeap < 20000) logLevel = LOG_WARNING;

    char msg[256];
    int offset = 0;

    // Stack watermarks
    UBaseType_t currentStack = uxTaskGetStackHighWaterMark(NULL);
    offset += snprintf(msg + offset, sizeof(msg) - offset, "Main=%dB", currentStack);

    extern TaskHandle_t uartBridgeTaskHandle;
    if (uartBridgeTaskHandle) {
        UBaseType_t uartStack = uxTaskGetStackHighWaterMark(uartBridgeTaskHandle);
        offset += snprintf(msg + offset, sizeof(msg) - offset, " UART=%dB", uartStack);
    }

    if (bridgeMode == BRIDGE_NET) {
        TaskHandle_t wifiTask = xTaskGetHandle("wifi");
        if (wifiTask) {
            offset += snprintf(msg + offset, sizeof(msg) - offset, " WiFi=%dB",
                               (int)uxTaskGetStackHighWaterMark(wifiTask));
        }
        TaskHandle_t sysEvtTask = xTaskGetHandle("sys_evt");
        if (sysEvtTask) {
            offset += snprintf(msg + offset, sizeof(msg) - offset, " SysEvt=%dB",
                               (int)uxTaskGetStackHighWaterMark(sysEvtTask));
        }
    }

    // Heap + PSRAM
    size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psramTotal = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);

    snprintf(msg + offset, sizeof(msg) - offset,
             " | Heap=%u/%u (min=%u) MaxBlk=%dB | PSRAM=%zu/%zuKB",
             freeHeap, ESP.getHeapSize(), minFreeHeap,
             ESP.getMaxAllocHeap(), psramFree/1024, psramTotal/1024);

    log_msg(logLevel, "%s", msg);
}

// Log DMA statistics for a UART interface
void logDmaStatistics(UartInterface* uartSerial) {
  UartDMA* dma = static_cast<UartDMA*>(uartSerial);
  if (dma && dma->isInitialized()) {
    log_msg(LOG_DEBUG, "DMA stats: RX=%lu TX=%lu, Overruns=%lu",
            dma->getRxBytesTotal(), dma->getTxBytesTotal(), dma->getOverrunCount());
  }
}

#ifdef DEBUG
// Force output to serial for critical debugging
// Useful when normal logging system is not available
void forceSerialLog(const char* format, ...) {
    static bool serialInited = false;
    if (!serialInited) {
        Serial.begin(115200);
        vTaskDelay(pdMS_TO_TICKS(100));
        serialInited = true;
    }

    // Use stack buffer to avoid heap allocation
    char buffer[256];

    // Format the message
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Output with prefix (leading \n ensures clean line after partial output from other components)
    Serial.print("\nFORCE_LOG: ");
    Serial.println(buffer);
    Serial.flush();
}
#endif


