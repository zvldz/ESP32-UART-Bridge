#ifndef SCHEDULER_TASKS_H
#define SCHEDULER_TASKS_H

// CRITICAL: DO NOT REMOVE - Required for TaskScheduler to avoid multiple definition errors
#define _TASK_INLINE inline

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
//#pragma GCC diagnostic ignored "-Wdeprecated-volatile"
#include <TaskScheduler.h>
#pragma GCC diagnostic pop

// Global scheduler instance
extern Scheduler taskScheduler;

// Initialize the task scheduler
void initializeScheduler();

// Task control functions
void enableStandaloneTasks();      // Enable tasks for standalone mode
void enableNetworkTasks(bool temporaryNetwork);  // Enable tasks for network mode
void disableAllTasks();

// WiFi timeout control
void startWiFiTimeout();
void cancelWiFiTimeout();

// Scheduled reboot
void scheduleReboot(unsigned long delayMs = 3000);

#endif // SCHEDULER_TASKS_H