#include "diagnostics.h"
#include "logging.h"
#include "crashlog.h"
#include "esp_system.h"
#include "uart_dma.h"
#include "types.h"
#include <Arduino.h>

// External object from main.cpp
extern UartStats uartStats;

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
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 10000) { // Every 10 seconds
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

        lastCheck = millis();
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

// Log bridge activity periodically
void logBridgeActivity(BridgeContext* ctx, DeviceMode currentMode) {
  if (millis() - *ctx->timing.lastPeriodicStats > 30000) {
    String mode = (currentMode == MODE_CONFIG) ? "WiFi" : "Normal";
    log_msg("UART bridge alive [" + mode + " mode]: D1 RX=" + String(*ctx->stats.device1RxBytes) +
            " TX=" + String(*ctx->stats.device1TxBytes) + " bytes" +
            (*ctx->diagnostics.totalDroppedBytes > 0 ? ", dropped=" + String(*ctx->diagnostics.totalDroppedBytes) : ""), LOG_DEBUG);
    *ctx->timing.lastPeriodicStats = millis();
  }
}

// Log stack diagnostics
void logStackDiagnostics(BridgeContext* ctx) {
  if (millis() - *ctx->timing.lastStackCheck > 5000) {
    UBaseType_t stackFree = uxTaskGetStackHighWaterMark(NULL);
    log_msg("UART task: Stack free=" + String(stackFree * 4) +
            " bytes, Heap free=" + String(ESP.getFreeHeap()) +
            " bytes, Largest block=" + String(ESP.getMaxAllocHeap()) + " bytes", LOG_DEBUG);
    *ctx->timing.lastStackCheck = millis();
    
    // Add DMA diagnostics if available
    UartDMA* dma = static_cast<UartDMA*>(ctx->interfaces.uartBridgeSerial);
    if (dma && dma->isInitialized()) {
      log_msg("DMA stats: RX=" + String(dma->getRxBytesTotal()) + 
              " TX=" + String(dma->getTxBytesTotal()) +
              ", Overruns=" + String(dma->getOverrunCount()), LOG_DEBUG);
    }
  }
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

// Log dropped data statistics
void logDroppedDataStats(BridgeContext* ctx) {
  // Check if we should log dropped data
  if (*ctx->diagnostics.droppedBytes > 0 && millis() - *ctx->timing.lastDropLog > 5000) {
    // For regular drops (buffer full)
    if (*ctx->diagnostics.maxDropSize > 0) {
      log_msg("USB buffer full: dropped " + String(*ctx->diagnostics.droppedBytes) + " bytes in " +
              String(*ctx->diagnostics.dropEvents) + " events (total: " + String(*ctx->diagnostics.totalDroppedBytes) +
              " bytes), max packet: " + String(*ctx->diagnostics.maxDropSize) + " bytes", LOG_INFO);
      *ctx->diagnostics.maxDropSize = 0;  // Reset for next period
    }
    
    // For timeout drops
    int* timeoutDropSizes = ctx->diagnostics.timeoutDropSizes;
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
      
      log_msg("USB timeout: dropped " + String(*ctx->diagnostics.droppedBytes) + " bytes in " +
              String(*ctx->diagnostics.dropEvents) + " events (total: " + String(*ctx->diagnostics.totalDroppedBytes) +
              " bytes). " + sizes, LOG_INFO);
      
      // Clear timeout sizes for next period
      for (int i = 0; i < 10; i++) {
        timeoutDropSizes[i] = 0;
      }
    }
    
    *ctx->timing.lastDropLog = millis();
    *ctx->diagnostics.droppedBytes = 0;
    *ctx->diagnostics.dropEvents = 0;
  }
}

// Main periodic diagnostics update function
void updatePeriodicDiagnostics(BridgeContext* ctx, DeviceMode currentMode) {
  // Log bridge activity every 30 seconds
  logBridgeActivity(ctx, currentMode);
  
  // Log stack diagnostics every 5 seconds
  logStackDiagnostics(ctx);
  
  // Log dropped data statistics if any
  logDroppedDataStats(ctx);
}