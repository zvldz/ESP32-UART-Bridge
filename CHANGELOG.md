# CHANGELOG

## v2.9.5 (Critical Memory Safety & Logging Refactoring) - January 2025 ✅
- **Critical Memory Safety Fixes**: Eliminated heap corruption and segmentation faults
  - **Adaptive Buffer Protection** (`src/adaptive_buffer.h`): Fixed buffer underflow and bounds checking
    - **Underflow prevention**: Added validation for `totalConsumed > bufferIndex` before size_t arithmetic
    - **Bounds checking**: Comprehensive validation before all memmove operations
    - **Safe state recovery**: Automatic buffer reset on detection of invalid states
    - **Partial transmission safety**: Enhanced partial data transmission with proper boundary checks
  - **Protocol Pipeline Hardening** (`src/protocols/protocol_pipeline.h`): Fixed MAVLink detection crashes
    - **Pointer validation**: Added bounds checking before `detector->canDetect()` calls
    - **Search window protection**: Prevented buffer overruns during protocol packet search
    - **Packet size validation**: Limited packet sizes to reasonable bounds (max 512 bytes)
    - **Diagnostic logging**: Added periodic state dumps for debugging protocol issues
  - **MAVLink Detector Improvements** (`src/protocols/mavlink_detector.h/cpp`): Enhanced error recovery
    - **Increased search window**: Expanded from 64 to 300 bytes for better resync capability
    - **Improved resync logic**: More efficient recovery from garbage data streams
    - **Enhanced error handling**: Better progress through invalid data sequences
  - **Stability Impact**: Eliminated MAVFtp connection drops and system crashes
    - **Zero heap corruption**: Fixed root cause of ESP32 reboots during MAVLink processing
    - **Reliable protocol detection**: MAVLink packets now consistently detected and processed
    - **Stable network operations**: MAVFtp parameter uploads work without interruption

- **Logging System Refactoring**: Complete elimination of heap fragmentation
  - **Printf-Style Migration**: Migrated all ~306 log_msg calls across 21 files
    - Replaced String concatenation (`"Text " + String(var)`) with printf format (`"Text %d", var`)
    - Unified logging interface: `log_msg(LOG_LEVEL, "format", args)`
    - Added compile-time format string validation with `__attribute__((format(printf, 2, 3)))`
  - **Memory Optimization**: Eliminated heap allocation in logging hot paths
    - **Stack-based formatting**: All log messages use 256-byte stack buffers
    - **Zero heap fragmentation**: Removed String object creation in critical paths
    - **Hot path protection**: Adaptive buffer and protocol pipeline logging optimized
  - **Performance Impact**: Significant stability improvement for ESP32
    - **Reduced RAM usage**: Eliminated hundreds of String allocations per second
    - **Lower fragmentation**: ESP32 heap stays healthier during intensive operations
    - **Better reliability**: Reduced risk of memory-related crashes during MAVLink processing
  - **Code Cleanup**: Removed deprecated logging functions
    - Deleted old `log_msg(String, LogLevel)` and `log_msg(const char*, LogLevel)` functions
    - Single unified printf-style logging function for entire codebase
    - Improved code maintainability and consistency

## v2.9.0 (MAVLink Protocol Implementation) - January 2025 ✅
- **MAVLink Protocol Detector** - Phase 4.2 Complete
  - **Core MAVLink Implementation**: Full MAVLink v1/v2 packet detection
    - Created `src/protocols/mavlink_detector.h/cpp` with complete implementation
    - MAVLink packet boundary detection with header validation and payload size calculation
    - Support for MAVLink v1 (0xFE) and v2 (0xFD) protocol versions
    - Error handling with next start byte search for packet recovery
  - **Protocol Statistics System**: Comprehensive performance tracking
    - Created `src/protocols/protocol_stats.h` for real-time metrics
    - Tracks detected packets, transmitted packets, error count, resync events
    - Packet size statistics (average, min, max) and transmission rates
    - Integration with web interface for live protocol monitoring
  - **Configuration Management**: Seamless integration with existing config system
    - Added `protocolOptimization` field with version migration (v6→v7)
    - Protocol factory system with `src/protocols/protocol_factory.h/cpp`
    - Web interface dropdown for protocol selection (None/MAVLink)
    - Configuration persistence and loading across reboots
  - **Web Interface Enhancements**: User-friendly protocol management
    - Protocol Optimization dropdown in main interface (after UART Configuration)
    - Collapsible Protocol Statistics section with real-time updates
    - Statistics display: packets, errors, sizes, rates, last packet timing
    - Form integration with save/load functionality
  - **Performance Impact**: Dramatic latency reduction for MAVLink streams
    - **Eliminates UART FIFO overflows** at 115200 baud with high-speed MAVLink data
    - **Instant packet transmission** upon boundary detection (no timeout delays)
    - **Perfect packet preservation** - no data loss or fragmentation
    - Compatible with adaptive buffering fallback for non-MAVLink data
  - **HTML Minification Fix**: Improved web interface readability
    - Disabled aggressive HTML minification to preserve text spacing
    - Maintained gzip compression for optimal size (70-78% reduction)
    - Preserved JavaScript/CSS minification for performance

