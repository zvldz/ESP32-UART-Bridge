#include "scheduler_tasks.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include <TaskScheduler.h>
#pragma GCC diagnostic pop
#include "diagnostics.h"
#include "crashlog.h"
#include "logging.h"
#include "defines.h"
#include "types.h"
#include "web/web_interface.h"
#include "wifi/wifi_manager.h"
#include "protocols/protocol_pipeline.h"
#include "circular_buffer.h"
#include "leds.h"
#include "protocols/sbus_router.h"
#include "device_init.h"

// External objects
extern Config config;
extern BridgeMode bridgeMode;

// Global scheduler instance
Scheduler taskScheduler;

// All task declarations (static to keep them local)
static Task tSystemDiagnostics(10000, TASK_FOREVER, nullptr);
static Task tCrashlogUpdate(5000, TASK_FOREVER, nullptr);
static Task tBridgeActivity(30000, TASK_FOREVER, nullptr);
static Task tAllStacksDiagnostics(5000, TASK_FOREVER, nullptr);
static Task tDroppedDataStats(5000, TASK_FOREVER, nullptr);
static Task tWiFiTimeout(WIFI_TIMEOUT, TASK_ONCE, nullptr);
static Task tDnsProcess(150, TASK_FOREVER, nullptr);
static Task tRebootDevice(TASK_IMMEDIATE, TASK_ONCE, nullptr);
static Task tUdpLoggerTask(100, TASK_FOREVER, nullptr);
Task tLedMonitor(50, TASK_FOREVER, nullptr);  // 50ms interval for LED monitoring - exported for external control
Task tSbusRouterTick(10, TASK_FOREVER, nullptr);  // Every 10ms - exported for external control

