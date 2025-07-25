# CHANGELOG

## v2.6.0 (ESPAsyncWebServer Migration) - July 2025 ✅
- **Migrated to ESPAsyncWebServer** - Completed
  - **Libraries**: Updated to ESPAsyncWebServer v3.7.10 + AsyncTCP v3.4.5
  - **Template System**: Changed from custom `{{}}` to built-in `%PLACEHOLDER%` processor
  - **API Migration**: Converted all handlers to async (request parameter access, response sending)
  - **OTA Adaptation**: Redesigned file upload handling for async server
  - **Architecture**: Removed webServerTask - AsyncWebServer works without dedicated task
  - **Memory Benefits**: Better resource usage, no blocking operations
  - **Performance**: Non-blocking request handling, improved concurrent connections
  - **JavaScript Fixes**: Fixed Reset Statistics and Clear Crash History button handling
  - **Diagnostics**: Enhanced stack monitoring for WiFi/TCP tasks instead of web server task

## v2.5.8 (Permanent Network Mode) - July 2025 ✅
- **Permanent Network Mode Implementation** - Completed
  - Added `permanent_network_mode` configuration parameter in Config structure
  - Updated configuration version from 3 to 4 with automatic migration
  - Web interface: Added checkbox in WiFi Configuration section
  - Backend: Full configuration save/load support with proper validation
  - Boot logic: Automatic permanent network mode detection in `detectMode()`
  - Benefits:
    - Wi-Fi access point remains active until manually disabled
    - No timeout for permanent mode (only for triple-click temporary mode)
    - Configurable via web interface with clear user feedback
    - Backward compatible with existing configurations

- **Build System Enhancement** - Completed
  - Added `update_readme_version.py` script for automatic version synchronization
  - Script automatically updates README.md badge with version from defines.h
  - Integrated into PlatformIO build process as pre-build script
  - Prevents version mismatches between firmware and documentation
  - **Fixed**: Removed blocking `exit(0)` that prevented compilation and upload

## v2.5.7 (Device Init Refactoring) - July 2025 ✅
- **Refactor Device Initialization** - Completed
  - Migrated `uartbridge_init()` from `uartbridge.cpp` to `device_init.cpp`
  - Renamed `uartbridge_init()` to `initMainUART()` for consistency
  - Migrated `initDevices()` from `main.cpp` to `device_init.cpp`
  - Updated all function calls in `main.cpp` (2 occurrences)
  - Removed function declarations from original locations
  - Benefits:
    - Better code organization with all device init functions in one module
    - Consistent naming convention for initialization functions
    - Reduced file sizes: uartbridge.cpp (~55 lines), main.cpp (~30 lines)
  - Technical details:
    - Added necessary includes: `usb_interface.h`, `flow_control.h`, `freertos/semphr.h`
    - Made `g_usbInterface` global (was static) for cross-module access
    - Added `diagnostics.h` include for helper functions
    - Total lines moved: ~85 lines to device_init module

## v2.5.6 (Bridge Mode Renaming) - July 2025 ✅
- **Rename Device Modes to Bridge Modes** - Completed
  - Renamed `DeviceMode` enum to `BridgeMode`
  - Renamed `MODE_NORMAL` to `BRIDGE_STANDALONE`
  - Renamed `MODE_CONFIG` to `BRIDGE_NET`
  - Updated all references across 11 source files
  - Benefits:
    - Clear separation of bridge functionality vs network state
    - Future-ready for Device 4 permanent network modes
    - Better naming that accurately describes functionality
    - Flexible design supporting different network configurations

- **Update SystemState Structure** - Completed
  - Renamed `wifiAPActive` to `networkActive`
  - Renamed `wifiStartTime` to `networkStartTime`
  - Added `isTemporaryNetwork` flag for future Device 4 support
  - Allows differentiation between setup AP (temporary) and permanent network modes

- **Update All User-Visible Text** - Completed
  - Changed "normal mode" to "standalone mode" throughout
  - Changed "config mode" to "network mode" throughout
  - Changed "WiFi configuration" to "network setup" where appropriate
  - Simplified to just "Network Mode" (without "setup") for universal usage
  - Updated help documentation and web interface

