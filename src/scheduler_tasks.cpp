#include "scheduler_tasks.h"
#include <TaskScheduler.h>
#include "diagnostics.h"
#include "crashlog.h"
#include "logging.h"
#include "defines.h"
#include "types.h"
#include "web_interface.h"
#include <DNSServer.h>

// External objects
extern Config config;
extern BridgeMode bridgeMode;
extern DNSServer* dnsServer;

// Global scheduler instance
Scheduler taskScheduler;

// All task declarations (static to keep them local)
static Task tSystemDiagnostics(10000, TASK_FOREVER, nullptr);
static Task tCrashlogUpdate(5000, TASK_FOREVER, nullptr);
static Task tBridgeActivity(30000, TASK_FOREVER, nullptr);
static Task tAllStacksDiagnostics(5000, TASK_FOREVER, nullptr);  // Renamed - shows all stacks
static Task tDroppedDataStats(5000, TASK_FOREVER, nullptr);
static Task tWiFiTimeout(WIFI_TIMEOUT, TASK_ONCE, nullptr);
static Task tUpdateStats(UART_STATS_UPDATE_INTERVAL_MS, TASK_FOREVER, nullptr);
static Task tUpdateStatsDevice3(UART_STATS_UPDATE_INTERVAL_MS * 2, TASK_FOREVER, nullptr);
static Task tDnsProcess(150, TASK_FOREVER, nullptr);

// External function declarations
extern void updateMainStats();        // Will be implemented in uartbridge.cpp
extern void updateDevice3Stats();     // Will be implemented in uartbridge.cpp

void initializeScheduler() {
    // Set all callbacks
    tSystemDiagnostics.set(10000, TASK_FOREVER, []{ 
        systemDiagnostics(); 
    });
    
    tCrashlogUpdate.set(5000, TASK_FOREVER, []{ 
        crashlog_update_variables(); 
    });
    
    tBridgeActivity.set(30000, TASK_FOREVER, []{ 
        runBridgeActivityLog(); 
    });
    
    tAllStacksDiagnostics.set(5000, TASK_FOREVER, []{ 
        runAllStacksDiagnostics();  // Unified function for all stacks
    });
    
    tDroppedDataStats.set(5000, TASK_FOREVER, []{ 
        runDroppedDataStats(); 
    });
    
    tWiFiTimeout.set(WIFI_TIMEOUT, TASK_ONCE, []{ 
        log_msg("WiFi timeout - switching to standalone mode", LOG_INFO);
        ESP.restart();
    });
    
    tUpdateStats.set(UART_STATS_UPDATE_INTERVAL_MS, TASK_FOREVER, []{ 
        updateMainStats(); 
    });
    
    tUpdateStatsDevice3.set(UART_STATS_UPDATE_INTERVAL_MS * 2, TASK_FOREVER, []{ 
        updateDevice3Stats(); 
    });
    
    tDnsProcess.set(150, TASK_FOREVER, []{ 
        if (dnsServer) {
            dnsServer->processNextRequest();
        }
    });
    
    // Initialize scheduler
    taskScheduler.init();
    
    // Add all tasks to scheduler
    taskScheduler.addTask(tSystemDiagnostics);
    taskScheduler.addTask(tCrashlogUpdate);
    taskScheduler.addTask(tBridgeActivity);
    taskScheduler.addTask(tAllStacksDiagnostics);
    taskScheduler.addTask(tDroppedDataStats);
    taskScheduler.addTask(tWiFiTimeout);
    taskScheduler.addTask(tUpdateStats);
    taskScheduler.addTask(tUpdateStatsDevice3);
    taskScheduler.addTask(tDnsProcess);
    
    // Enable basic tasks that run in all modes
    tSystemDiagnostics.enable();
    tCrashlogUpdate.enable();
    
    // Distribute tasks over time to prevent simultaneous execution
    // This prevents all tasks from running at t=0, t=30s, t=60s, etc.
    tBridgeActivity.delay(5000);      // Start after 5 seconds
    tAllStacksDiagnostics.delay(1000); // Start after 1 second
    tDroppedDataStats.delay(2500);     // Start after 2.5 seconds
    tUpdateStatsDevice3.delay(250);    // Start after 0.25 seconds
}

void enableStandaloneTasks() {
    // Enable tasks for standalone bridge mode
    tBridgeActivity.enable();
    tAllStacksDiagnostics.enable();
    tDroppedDataStats.enable();
    tUpdateStats.enable();
    
    // Enable Device3 stats only if device is active
    if (config.device3.role != D3_NONE) {
        tUpdateStatsDevice3.enable();
    }
    
    // Disable network mode tasks
    tDnsProcess.disable();
    tWiFiTimeout.disable();
}

void enableNetworkTasks(bool temporaryNetwork) {
    // Enable tasks for network setup mode
    tDnsProcess.enable();
    
    // Only start timeout for temporary network (setup AP)
    if (temporaryNetwork) {
        startWiFiTimeout();
    }
    
    // Bridge tasks continue to work in network mode
    tBridgeActivity.enable();
    tAllStacksDiagnostics.enable();
    tDroppedDataStats.enable();
    tUpdateStats.enable();
    
    if (config.device3.role != D3_NONE) {
        tUpdateStatsDevice3.enable();
    }
}

void disableAllTasks() {
    // Disable all tasks except basic ones
    tBridgeActivity.disable();
    tAllStacksDiagnostics.disable();
    tDroppedDataStats.disable();
    tUpdateStats.disable();
    tUpdateStatsDevice3.disable();
    tDnsProcess.disable();
    tWiFiTimeout.disable();
}

void startWiFiTimeout() {
    tWiFiTimeout.restartDelayed();
}

void cancelWiFiTimeout() {
    tWiFiTimeout.disable();
}