// Simple snapshot for LED comparison (not atomic)
struct LedSnapshot {
    unsigned long d1_rx = 0;
    unsigned long d2_rx = 0;
    unsigned long d3_tx = 0;
    unsigned long d3_rx = 0;
};
static LedSnapshot ledPrevSnapshot;


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
        // Check if firmware update is in progress
        extern SystemState systemState;
        if (systemState.firmwareUpdateInProgress) {
            log_msg(LOG_INFO, "WiFi timeout skipped - firmware update in progress");
            return;  // Don't reboot during firmware update
        }

        log_msg(LOG_INFO, "WiFi timeout - switching to standalone mode");
        ESP.restart();
    });
    
    
    tDnsProcess.set(150, TASK_FOREVER, []{ 
        if (dnsServer) {
            dnsServer->processNextRequest();
        }
    });
    
    tRebootDevice.set(TASK_IMMEDIATE, TASK_ONCE, []{ 
        log_msg(LOG_INFO, "Executing scheduled reboot");
        ESP.restart();
    });
    
    // UDP Logger task - copies log lines to Pipeline
    tLedMonitor.set(50, TASK_FOREVER, []{ 
        // Only show data activity LEDs in standalone mode
        if (bridgeMode != BRIDGE_STANDALONE) {
            return;  // Network mode uses WiFi state LEDs
        }
        
        // Device 1 UART RX activity
        const auto d1_rx = g_deviceStats.device1.rxBytes.load(std::memory_order_relaxed);
        if (d1_rx > ledPrevSnapshot.d1_rx) {
            led_notify_uart_rx();  // Blue LED
        }
        ledPrevSnapshot.d1_rx = d1_rx;
        
        // Device 2 activity (USB or UART2)
        if (config.device2.role == D2_USB) {
            const auto d2_rx = g_deviceStats.device2.rxBytes.load(std::memory_order_relaxed);
            if (d2_rx > ledPrevSnapshot.d2_rx) {
                led_notify_usb_rx();  // Green LED
            }
            ledPrevSnapshot.d2_rx = d2_rx;
        } else if (config.device2.role == D2_UART2) {
            const auto d2_rx = g_deviceStats.device2.rxBytes.load(std::memory_order_relaxed);
            if (d2_rx > ledPrevSnapshot.d2_rx) {
                led_notify_uart_rx();  // Blue LED (same as Device1)
            }
            ledPrevSnapshot.d2_rx = d2_rx;
        }
        
        // Device 3 TX activity
        if (config.device3.role != D3_NONE) {
            const auto d3_tx = g_deviceStats.device3.txBytes.load(std::memory_order_relaxed);
            if (d3_tx > ledPrevSnapshot.d3_tx) {
                led_notify_device3_tx();  // Magenta LED
            }
            ledPrevSnapshot.d3_tx = d3_tx;
            
            // Device 3 RX activity (Bridge mode only)
            if (config.device3.role == D3_UART3_BRIDGE) {
                const auto d3_rx = g_deviceStats.device3.rxBytes.load(std::memory_order_relaxed);
                if (d3_rx > ledPrevSnapshot.d3_rx) {
                    led_notify_device3_rx();  // Yellow LED
                }
                ledPrevSnapshot.d3_rx = d3_rx;
            }
        }
    });

    tSbusRouterTick.set(10, TASK_FOREVER, []{
        SbusRouter::getInstance()->tick();
    });

    tUdpLoggerTask.set(100, TASK_FOREVER, []{
        if (config.device4.role != D4_LOG_NETWORK) return;
        
        extern uint8_t udpLogBuffer[];
        extern int udpLogHead;
        extern int udpLogTail;
        extern SemaphoreHandle_t udpLogMutex;
        // Get bridge context via diagnostics
        BridgeContext* g_bridgeContext = getBridgeContext();
        
        // Static variables to preserve state between calls
        static uint8_t lineBuffer[256];
        static size_t lineLen = 0;
        static uint32_t lastFlushMs = 0;
        
        if (!g_bridgeContext || !udpLogMutex) return;
        
        // Check WiFi readiness for data transmission
        if (!wifiIsReady()) {
            // Clear buffers when WiFi is down
            if (xSemaphoreTake(udpLogMutex, 0) == pdTRUE) {
                udpLogHead = 0;
                udpLogTail = 0;
                xSemaphoreGive(udpLogMutex);
            }
            lineLen = 0;  // Reset line buffer
            return;
        }
        
        // Count available lines
        uint32_t lineCount = 0;
        if (xSemaphoreTake(udpLogMutex, 0) == pdTRUE) {
            int tempTail = udpLogTail;
            while (tempTail != udpLogHead) {
                if (udpLogBuffer[tempTail] == '\n') lineCount++;
                tempTail = (tempTail + 1) % UDP_LOG_BUFFER_SIZE;
            }
            xSemaphoreGive(udpLogMutex);
        }
        
        uint32_t now = millis();
        bool shouldFlush = false;
        if (lineCount >= 10 ||
            (lineCount > 0 && (now - lastFlushMs) >= 100)) {
            shouldFlush = true;
        }
        
        if (shouldFlush && xSemaphoreTake(udpLogMutex, 10) == pdTRUE) {
            // Get log buffer from context
            CircularBuffer* inputBuffer = g_bridgeContext->buffers.logBuffer;
            
            if (!inputBuffer) {
                log_msg(LOG_ERROR, "Log buffer not available!");
                xSemaphoreGive(udpLogMutex);
                return;
            }
            
            while (udpLogTail != udpLogHead && lineLen < sizeof(lineBuffer)) {
                uint8_t byte = udpLogBuffer[udpLogTail];
                lineBuffer[lineLen++] = byte;
                udpLogTail = (udpLogTail + 1) % UDP_LOG_BUFFER_SIZE;
                
                if (byte == '\n') {
                    // Write complete line
                    inputBuffer->write(lineBuffer, lineLen);
                    lineLen = 0;
                    if (--lineCount == 0) break;
                }
            }
            
            // Handle incomplete line that filled the buffer
            if (lineLen >= sizeof(lineBuffer)) {
                // Force flush as incomplete/broken line
                inputBuffer->write(lineBuffer, lineLen);
                lineLen = 0;
            }
            
            // Partial line kept for next iteration

            xSemaphoreGive(udpLogMutex);
            lastFlushMs = now;
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
    taskScheduler.addTask(tDnsProcess);
    taskScheduler.addTask(tRebootDevice);
    taskScheduler.addTask(tUdpLoggerTask);
    taskScheduler.addTask(tLedMonitor);
    taskScheduler.addTask(tSbusRouterTick);

    // Enable basic tasks that run in all modes
    tSystemDiagnostics.enable();
    tCrashlogUpdate.enable();

    // Distribute tasks over time to prevent simultaneous execution
    // This prevents all tasks from running at t=0, t=30s, t=60s, etc.
    tBridgeActivity.delay(5000);      // Start after 5 seconds
    tAllStacksDiagnostics.delay(1000); // Start after 1 second
    tDroppedDataStats.delay(2500);     // Start after 2.5 seconds
}

void enableStandaloneTasks() {
    // Enable tasks for standalone bridge mode
    tBridgeActivity.enable();
    tAllStacksDiagnostics.enable();
    tDroppedDataStats.enable();
    tLedMonitor.enable();  // Enable LED monitoring in standalone mode

    // Enable SBUS Router tick ONLY for UDP RX (Timing Keeper needs it)
    if (config.device4.role == D4_SBUS_UDP_RX) {
        tSbusRouterTick.enable();
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
    } else {
        // Permanent network mode - ensure timeout is disabled
        tWiFiTimeout.disable();
    }

    // Bridge tasks continue to work in network mode
    tBridgeActivity.enable();
    tLedMonitor.disable();  // Disable LED monitoring in network mode
    tAllStacksDiagnostics.enable();
    tDroppedDataStats.enable();

    // Enable SBUS Router tick ONLY for UDP RX (Timing Keeper needs it)
    if (config.device4.role == D4_SBUS_UDP_RX) {
        tSbusRouterTick.enable();
    }

    // Enable UDP Logger task if Device 4 is in Logger mode
    if (config.device4.role == D4_LOG_NETWORK) {
        tUdpLoggerTask.enable();
    }

}

void disableAllTasks() {
    // Disable all tasks except basic ones
    tBridgeActivity.disable();
    tAllStacksDiagnostics.disable();
    tDroppedDataStats.disable();
    tDnsProcess.disable();
    tWiFiTimeout.disable();
}

void startWiFiTimeout() {
    tWiFiTimeout.restartDelayed();
}

void cancelWiFiTimeout() {
    tWiFiTimeout.disable();
}

void scheduleReboot(unsigned long delayMs) {
    log_msg(LOG_INFO, "Device reboot scheduled in %lums", delayMs);
    
    // Cancel WiFi timeout if it was active
    cancelWiFiTimeout();
    
    // Start reboot task with delay
    tRebootDevice.restartDelayed(delayMs);
}