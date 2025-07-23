#ifndef SCHEDULER_TASKS_H
#define SCHEDULER_TASKS_H

#define _TASK_INLINE inline  // Force inline to avoid multiple definitions
#include <TaskScheduler.h>

// Global scheduler
extern Scheduler taskScheduler;

// Initialize all tasks
void initializeScheduler();

// Mode control
void enableRuntimeTasks();      // Enable tasks for normal/bridge mode
void enableSetupTasks();        // Enable tasks for WiFi config mode
void disableAllTasks();

// WiFi timeout control
void startWiFiTimeout();
void cancelWiFiTimeout();

// Statistics update functions (implemented in other files)
extern void updateMainStats();     // Implemented in uartbridge.cpp
extern void updateDevice3Stats();  // Implemented in uartbridge.cpp

#endif // SCHEDULER_TASKS_H