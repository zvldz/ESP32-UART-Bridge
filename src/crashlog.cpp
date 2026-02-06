#include "crashlog.h"
#include "logging.h"
#include "defines.h"
#include <LittleFS.h>
#include <ArduinoJson.h>


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

// Validate string contains only printable ASCII (JSON-safe)
static bool isValidAsciiString(const char* str, size_t maxLen) {
    if (!str || str[0] == '\0') return false;
    for (size_t i = 0; i < maxLen && str[i] != '\0'; i++) {
        unsigned char c = str[i];
        if (c < 32 || c > 126) return false;
    }
    return true;
}

// RTC validation limits (garbage detection after power loss)
static const uint32_t MAX_VALID_UPTIME = 31536000;  // 1 year in seconds
static const uint32_t MAX_VALID_HEAP = 1048576;     // 1MB

// RTC variables for crash logging (survive reset but not power loss)
RTC_NOINIT_ATTR uint32_t g_last_heap;
RTC_NOINIT_ATTR uint32_t g_last_uptime;
RTC_NOINIT_ATTR uint32_t g_min_heap;
RTC_NOINIT_ATTR uint32_t g_last_timestamp;  // Unix epoch at last update (0 = no sync)
RTC_NOINIT_ATTR char g_last_version[16];    // Store version string at crash time

// Browser time sync state (RAM only, lost on reboot)
static uint32_t s_time_epoch = 0;        // Browser epoch at sync moment
static uint32_t s_time_sync_millis = 0;  // millis() at sync moment
static bool s_time_synced = false;        // Accept only first sync per boot

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

        // Validate RTC values (garbage if crash before first update or after power loss)
        newEntry["uptime"] = (g_last_uptime <= MAX_VALID_UPTIME) ? g_last_uptime : 0;
        newEntry["heap"] = (g_last_heap <= MAX_VALID_HEAP) ? g_last_heap : 0;
        newEntry["min_heap"] = (g_min_heap <= MAX_VALID_HEAP) ? g_min_heap : 0;

        // Timestamp from browser sync (0 = no sync happened before crash)
        static const uint32_t MIN_VALID_EPOCH = 1700000000;  // ~Nov 2023
        newEntry["time"] = (g_last_timestamp >= MIN_VALID_EPOCH) ? g_last_timestamp : 0;

        // Save firmware version at crash time (validate to prevent JSON corruption)
        if (isValidAsciiString(g_last_version, sizeof(g_last_version))) {
            newEntry["version"] = g_last_version;
        }

        // Try to get coredump details if available
        esp_core_dump_summary_t summary;
        if (esp_core_dump_get_summary(&summary) == ESP_OK) {
            JsonObject panic = newEntry["panic"].to<JsonObject>();

            // Program counter where crash occurred
            char pc_buf[12];
            snprintf(pc_buf, sizeof(pc_buf), "0x%08x", summary.exc_pc);
            panic["pc"] = pc_buf;

            // Task name (validate to prevent JSON corruption from garbage memory)
            if (isValidAsciiString(summary.exc_task, sizeof(summary.exc_task))) {
                panic["task"] = summary.exc_task;
            }

            // Exception cause and address
            char exc_buf[12];
            snprintf(exc_buf, sizeof(exc_buf), "0x%08x", summary.ex_info.exc_cause);
            panic["excause"] = exc_buf;

            snprintf(exc_buf, sizeof(exc_buf), "0x%08x", summary.ex_info.exc_vaddr);
            panic["excvaddr"] = exc_buf;

            // Backtrace array
            JsonArray backtrace = panic["backtrace"].to<JsonArray>();
            for (int i = 0; i < summary.exc_bt_info.depth && i < 16; i++) {
                char bt_buf[12];
                snprintf(bt_buf, sizeof(bt_buf), "0x%08x", summary.exc_bt_info.bt[i]);
                backtrace.add(bt_buf);
            }

            // Mark if backtrace was truncated
            if (summary.exc_bt_info.depth >= 16) {
                panic["truncated"] = true;
            }
            if (summary.exc_bt_info.corrupted) {
                panic["corrupted"] = true;
            }

            // Erase coredump after successful read
            esp_core_dump_image_erase();
            log_msg(LOG_INFO, "Coredump captured: PC=%s, Task=%s", pc_buf, summary.exc_task);
        }

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

    // Parse and re-serialize to ensure clean JSON output
    // (raw file may contain control characters that break browser JSON.parse)
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        // File is corrupted - delete it and return empty
        LittleFS.remove(CRASHLOG_FILE_PATH);
        log_msg(LOG_WARNING, "Crashlog file corrupted, deleted: %s", error.c_str());
        return getFallbackJson();
    }

    // Re-serialize to get clean JSON
    String json;
    serializeJson(doc, json);
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

// Update RTC variables periodically
void crashlog_update_variables() {
    g_last_heap = ESP.getFreeHeap();
    g_last_uptime = millis() / 1000;

    // Compute current timestamp from browser sync reference
    if (s_time_synced) {
        g_last_timestamp = s_time_epoch + (millis() - s_time_sync_millis) / 1000;
    }

    // Track minimum heap
    if (g_last_heap < g_min_heap) {
        g_min_heap = g_last_heap;
    }

    // Store current firmware version for crash logging
    strncpy(g_last_version, DEVICE_VERSION, sizeof(g_last_version) - 1);
    g_last_version[sizeof(g_last_version) - 1] = '\0';
}

// Sync time from browser (once per boot)
void crashlog_sync_time(uint32_t browser_epoch) {
    if (s_time_synced) return;
    s_time_epoch = browser_epoch;
    s_time_sync_millis = millis();
    s_time_synced = true;
    g_last_timestamp = browser_epoch;
    log_msg(LOG_INFO, "Time synced from browser: %u", browser_epoch);
}