- **Implementation Details** - Completed
  - Total changes: ~60 occurrences across 11 files
  - Added critical comment for `_TASK_INLINE` macro in scheduler_tasks.h
  - All mode checks updated to use new enum values
  - Function names updated (initNormalMode → initStandaloneMode, etc.)
  - TaskScheduler functions renamed (enableRuntimeTasks → enableStandaloneTasks)
  - Network timeout only active when `isTemporaryNetwork=true`

## v2.5.5 (Adaptive Buffer Optimization) - July 2025 ✅
- **Fix FIFO Overflow at 115200 baud** - Completed
  - Identified root cause: increased buffer size (512 bytes) causing USB bottleneck
  - Created graduated buffer sizing for smoother performance:
    - < 115200: 256 bytes (original size)
    - 115200: 288 bytes (optimal compromise)
    - 230400: 768 bytes
    - 460800: 1024 bytes
    - 921600+: 2048 bytes
  - Results at 115200 baud:
    - Reduced FIFO overflow from constant to rare (3-8 times per test)
    - Improved packet loss from 25% to 2-10%
    - Parameter download success rate: 83% (5 out of 6 attempts)
  - Determined 288 bytes as optimal for 115200 baud without flow control

- **Code Cleanup** - Completed
  - Fixed duplicated adaptive buffering code in `bridge_processing.h`
  - Now properly uses functions from `adaptive_buffer.h`
  - Removed ~55 lines of duplicated code
  - Added temporary diagnostics (marked for removal after testing)

## v2.5.4 (TaskScheduler Implementation) - July 2025 ✅
- **Implement TaskScheduler** - Completed
  - Added TaskScheduler library to replace manual timer checks
  - Created `scheduler_tasks.cpp/h` for centralized task management
  - Migrated all periodic tasks (diagnostics, statistics, LED updates)
  - Saved ~100 lines of timer management code
  - Benefits achieved:
    - Cleaner code structure
    - Easy to add/remove periodic tasks
    - Better timing accuracy
    - Reduced cognitive load
  - All tasks properly distributed in time to prevent simultaneous execution
  - Mode-specific task management (Runtime vs Setup modes)

## v2.5.3 (Phase 2 Code Refactoring - Hybrid Approach) - July 2025 ✅
- **Phase 2 Refactoring - Hybrid Approach** - Completed
  - Further refactored uartbridge.cpp from ~555 to ~250 lines
  - Created BridgeContext structure for clean parameter passing
  - Maintained performance with static inline functions
  
- **Extended types.h** - Completed
  - Added comprehensive BridgeContext structure
  - Groups all task-local variables logically
  - Added inline initBridgeContext() function
  - Forward declarations for UartInterface/UsbInterface
  
- **Enhanced Diagnostics Module** - Completed
  - Moved periodic diagnostics from uartbridge.cpp
  - Added logBridgeActivity(), logStackDiagnostics()
  - Added logDmaStatistics(), logDroppedDataStats()
  - Created updatePeriodicDiagnostics() main function
  - ~150 lines moved to diagnostics module
  
- **Created bridge_processing.h** - Completed
  - All data processing logic as inline functions
  - processDevice1Input(), processDevice2USB/UART()
  - processDevice3BridgeInput()
  - shouldYieldToWiFi() helper
  - ~200 lines of processing logic extracted
  
- **Created adaptive_buffer.h** - Completed
  - All adaptive buffering logic in one place
  - shouldTransmitBuffer(), transmitAdaptiveBuffer()
  - handleBufferTimeout()
  - calculateAdaptiveBufferSize() moved from device_init
  - ~120 lines of buffering logic extracted
  
- **Simplified Main Loop** - Completed
  - Clean, readable main loop with function calls
  - All complex logic in specialized modules
  - Performance preserved with inline functions
  - Ready for future extensions

## v2.5.2 (Phase 1 Code Refactoring) - July 2025 ✅
- **Refactor Large uartbridge.cpp File** - Phase 1 Complete
  - Original size: ~700 lines, reduced to ~555 lines
  - Created modular structure without breaking functionality
  
