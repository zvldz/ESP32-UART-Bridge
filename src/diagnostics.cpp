#include "diagnostics.h"
#include "logging.h"
#include "crashlog.h"
#include "esp_system.h"
#include "uart/uart_dma.h"
#include "types.h"
#include "protocols/udp_sender.h"
#include <Arduino.h>

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
        
        Serial.begin(115200);
        vTaskDelay(pdMS_TO_TICKS(100));
        
        Serial.println("\n\n=== BOOT INFO ===");
        Serial.print("CRASH DETECTED! Reset reason: ");
        Serial.println(crashlog_get_reset_reason_string(reason));
        Serial.println("===================\n");
        Serial.flush();
        
        Serial.end();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// System diagnostics - memory stats, uptime
void systemDiagnostics() {
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t minFreeHeap = ESP.getMinFreeHeap();
    
    // Critical memory conditions
    if (freeHeap < 10000) {
        log_msg(LOG_ERROR, "CRITICAL: Low memory! Free: %u bytes", freeHeap);
    } else if (freeHeap < 20000) {
        log_msg(LOG_WARNING, "Warning: Memory getting low. Free: %u bytes", freeHeap);
    } else {
        log_msg(LOG_DEBUG, "Memory: Free=%u bytes, Min=%u bytes", freeHeap, minFreeHeap);
    }
}

// Helper function for device2 role names
const char* getDevice2RoleName(uint8_t role) {
  switch(role) {
    case D2_NONE: return "Disabled";
    case D2_UART2: return "UART2";
    case D2_USB: return "USB";
    case D2_SBUS_IN: return "SBUS Input";    // NEW
    case D2_SBUS_OUT: return "SBUS Output";  // NEW
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
    case D3_SBUS_IN: return "SBUS Input";    // NEW
    case D3_SBUS_OUT: return "SBUS Output";  // NEW
    default: return "Unknown";
  }
}

// Helper function for device4 role names
const char* getDevice4RoleName(uint8_t role) {
  switch(role) {
    case D4_NONE: return "Disabled";
    case D4_NETWORK_BRIDGE: return "Network Bridge";
    case D4_LOG_NETWORK: return "Network Logger";
    default: return "Unknown";
  }
}

// Separate diagnostic functions for TaskScheduler

void runBridgeActivityLog() {
    if (!g_bridgeContext) return;
    
    String mode = (*g_bridgeContext->system.bridgeMode == BRIDGE_NET) ? "Network" : "Standalone";
    log_msg(LOG_DEBUG, "UART bridge alive [%s mode]: D1 RX=%lu TX=%lu bytes%s", mode.c_str(),
            g_deviceStats.device1.rxBytes.load(std::memory_order_relaxed), 
            g_deviceStats.device1.txBytes.load(std::memory_order_relaxed),
            (*g_bridgeContext->diagnostics.totalDroppedBytes > 0 ? 
             String(", dropped=" + String(*g_bridgeContext->diagnostics.totalDroppedBytes)).c_str() : ""));
}

void runStackDiagnostics() {
    if (!g_bridgeContext) return;
    
    UBaseType_t stackFree = uxTaskGetStackHighWaterMark(NULL);
    log_msg(LOG_DEBUG, "UART task: Stack free=%u bytes, Heap free=%u bytes, Largest block=%u bytes", 
            stackFree * 4, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    
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
    String msg = "Stack free: ";
    
    // Current task (main loop)
    UBaseType_t currentStack = uxTaskGetStackHighWaterMark(NULL);
    msg += "Main=" + String(currentStack * 4) + "B";
    
    // UART bridge task
    extern TaskHandle_t uartBridgeTaskHandle;
    if (uartBridgeTaskHandle) {
        UBaseType_t uartStack = uxTaskGetStackHighWaterMark(uartBridgeTaskHandle);
        msg += " UART=" + String(uartStack * 4) + "B";
    }
    
    // AsyncWebServer monitoring (runs in WiFi/TCP context)
    if (bridgeMode == BRIDGE_NET) {
        msg += " Web=Active";
        // Monitor WiFi task which handles AsyncWebServer requests
        TaskHandle_t wifiTask = xTaskGetHandle("wifi");
        if (wifiTask) {
            UBaseType_t wifiStack = uxTaskGetStackHighWaterMark(wifiTask);
            msg += ",WiFi=" + String(wifiStack * 4) + "B";
        }
        // Monitor TCP/IP task
        TaskHandle_t tcpipTask = xTaskGetHandle("tcpip_thread");
        if (tcpipTask) {
            UBaseType_t tcpipStack = uxTaskGetStackHighWaterMark(tcpipTask);
            msg += ",TCP=" + String(tcpipStack * 4) + "B";
        }
    } else {
        msg += " Web=Off";
    }
    
    // Add heap info
    msg += ", Heap=" + String(ESP.getFreeHeap()) + "B";
    msg += ", MaxBlock=" + String(ESP.getMaxAllocHeap()) + "B";
    
    log_msg(LOG_DEBUG, "%s", msg.c_str());
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
    
    // Output with prefix
    Serial.print("FORCE_LOG: ");
    Serial.println(buffer);
    Serial.flush();
}
#endif


