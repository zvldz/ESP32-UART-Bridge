#include "crashlog.h"
#include "logging.h"
#include "defines.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// Crashlog constants
#define CRASHLOG_MAX_FILE_SIZE  4096

// Helper function to create empty JSON structure
static void createEmptyLog(JsonDocument& doc) {
    doc["total"] = 0;
    doc["entries"].to<JsonArray>();
}

// Helper function to get fallback JSON string
static const char* getFallbackJson() {
    return "{\"total\":0,\"entries\":[]}";
}

// Helper function to write JSON document to crashlog file
static bool writeJsonToFile(JsonDocument& doc) {
    File file = LittleFS.open(CRASHLOG_FILE_PATH, "w");
    if (file) {
        serializeJson(doc, file);
        file.close();
        return true;
    }
    return false;
}

// RTC variables for crash logging (survive reset but not power loss)
RTC_NOINIT_ATTR uint32_t g_last_heap;
RTC_NOINIT_ATTR uint32_t g_last_uptime;
RTC_NOINIT_ATTR uint32_t g_min_heap;

// Check reset reason and save crash if needed
void crashlog_check_and_save() {
    esp_reset_reason_t reason = esp_reset_reason();

    // Only log abnormal resets
    if (reason == ESP_RST_PANIC || reason == ESP_RST_TASK_WDT ||
        reason == ESP_RST_INT_WDT || reason == ESP_RST_WDT) {

        log_msg(LOG_ERROR, "System recovered from crash: %s", crashlog_get_reset_reason_string(reason).c_str());

        // Check if file exists, create if not
        if (!LittleFS.exists(CRASHLOG_FILE_PATH)) {
            // Create empty structure
            JsonDocument doc;
            createEmptyLog(doc);

            writeJsonToFile(doc);
        }

        // Read existing crash log
        JsonDocument doc;
        File file = LittleFS.open(CRASHLOG_FILE_PATH, "r");

        if (file) {
            // Check file size protection
            if (file.size() > CRASHLOG_MAX_FILE_SIZE) {
                log_msg(LOG_WARNING, "Crashlog too large, truncating");
                file.close();
                LittleFS.remove(CRASHLOG_FILE_PATH);

                // Create new empty file
                createEmptyLog(doc);
            } else {
                DeserializationError error = deserializeJson(doc, file);
                file.close();

                if (error) {
                    log_msg(LOG_WARNING, "Crashlog corrupted, reinitializing: %s", error.c_str());
                    LittleFS.remove(CRASHLOG_FILE_PATH);

                    // Create new structure
                    createEmptyLog(doc);
                }
            }
        } else {
            // File doesn't exist, create structure
            createEmptyLog(doc);
        }

        // Update crash count
        int totalCrashes = doc["total"] | 0;
        totalCrashes++;
        doc["total"] = totalCrashes;

        // Add new crash entry
        JsonArray entries = doc["entries"];
        if (!entries) {
            entries = doc["entries"].to<JsonArray>();
        }

        // Create new entry at the beginning
        JsonObject newEntry = entries.add<JsonObject>();
        newEntry["num"] = totalCrashes;
        newEntry["reason"] = crashlog_get_reset_reason_string(reason);
        newEntry["uptime"] = g_last_uptime;
        newEntry["heap"] = g_last_heap;
        newEntry["min_heap"] = g_min_heap;

        // Keep only last CRASHLOG_MAX_ENTRIES
        while (entries.size() > CRASHLOG_MAX_ENTRIES) {
            entries.remove(entries.size() - 1);  // Remove oldest
        }

        // Save updated log
        if (writeJsonToFile(doc)) {
            log_msg(LOG_INFO, "Crash #%d logged successfully", totalCrashes);
        } else {
            log_msg(LOG_ERROR, "Cannot write crashlog file");
        }

        // Reset min heap tracking for new session
        g_min_heap = 0xFFFFFFFF;
    } else {
        // Normal startup - just reset min heap tracking
        g_min_heap = 0xFFFFFFFF;
    }
}

// Get crash log as JSON string
String crashlog_get_json() {
    if (!LittleFS.exists(CRASHLOG_FILE_PATH)) {
        return getFallbackJson();
    }

    File file = LittleFS.open(CRASHLOG_FILE_PATH, "r");
    if (!file) {
        return getFallbackJson();
    }

    String json = file.readString();
    file.close();

    // Validate it's valid JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        return getFallbackJson();
    }

    return json;
}

// Clear crash history
void crashlog_clear() {
    if (LittleFS.exists(CRASHLOG_FILE_PATH)) {
        LittleFS.remove(CRASHLOG_FILE_PATH);
        log_msg(LOG_INFO, "Crash history cleared");
    }

    // Create empty file
    JsonDocument doc;
    createEmptyLog(doc);

    writeJsonToFile(doc);
}

// Convert reset reason to string
String crashlog_get_reset_reason_string(esp_reset_reason_t reason) {
    switch(reason) {
        case ESP_RST_POWERON:    return "POWERON";
        case ESP_RST_SW:         return "SW_RESET";
        case ESP_RST_PANIC:      return "PANIC";
        case ESP_RST_INT_WDT:    return "INT_WDT";
        case ESP_RST_TASK_WDT:   return "TASK_WDT";
        case ESP_RST_WDT:        return "WDT";
        case ESP_RST_DEEPSLEEP:  return "DEEPSLEEP";
        case ESP_RST_BROWNOUT:   return "BROWNOUT";
        case ESP_RST_SDIO:       return "SDIO";
        default:                 return "UNKNOWN";
    }
}

// Format uptime in human readable form
String crashlog_format_uptime(uint32_t seconds) {
    if (seconds < 60) {
        return String(seconds) + "s";
    } else if (seconds < 3600) {
        return String(seconds / 60) + "m";
    } else {
        uint32_t hours = seconds / 3600;
        uint32_t minutes = (seconds % 3600) / 60;
        return String(hours) + "h " + String(minutes) + "m";
    }
}

// Update RTC variables periodically
void crashlog_update_variables() {
    g_last_heap = ESP.getFreeHeap();
    g_last_uptime = millis() / 1000;

    // Track minimum heap
    if (g_last_heap < g_min_heap) {
        g_min_heap = g_last_heap;
    }
}