- **Flow Control Module** - Completed
  - Created `flow_control.h` and `flow_control.cpp`
  - Extracted `detectFlowControl()` and `getFlowControlStatus()` functions
  - ~50 lines of code moved to separate module
  - Clean separation of flow control logic

- **Device Initialization Module** - Completed
  - Created `device_init.h` and `device_init.cpp`
  - Extracted `initDevice2UART()` and `initDevice3()` functions
  - ~85 lines of code moved to separate module
  - Fixed linkage issues with proper extern declarations

- **Build System Verification** - Completed
  - Fixed static/extern conflicts for device2Serial
  - Added proper extern declarations in uartbridge.h
  - Project compiles successfully with new structure
  - All functionality preserved

## v2.5.1 (Web Interface Modularization) - July 2025 ✅
- **Split main.js into modules** - Completed
  - Created separate JS modules for better organization:
    - `utils.js` - Common utility functions
    - `device-config.js` - Device configuration UI
    - `form-utils.js` - Form handling and validation
    - `status-updates.js` - Status and statistics updates
    - `crash-log.js` - Crash log handling
    - `main.js` - Main initialization and coordination
  - Updated `web_interface.cpp` to serve all JS files
  - Updated `embed_html.py` to process all JS files
  - Added proper handlers in `web_interface.h`

- **Fix Reset Statistics button** - Completed
  - Removed confirmation dialog as requested
  - Fixed button state stuck on "Resetting..."
  - Button now properly returns to "Reset Statistics" after operation

- **OTA Update Safety** - Completed
  - Added Device 3 UART0 deinitialization before OTA update
  - Prevents conflicts during firmware update when Device 3 uses UART0
  - Made `device3Serial` accessible from `web_ota.cpp`
  - Ensures clean OTA update even with active Device 3

- **Code Cleanup** - Completed
  - Removed unused `lastWdtFeed` variable from `uartbridge.cpp`
  - Fixed legacy code remnants

## v2.5.0 (Complete ESP-IDF Migration + Performance) - July 2025 ✅
- **Complete ESP-IDF Migration** - Completed
  - Migrated Device 2 UART to ESP-IDF/DMA ✅
  - Migrated Device 3 UART to ESP-IDF/DMA ✅
  - Migrated UART logger to ESP-IDF/DMA ✅
  - Removed HardwareSerial dependencies completely ✅
  - Removed serial_config_decoder.h ✅

- **DMA Polling Mode Implementation** - Completed
  - Added polling mode support to UartDMA class ✅
  - Device 2/3 use polling mode (no event tasks) ✅
  - Added `pollEvents()` method for non-blocking operation ✅
  - Optimized for minimal resource usage ✅

- **Configuration Architecture Improvement** - Completed
  - Created `UartConfig` structure for UART parameters ✅
  - Separated DMA configuration from UART configuration ✅
  - Fixed scope conflicts with global config ✅
  - Clean parameter passing without global dependencies ✅

- **Dynamic Adaptive Buffer Sizing** - Completed
  - Buffer size now adjusts based on baud rate (256-2048 bytes) ✅
  - 256 bytes for ≤19200 baud
  - 512 bytes for ≤115200 baud
  - 1024 bytes for ≤460800 baud
  - 2048 bytes for >460800 baud
  - Prevents packet fragmentation at high speeds
  - Maintains low latency at all speeds

