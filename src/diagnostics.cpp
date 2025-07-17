#include "diagnostics.h"
#include "logging.h"
#include "crashlog.h"
#include "esp_system.h"
#include <Arduino.h>

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
        
        log_msg("Uptime: " + String(millis()/1000) + "s", LOG_DEBUG);
        lastCheck = millis();
    }
}