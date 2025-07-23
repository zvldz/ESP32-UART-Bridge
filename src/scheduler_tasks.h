#ifndef SCHEDULER_TASKS_H
#define SCHEDULER_TASKS_H

// CRITICAL: DO NOT REMOVE - Required for TaskScheduler to avoid multiple definition errors
#define _TASK_INLINE inline

#include <TaskScheduler.h>

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

#endif // SCHEDULER_TASKS_H