## v2.4.0 (ESP-IDF Migration) - July 2025 ✅
- **Remove Arduino Framework Dependencies for Device1** - Completed
  - Migrated Device 1 UART to ESP-IDF driver with DMA
  - Removed all conditional compilation (#ifdef USE_UART_DMA)
  - Deleted uart_arduino.cpp wrapper
  - Improved UART performance with hardware packet detection

- **Configuration System Update** - Completed
  - Updated Config struct to use ESP-IDF native types (uart_word_length_t, uart_parity_t, uart_stop_bits_t)
  - Increased config version to 3 with automatic migration
  - Added conversion functions between ESP-IDF enums and web interface strings

- **Code Cleanup and Optimization** - Completed
  - Created UsbBase class to eliminate code duplication between USB Host and Device
  - Simplified UartInterface with direct ESP-IDF configuration
  - Removed serial_config_decoder.h dependency for Device 1
  - Improved error handling and logging

- **Performance Improvements** - Completed
  - Hardware DMA for Device 1 UART with packet boundary detection
  - Zero-copy ring buffer implementation
  - Reduced CPU usage through event-driven architecture
  - Native ESP-IDF drivers for better performance
  - Increased DMA buffers and task priority for high-throughput scenarios

- **USB Implementation Decision** - Completed
  - Kept Arduino Serial for USB Device (stable and sufficient)
  - USB Host already uses ESP-IDF
  - Created common base class for code reuse
  - USB performance not critical compared to UART

## v2.3.3 (Performance Optimization) - July 2025 ✅
- **UART Bridge Performance Fix** - Completed
  - Fixed packet drops during MAVLink parameter downloads
  - Optimized device role checks with cached flags
  - Removed repeated config.deviceX.role comparisons in critical loop
  - Improved performance at 115200 baud and higher

- **Buffer Size Optimization** - Completed
  - Increased UART_BUFFER_SIZE from 256 to 1024 bytes
  - Added Serial.setTxBufferSize(1024) for USB interface
  - Removed baudrate condition - always use 1KB buffers
  - Reduced WiFi yield time from 5ms to 3ms

## v2.3.2 (Web Interface Migration) - July 2025 ✅
- **Web Interface Refactoring** - Completed
  - Migrated from embedded HTML strings to separate HTML/CSS/JS files
  - Created build-time processing with `embed_html.py` script
  - Reduced placeholders from 40+ to just {{CONFIG_JSON}}
  - Separated JavaScript into main.js and crash-log.js
  - HTML/CSS/JS files now editable with proper IDE support
  - C++ code reduced by ~3000 lines

## v2.3.1 - July 2025 ✅
- **Update Statistics System** - Completed
  - Replaced old `bytesUartToUsb`/`bytesUsbToUart` with per-device counters
  - Added `device1RxBytes`/`device1TxBytes`, `device2RxBytes`/`device2TxBytes`, `device3RxBytes`/`device3TxBytes`
  - Updated web interface to show statistics for all active devices
  - Shows device roles in statistics display

- **Code Cleanup** - Completed
  - Moved `updateSharedStats` and `resetStatistics` to diagnostics module
  - Removed legacy `if (currentMode == MODE_NORMAL || currentMode == MODE_CONFIG)` check
  - Fixed deprecated ArduinoJson `containsKey()` warning
  - Added helper functions `getDevice2RoleName()` and `getDevice3RoleName()`

- **Web Interface Updates** - Completed
  - Updated traffic statistics table to show per-device data
  - Dynamic show/hide of device rows based on active roles
  - Updated JavaScript to handle new JSON structure from `/status` endpoint
  - Fixed "Never" display for last activity

## v2.3.0 - July 2025 ✅
- **Remove DEBUG_MODE** - Completed
  - Removed all DEBUG_MODE checks from code
  - Bridge always active in all modes
  - Diagnostics converted to log levels
  
- **Remove CONFIG_FREERTOS_UNICORE** - Completed
  - Now only supports multi-core ESP32
  - UART task on core 0, Web task on core 1
  
- **Code Organization** - Completed
  - Extracted diagnostics to separate module (diagnostics.cpp/h)
  - Moved system utilities to system_utils.cpp/h
  - Moved RTC variables and crash update to crashlog.cpp/h
  - main.cpp reduced from ~600 to ~450 lines



## Current Status Summary

The project now uses a full ESP-IDF approach for all UART operations and modern AsyncWebServer:
- **Device 1 (Main UART)**: Full ESP-IDF with DMA and event task ✅
- **Device 2 (Secondary)**: ESP-IDF with DMA polling mode ✅
- **Device 3 (Mirror/Bridge/Log)**: ESP-IDF with DMA polling mode ✅
- **USB Device**: Arduino Serial (proven stable) ✅
- **USB Host**: ESP-IDF implementation ✅
- **UART Logger**: ESP-IDF with DMA polling mode ✅
- **Permanent Network Mode**: Fully implemented and configurable ✅
- **Web Server**: ESPAsyncWebServer for non-blocking operations ✅