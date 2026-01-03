// Quick Reset Detection - only for boards without BOOT button (e.g., MiniKit)
// Detects 3 quick manual resets to activate network mode (replaces triple-click on BOOT)
#if defined(BOARD_MINIKIT_ESP32)

#include "quick_reset.h"
#include "logging.h"
#include <esp_system.h>
#include <Preferences.h>

#define QUICK_RESET_THRESHOLD_MS  3000   // Max uptime to count as "quick"
#define QUICK_RESET_COUNT_TARGET  2      // 2 quick resets = 3 button presses
#define NVS_NAMESPACE "quickreset"

static bool forceNetworkMode = false;
static Preferences prefs;

void quickResetInit() {
    esp_reset_reason_t reason = esp_reset_reason();

    // Read stored values from NVS
    prefs.begin(NVS_NAMESPACE, false);
    unsigned long lastUptime = prefs.getULong("uptime", 0);
    int quickResetCount = prefs.getInt("count", 0);

    // MiniKit RESET button gives POWERON (not EXT), so we accept any reason
    // Crashes (PANIC, WDT, BROWNOUT) clear the counter to prevent boot loop triggers
    bool isCrash = (reason == ESP_RST_PANIC ||
                    reason == ESP_RST_INT_WDT ||
                    reason == ESP_RST_TASK_WDT ||
                    reason == ESP_RST_WDT ||
                    reason == ESP_RST_BROWNOUT);

    // Crash - clear counter (boot loop protection)
    if (isCrash) {
        prefs.putInt("count", 0);
        prefs.putULong("uptime", 0);
        prefs.end();
        return;
    }

    // Check if previous session was short (quick reset)
    if (lastUptime > 0 && lastUptime < QUICK_RESET_THRESHOLD_MS) {
        quickResetCount++;
    } else {
        // First boot OR previous session ran long enough - reset counter
        quickResetCount = 0;
    }

    // Check if we hit the target
    if (quickResetCount >= QUICK_RESET_COUNT_TARGET) {
        forceNetworkMode = true;
        quickResetCount = 0;  // Reset for next time
    }

    // Save updated count and reset uptime for this session
    prefs.putInt("count", quickResetCount);
    prefs.putULong("uptime", 0);
    prefs.end();
}

bool quickResetDetected() {
    return forceNetworkMode;
}

void quickResetUpdateUptime() {
    // Update NVS with current uptime, but only during detection window
    // After threshold passed, no need to keep updating
    unsigned long now = millis();
    if (now < QUICK_RESET_THRESHOLD_MS + 500) {  // +500ms safety margin
        prefs.begin(NVS_NAMESPACE, false);
        prefs.putULong("uptime", now);
        prefs.end();
    }
}

#endif // BOARD_MINIKIT_ESP32
