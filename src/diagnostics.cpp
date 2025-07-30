#include "diagnostics.h"
#include "logging.h"
#include "crashlog.h"
#include "esp_system.h"
#include "uart_dma.h"
#include "types.h"
#include <Arduino.h>

// External object from main.cpp
extern UartStats uartStats;
extern BridgeMode bridgeMode;

// Global access to context (for TaskScheduler callbacks)
static BridgeContext* g_bridgeContext = nullptr;

void setBridgeContext(BridgeContext* ctx) {
    g_bridgeContext = ctx;
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
        log_msg("CRITICAL: Low memory! Free: " + String(freeHeap) + " bytes", LOG_ERROR);
    } else if (freeHeap < 20000) {
        log_msg("Warning: Memory getting low. Free: " + String(freeHeap) + " bytes", LOG_WARNING);
    } else {
        log_msg("Memory: Free=" + String(freeHeap) + " bytes, Min=" + String(minFreeHeap) + " bytes", LOG_DEBUG);
    }
}

// Helper function for device2 role names
const char* getDevice2RoleName(uint8_t role) {
  switch(role) {
    case D2_NONE: return "Disabled";
    case D2_UART2: return "UART2";
    case D2_USB: return "USB";
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

// Thread-safe function to update shared statistics using critical sections
void updateSharedStats(unsigned long device1Rx, unsigned long device1Tx,
                      unsigned long device2Rx, unsigned long device2Tx,
                      unsigned long device3Rx, unsigned long device3Tx,
                      unsigned long lastActivity) {
  enterStatsCritical();
  uartStats.device1RxBytes = device1Rx;
  uartStats.device1TxBytes = device1Tx;
  uartStats.device2RxBytes = device2Rx;
  uartStats.device2TxBytes = device2Tx;
  uartStats.device3RxBytes = device3Rx;
  uartStats.device3TxBytes = device3Tx;
  if (lastActivity > 0) {
    uartStats.lastActivityTime = lastActivity;
  }
  exitStatsCritical();
}

// Reset all statistics using critical sections
void resetStatistics(UartStats* stats) {
  enterStatsCritical();
  stats->device1RxBytes = 0;
  stats->device1TxBytes = 0;
  stats->device2RxBytes = 0;
  stats->device2TxBytes = 0;
  stats->device3RxBytes = 0;
  stats->device3TxBytes = 0;
  stats->lastActivityTime = 0;
  stats->deviceStartTime = millis();
  stats->totalUartPackets = 0;
  exitStatsCritical();
}

// Separate functions for TaskScheduler

void runBridgeActivityLog() {
    if (!g_bridgeContext) return;
    
    String mode = (*g_bridgeContext->system.bridgeMode == BRIDGE_NET) ? "Network" : "Standalone";
    log_msg("UART bridge alive [" + mode + " mode]: D1 RX=" + 
            String(*g_bridgeContext->stats.device1RxBytes) +
            " TX=" + String(*g_bridgeContext->stats.device1TxBytes) + " bytes" +
            (*g_bridgeContext->diagnostics.totalDroppedBytes > 0 ? 
             ", dropped=" + String(*g_bridgeContext->diagnostics.totalDroppedBytes) : ""), 
            LOG_DEBUG);
}

void runStackDiagnostics() {
    if (!g_bridgeContext) return;
    
    UBaseType_t stackFree = uxTaskGetStackHighWaterMark(NULL);
    log_msg("UART task: Stack free=" + String(stackFree * 4) +
            " bytes, Heap free=" + String(ESP.getFreeHeap()) +
            " bytes, Largest block=" + String(ESP.getMaxAllocHeap()) + " bytes", 
            LOG_DEBUG);
    
    // Add DMA diagnostics if available
    UartDMA* dma = static_cast<UartDMA*>(g_bridgeContext->interfaces.uartBridgeSerial);
    if (dma && dma->isInitialized()) {
        log_msg("DMA stats: RX=" + String(dma->getRxBytesTotal()) + 
                " TX=" + String(dma->getTxBytesTotal()) +
                ", Overruns=" + String(dma->getOverrunCount()), LOG_DEBUG);
    }
}

void runDroppedDataStats() {
    if (!g_bridgeContext) return;
    
    // Only log if there's something to report
    if (*g_bridgeContext->diagnostics.droppedBytes > 0) {
        // For regular drops (buffer full)
        if (*g_bridgeContext->diagnostics.maxDropSize > 0) {
            log_msg("USB buffer full: dropped " + String(*g_bridgeContext->diagnostics.droppedBytes) + " bytes in " +
                    String(*g_bridgeContext->diagnostics.dropEvents) + " events (total: " + String(*g_bridgeContext->diagnostics.totalDroppedBytes) +
                    " bytes), max packet: " + String(*g_bridgeContext->diagnostics.maxDropSize) + " bytes", LOG_INFO);
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
            
            log_msg("USB timeout: dropped " + String(*g_bridgeContext->diagnostics.droppedBytes) + " bytes in " +
                    String(*g_bridgeContext->diagnostics.dropEvents) + " events (total: " + String(*g_bridgeContext->diagnostics.totalDroppedBytes) +
                    " bytes). " + sizes, LOG_INFO);
            
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
    
    // Device3 task (if exists)
    extern TaskHandle_t device3TaskHandle;
    if (device3TaskHandle) {
        UBaseType_t device3Stack = uxTaskGetStackHighWaterMark(device3TaskHandle);
        msg += " Dev3=" + String(device3Stack * 4) + "B";
    }
    
    // Add heap info
    msg += ", Heap=" + String(ESP.getFreeHeap()) + "B";
    msg += ", MaxBlock=" + String(ESP.getMaxAllocHeap()) + "B";
    
    log_msg(msg, LOG_DEBUG);
}

// Log DMA statistics for a UART interface
void logDmaStatistics(UartInterface* uartSerial) {
  UartDMA* dma = static_cast<UartDMA*>(uartSerial);
  if (dma && dma->isInitialized()) {
    log_msg("DMA stats: RX=" + String(dma->getRxBytesTotal()) + 
            " TX=" + String(dma->getTxBytesTotal()) +
            ", Overruns=" + String(dma->getOverrunCount()), LOG_DEBUG);
  }
}

#ifdef SERIAL_LOG_ENABLE
// Force output to serial for critical debugging
// Useful when normal logging system is not available
void forceSerialLog(const String& message) {
    static bool serialInited = false;
    if (!serialInited) {
        Serial.begin(115200);
        vTaskDelay(pdMS_TO_TICKS(100));
        serialInited = true;
    }
    Serial.printf("FORCE_LOG: %s\n", message.c_str());
    Serial.flush();
}
#endif