## v2.8.3 (Protocol Detection Framework) - July 2025 ✅
- **Protocol Detection Infrastructure** - Phase 4.1 Complete
  - **Framework Architecture**: Extensible protocol detection system
    - Created `src/protocols/` directory with base classes and interfaces
    - Implemented `ProtocolDetector` interface for protocol-specific implementations
    - Added comprehensive protocol pipeline with processing hooks
  - **Data Structure Extensions**: Enhanced core structures for protocol support
    - Added `ProtocolType` enum with support for MAVLink, Modbus RTU, NMEA, Text Line, SBUS
    - Extended `BridgeContext` with protocol detection state and error tracking
    - Added `protocolOptimization` configuration field with PROTOCOL_NONE default
  - **Integration Points**: Protocol-aware data processing
    - Integrated protocol hooks in `bridge_processing.h` for byte-level processing
    - Enhanced `adaptive_buffer.h` with protocol-specific transmission logic
    - Added protocol lifecycle management in `uartbridge.cpp`
  - **Stub Implementation**: Complete framework with zero functional impact
    - All protocol functions implemented as safe stubs returning default values
    - Zero performance overhead when disabled (PROTOCOL_NONE default)
    - Full backward compatibility with existing adaptive buffering
  - **Benefits**: Ready infrastructure for Phase 4.2+ protocol implementations
    - Easy addition of new protocols without architectural changes
    - Clear separation of concerns between framework and protocol logic
    - Prepared for future protocol-specific optimizations

## v2.8.2 (WiFi Manager ESP-IDF Migration) - July 2025 ✅
- **Complete WiFi Manager Migration** - Completed
  - **Full ESP-IDF Implementation**: Migrated from Arduino WiFi API to native ESP-IDF
    - Replaced WiFi.h with esp_wifi.h, esp_event.h, esp_netif.h
    - Event-driven architecture using ESP-IDF event handlers
    - Native WiFi configuration and state management
    - Eliminated temporary AP appearance in Client mode (main issue resolved)
  - **mDNS Service Implementation**: Device discovery by hostname in Client mode
    - Added mdns.h support with CONFIG_MDNS_ENABLED=y
    - Generated unique hostnames using device name + MAC suffix (2 bytes)
    - Safe mDNS initialization outside event handler context
    - HTTP service advertisement for web interface discovery
  - **DHCP Hostname Configuration**: Proper device identification in router
    - Implemented esp_netif_set_hostname() before WiFi start
    - Router now displays custom device name instead of "espresiff"
    - Hostname generation with MAC uniqueness for multiple devices
  - **Enhanced Error Handling**: Robust connection management and recovery
    - Bootloop protection with safe mode after 3 WiFi init failures
    - Proper memory checks before WiFi initialization
    - Connection timeout handling and automatic retry logic
    - Scan failure recovery with WiFi subsystem reset

## v2.8.1 (WiFi Client Mode Stability Fixes) - July 2025 ✅
- **WiFi Client Connection Logic** - Major stability improvements
  - **AP Mode Conflict Resolution**: Fixed dual AP+Client mode issue causing network conflicts
    - Added ESP-IDF level WiFi initialization with forced STA-only mode 
    - Implemented double AP disable: at client start and after successful connection
    - Eliminated unwanted AP broadcast during client connection process
  - **Intelligent Connection Management**: Enhanced retry logic with proper error distinction
    - **Initial Connection**: Scans every 15 seconds when network not found
    - **Connection Attempts**: Up to 5 attempts when target network is available  
    - **Authentication Failure**: Stops after 5 failed attempts to prevent router lockout
    - **Network Loss Recovery**: Unlimited reconnection attempts after successful initial connection
    - **Error State Handling**: Clear distinction between "wrong password" and "network not found"
  - **LED Indication Accuracy**: Corrected LED behavior for all connection states
    - Orange slow blink (2s): Searching for configured network
    - Orange solid: Successfully connected to WiFi network
    - Red fast blink (500ms): Authentication failed (wrong password) - restart required
    - Purple solid: AP mode active for direct configuration
  - **Performance Optimization**: Eliminated unnecessary scanning after successful connection
    - Fixed continuous scanning loop when already connected
    - Improved WiFi manager process state handling
- **Code Quality Improvements**
  - **ESP-IDF Integration**: Hybrid approach using ESP-IDF for initialization, Arduino for operations
- **Documentation Updates**
  - **README.md**: Comprehensive WiFi Client section with detailed connection logic
  - **Help Page**: Updated HTML help with key WiFi Client information and troubleshooting
  - **Troubleshooting**: Enhanced problem resolution guide with specific LED state meanings

## v2.8.0 (WiFi Client Mode Implementation) - July 2025 ✅
- **WiFi Client Mode** - Full implementation completed
  - **Dual WiFi Modes**: Support for both Access Point (AP) and Client (STA) modes
  - **WiFi Manager**: Complete state machine with scanning, connecting, and error handling
  - **Triple Click Logic**: Enhanced button logic for mode switching between Standalone/Client/AP
  - **LED Indication System**: 
    - Orange slow blink during network scanning (2s interval)
    - Solid orange when connected as WiFi client
    - Fast red blink for connection errors (500ms interval)
    - White blink for button click feedback in Client mode
  - **Device 4 Network Awareness**: Proper WiFi connection waiting using EventGroup synchronization
  - **Temporary/Permanent Modes**: Session-based temporary modes and persistent configuration
  - **Configuration**: Full web interface integration for WiFi Client credentials
  - **NVS Optimization**: Fixed preferences key length limitations (temp_net_mode)
  - **Benefits**:
    - Connect to existing WiFi networks while maintaining AP functionality
    - Seamless mode switching without configuration loss
    - Visual feedback for all WiFi states and user interactions
    - Reliable network operations with proper synchronization

## v2.7.3 (Configuration Import/Export + UI Improvements) - July 2025 ✅
- **Configuration Import/Export** - Completed
  - **Export Configuration**: Download current config as JSON file with unique filename
  - **Import Configuration**: Upload JSON config file with validation and auto-reboot  
  - **Web Interface**: Added "Configuration Backup" section with Export/Import buttons
  - **Async Reboot Fix**: Resolved reboot timing issues using TaskScheduler for delayed restart
  - **UI/UX Improvements**: Proper status messages, progress bars, and error handling
  - **Benefits**:
    - Quick device provisioning and configuration backup/restore
    - Share configurations between devices  
    - No need to reconfigure after firmware updates
    - Reliable status display without "Failed to fetch" errors

## v2.7.2 (Device 3/4 Code Refactoring) - July 2025 ✅
- **Device 3/4 Module Separation** - Completed
  - **File Structure**: Created independent device3_task.cpp/h and device4_task.cpp/h modules
  - **Code Reduction**: Reduced uartbridge.cpp from 600+ to 240 lines (360+ lines moved)
  - **Architecture Improvement**: Clear separation of device responsibilities for better maintainability
  - **Dependencies**: Updated all cross-file includes and extern declarations
  - **Linker Fixes**: Resolved device3Serial definition conflicts between modules
  - **Benefits**:
    - Easier maintenance and development
    - Reduced file complexity and improved readability
    - Clear module boundaries for future expansion
    - Preserved all existing functionality without changes

## v2.7.1 (Device 3/4 Statistics Unification) - July 2025 ✅
- **Statistics Thread Safety** - Completed
  - **Device 4 Protection**: Added critical sections for all TX/RX statistics updates
  - **Device 3 Migration**: Moved from local variables to global variables approach
  - **Race Condition Fixes**: Eliminated multi-threaded statistics conflicts
  - **Unified Architecture**: Consistent statistics handling across Device 3 and Device 4
  - **TaskScheduler Integration**: Implemented updateDevice3Stats() for periodic updates
  - **Benefits**:
    - Thread-safe statistics under high data load
    - Reliable counters without "jumping" values
    - Maintainable unified codebase
    - Stable operation in concurrent scenarios

## v2.7.0 (Device 4 Network Implementation) - July 2025 ✅
- **Device 4 Network Functionality** - Completed
  - **Network Logger Mode**: Send system logs via UDP (broadcast or unicast)
  - **Network Bridge Mode**: Bidirectional UART<->UDP communication
  - **Configuration**: Full web interface integration with IP/Port settings
  - **Statistics**: Real-time TX/RX bytes and packet counters
  - **Architecture**:
    - AsyncUDP library for network operations
    - Ring buffers for non-blocking data transfer
    - Task runs on Core 1 with web server
    - DMA-style approach for efficient data handling
  - **Integration**:
    - Device 4 configuration saved/loaded with config version 5
    - Network Log Level enabled when Device 4 is active
    - Full statistics in web interface status display
    - Works in both standalone and network modes
  - **Technical Details**:
    - 2KB buffers for logs and bridge data
    - 50ms task delay for low latency
    - Mutex protection for thread-safe buffer access
    - Forward declarations to resolve compilation order
  - **Fixed Issues**:
    - Web interface now correctly displays selected Device 4 role
    - Removed duplicate role name transmission in config JSON

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