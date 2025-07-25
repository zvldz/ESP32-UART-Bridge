# TODO / Roadmap

## Completed ✅

### v2.3.0
- [x] **Remove DEBUG_MODE** - Completed
  - Removed all DEBUG_MODE checks from code
  - Bridge always active in all modes
  - Diagnostics converted to log levels
  
- [x] **Remove CONFIG_FREERTOS_UNICORE** - Completed
  - Now only supports multi-core ESP32
  - UART task on core 0, Web task on core 1
  
- [x] **Code Organization** - Completed
  - Extracted diagnostics to separate module (diagnostics.cpp/h)
  - Moved system utilities to system_utils.cpp/h
  - Moved RTC variables and crash update to crashlog.cpp/h
  - main.cpp reduced from ~600 to ~450 lines

### v2.3.1
- [x] **Update Statistics System** - Completed
  - Replaced old `bytesUartToUsb`/`bytesUsbToUart` with per-device counters
  - Added `device1RxBytes`/`device1TxBytes`, `device2RxBytes`/`device2TxBytes`, `device3RxBytes`/`device3TxBytes`
  - Updated web interface to show statistics for all active devices
  - Shows device roles in statistics display

- [x] **Code Cleanup** - Completed
  - Moved `updateSharedStats` and `resetStatistics` to diagnostics module
  - Removed legacy `if (currentMode == MODE_NORMAL || currentMode == MODE_CONFIG)` check
  - Fixed deprecated ArduinoJson `containsKey()` warning
  - Added helper functions `getDevice2RoleName()` and `getDevice3RoleName()`

- [x] **Web Interface Updates** - Completed
  - Updated traffic statistics table to show per-device data
  - Dynamic show/hide of device rows based on active roles
  - Updated JavaScript to handle new JSON structure from `/status` endpoint
  - Fixed "Never" display for last activity

### v2.3.2 (Web Interface Migration)
- [x] **Web Interface Refactoring** - Completed
  - Migrated from embedded HTML strings to separate HTML/CSS/JS files
  - Created build-time processing with `embed_html.py` script
  - Reduced placeholders from 40+ to just {{CONFIG_JSON}}
  - Separated JavaScript into main.js and crash-log.js
  - HTML/CSS/JS files now editable with proper IDE support
  - C++ code reduced by ~3000 lines

### v2.3.3 (Performance Optimization)
- [x] **UART Bridge Performance Fix** - Completed
  - Fixed packet drops during MAVLink parameter downloads
  - Optimized device role checks with cached flags
  - Removed repeated config.deviceX.role comparisons in critical loop
  - Improved performance at 115200 baud and higher

- [x] **Buffer Size Optimization** - Completed
  - Increased UART_BUFFER_SIZE from 256 to 1024 bytes
  - Added Serial.setTxBufferSize(1024) for USB interface
  - Removed baudrate condition - always use 1KB buffers
  - Reduced WiFi yield time from 5ms to 3ms

### v2.4.0 (ESP-IDF Migration)
- [x] **Remove Arduino Framework Dependencies for Device1** - Completed
  - Migrated Device 1 UART to ESP-IDF driver with DMA
  - Removed all conditional compilation (#ifdef USE_UART_DMA)
  - Deleted uart_arduino.cpp wrapper
  - Improved UART performance with hardware packet detection

- [x] **Configuration System Update** - Completed
  - Updated Config struct to use ESP-IDF native types (uart_word_length_t, uart_parity_t, uart_stop_bits_t)
  - Increased config version to 3 with automatic migration
  - Added conversion functions between ESP-IDF enums and web interface strings

- [x] **Code Cleanup and Optimization** - Completed
  - Created UsbBase class to eliminate code duplication between USB Host and Device
  - Simplified UartInterface with direct ESP-IDF configuration
  - Removed serial_config_decoder.h dependency for Device 1
  - Improved error handling and logging

- [x] **Performance Improvements** - Completed
  - Hardware DMA for Device 1 UART with packet boundary detection
  - Zero-copy ring buffer implementation
  - Reduced CPU usage through event-driven architecture
  - Native ESP-IDF drivers for better performance
  - Increased DMA buffers and task priority for high-throughput scenarios

- [x] **USB Implementation Decision** - Completed
  - Kept Arduino Serial for USB Device (stable and sufficient)
  - USB Host already uses ESP-IDF
  - Created common base class for code reuse
  - USB performance not critical compared to UART

### v2.5.0 (Complete ESP-IDF Migration + Performance)
- [x] **Complete ESP-IDF Migration** - Completed
  - Migrated Device 2 UART to ESP-IDF/DMA ✅
  - Migrated Device 3 UART to ESP-IDF/DMA ✅
  - Migrated UART logger to ESP-IDF/DMA ✅
  - Removed HardwareSerial dependencies completely ✅
  - Removed serial_config_decoder.h ✅

- [x] **DMA Polling Mode Implementation** - Completed
  - Added polling mode support to UartDMA class ✅
  - Device 2/3 use polling mode (no event tasks) ✅
  - Added `pollEvents()` method for non-blocking operation ✅
  - Optimized for minimal resource usage ✅

- [x] **Configuration Architecture Improvement** - Completed
  - Created `UartConfig` structure for UART parameters ✅
  - Separated DMA configuration from UART configuration ✅
  - Fixed scope conflicts with global config ✅
  - Clean parameter passing without global dependencies ✅

- [x] **Dynamic Adaptive Buffer Sizing** - Completed
  - Buffer size now adjusts based on baud rate (256-2048 bytes) ✅
  - 256 bytes for ≤19200 baud
  - 512 bytes for ≤115200 baud
  - 1024 bytes for ≤460800 baud
  - 2048 bytes for >460800 baud
  - Prevents packet fragmentation at high speeds
  - Maintains low latency at all speeds

### v2.5.1 (Web Interface Modularization)
- [x] **Split main.js into modules** - Completed (July 2025)
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

- [x] **Fix Reset Statistics button** - Completed
  - Removed confirmation dialog as requested
  - Fixed button state stuck on "Resetting..."
  - Button now properly returns to "Reset Statistics" after operation

- [x] **OTA Update Safety** - Completed
  - Added Device 3 UART0 deinitialization before OTA update
  - Prevents conflicts during firmware update when Device 3 uses UART0
  - Made `device3Serial` accessible from `web_ota.cpp`
  - Ensures clean OTA update even with active Device 3

- [x] **Code Cleanup** - Completed
  - Removed unused `lastWdtFeed` variable from `uartbridge.cpp`
  - Fixed legacy code remnants

### v2.5.2 (Phase 1 Code Refactoring)
- [x] **Refactor Large uartbridge.cpp File** - Phase 1 Complete
  - Original size: ~700 lines, reduced to ~555 lines
  - Created modular structure without breaking functionality
  
- [x] **Flow Control Module** - Completed
  - Created `flow_control.h` and `flow_control.cpp`
  - Extracted `detectFlowControl()` and `getFlowControlStatus()` functions
  - ~50 lines of code moved to separate module
  - Clean separation of flow control logic

- [x] **Device Initialization Module** - Completed
  - Created `device_init.h` and `device_init.cpp`
  - Extracted `initDevice2UART()` and `initDevice3()` functions
  - ~85 lines of code moved to separate module
  - Fixed linkage issues with proper extern declarations

- [x] **Build System Verification** - Completed
  - Fixed static/extern conflicts for device2Serial
  - Added proper extern declarations in uartbridge.h
  - Project compiles successfully with new structure
  - All functionality preserved

### v2.5.3 (Phase 2 Code Refactoring - Hybrid Approach)
- [x] **Phase 2 Refactoring - Hybrid Approach** - Completed
  - Further refactored uartbridge.cpp from ~555 to ~250 lines
  - Created BridgeContext structure for clean parameter passing
  - Maintained performance with static inline functions
  
- [x] **Extended types.h** - Completed
  - Added comprehensive BridgeContext structure
  - Groups all task-local variables logically
  - Added inline initBridgeContext() function
  - Forward declarations for UartInterface/UsbInterface
  
- [x] **Enhanced Diagnostics Module** - Completed
  - Moved periodic diagnostics from uartbridge.cpp
  - Added logBridgeActivity(), logStackDiagnostics()
  - Added logDmaStatistics(), logDroppedDataStats()
  - Created updatePeriodicDiagnostics() main function
  - ~150 lines moved to diagnostics module
  
- [x] **Created bridge_processing.h** - Completed
  - All data processing logic as inline functions
  - processDevice1Input(), processDevice2USB/UART()
  - processDevice3BridgeInput()
  - shouldYieldToWiFi() helper
  - ~200 lines of processing logic extracted
  
- [x] **Created adaptive_buffer.h** - Completed
  - All adaptive buffering logic in one place
  - shouldTransmitBuffer(), transmitAdaptiveBuffer()
  - handleBufferTimeout()
  - calculateAdaptiveBufferSize() moved from device_init
  - ~120 lines of buffering logic extracted
  
- [x] **Simplified Main Loop** - Completed
  - Clean, readable main loop with function calls
  - All complex logic in specialized modules
  - Performance preserved with inline functions
  - Ready for future extensions

### v2.5.4 (TaskScheduler Implementation) - Completed ✅ (July 2025)
- [x] **Implement TaskScheduler** - Completed
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

### v2.5.5 (Adaptive Buffer Optimization) - Completed ✅ (July 2025)
- [x] **Fix FIFO Overflow at 115200 baud** - Completed
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

- [x] **Code Cleanup** - Completed
  - Fixed duplicated adaptive buffering code in `bridge_processing.h`
  - Now properly uses functions from `adaptive_buffer.h`
  - Removed ~55 lines of duplicated code
  - Added temporary diagnostics (marked for removal after testing)

### v2.5.6 (Bridge Mode Renaming) - Completed ✅ (July 2025)
- [x] **Rename Device Modes to Bridge Modes** - Completed
  - Renamed `DeviceMode` enum to `BridgeMode`
  - Renamed `MODE_NORMAL` to `BRIDGE_STANDALONE`
  - Renamed `MODE_CONFIG` to `BRIDGE_NET`
  - Updated all references across 11 source files
  - Benefits:
    - Clear separation of bridge functionality vs network state
    - Future-ready for Device 4 permanent network modes
    - Better naming that accurately describes functionality
    - Flexible design supporting different network configurations

- [x] **Update SystemState Structure** - Completed
  - Renamed `wifiAPActive` to `networkActive`
  - Renamed `wifiStartTime` to `networkStartTime`
  - Added `isTemporaryNetwork` flag for future Device 4 support
  - Allows differentiation between setup AP (temporary) and permanent network modes

- [x] **Update All User-Visible Text** - Completed
  - Changed "normal mode" to "standalone mode" throughout
  - Changed "config mode" to "network mode" throughout
  - Changed "WiFi configuration" to "network setup" where appropriate
  - Simplified to just "Network Mode" (without "setup") for universal usage
  - Updated help documentation and web interface

- [x] **Implementation Details** - Completed
  - Total changes: ~60 occurrences across 11 files
  - Added critical comment for `_TASK_INLINE` macro in scheduler_tasks.h
  - All mode checks updated to use new enum values
  - Function names updated (initNormalMode → initStandaloneMode, etc.)
  - TaskScheduler functions renamed (enableRuntimeTasks → enableStandaloneTasks)
  - Network timeout only active when `isTemporaryNetwork=true`

### v2.5.7 (Device Init Refactoring) - Completed ✅ (July 2025)
- [x] **Refactor Device Initialization** - Completed
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

### v2.5.8 (Permanent Network Mode) - Completed ✅ (July 2025)
- [x] **Permanent Network Mode Implementation** - Completed
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

- [x] **Build System Enhancement** - Completed
  - Added `update_readme_version.py` script for automatic version synchronization
  - Script automatically updates README.md badge with version from defines.h
  - Integrated into PlatformIO build process as pre-build script
  - Prevents version mismatches between firmware and documentation
  - **Fixed**: Removed blocking `exit(0)` that prevented compilation and upload

### v2.6.0 (ESPAsyncWebServer Migration) - Completed ✅ (July 2025)
- [x] **Migrate to ESPAsyncWebServer** - Completed
  - **Libraries**: Updated to ESPAsyncWebServer v3.7.10 + AsyncTCP v3.4.5
  - **Template System**: Changed from custom `{{}}` to built-in `%PLACEHOLDER%` processor
  - **API Migration**: Converted all handlers to async (request parameter access, response sending)
  - **OTA Adaptation**: Redesigned file upload handling for async server
  - **Architecture**: Removed webServerTask - AsyncWebServer works without dedicated task
  - **Memory Benefits**: Better resource usage, no blocking operations
  - **Performance**: Non-blocking request handling, improved concurrent connections
  - **JavaScript Fixes**: Fixed Reset Statistics and Clear Crash History button handling
  - **Diagnostics**: Enhanced stack monitoring for WiFi/TCP tasks instead of web server task

### Priority 1 — Logging System and Device Roles ✅ COMPLETED

#### Logging Channels

There are three distinct logging channels, each with its own configurable verbosity level:

- **Web (GUI) Logs** — displayed in the browser, optionally with filters.
- **UART Logs** — output through UART (Device 3, TX pin 11).
- **Network Logs** — transmitted over UDP when Wi-Fi is active (Device 4).

Each channel can be enabled or disabled independently and can have its own log level (e.g. ERROR, WARNING, DEBUG).

#### Device Roles

Each device can operate in one of several roles, or be disabled (`None`). Roles and pin assignments must be clearly configured through the web interface using a dropdown selector.

##### Device 1 — Main UART Interface (Required)
- **Always enabled**, uses fixed UART TX/RX on GPIO 4/5.
- Protocol is generic UART (e.g. MAVLink, NMEA), not limited to MAVLink.
- This device **does not participate in logging**.
- **Now uses ESP-IDF DMA implementation exclusively** ✅

##### Device 2 — Secondary Communication Channel
- Can be:
  - Disabled
  - UART over GPIO 8/9 (**now uses UartDMA with polling**) ✅
  - USB Device mode (uses Arduino Serial)
  - USB Host mode (uses ESP-IDF USB Host API)
- Can be a participant in a bidirectional UART bridge with Device 1.
- Not used for logging.

##### Device 3 — Logging or Mirror UART
- Can be:
  - Disabled
  - Mirror of Device 1 (unidirectional 1 → 3)
  - Full UART bridge with Device 1 (bidirectional)
  - Dedicated UART log channel (TX only) at 115200 baud
- Uses fixed UART pins 11/12.
- **Now uses UartDMA with polling (including logger mode)** ✅

##### Device 4 — Wi-Fi / Network Channel
- Can be:
  - Disabled
  - Network bridge (e.g. MAVLink-over-UDP)
  - Logging over Wi-Fi (UDP logs)
- Wi-Fi must be active for this device to be used.
- **Will use AsyncUDP (built into ESP32 Arduino Core) for implementation**

**Note**: Only one role can be active per device. Conflicting configurations (e.g. enabling both mirror and log on Device 3) must be blocked in the UI.

#### Logging Policies (Default Suggestions)
- **UART logs (Device 3)** — log only errors by default
- **Network logs (Device 4)** — log only errors or be disabled
- **Web logs (GUI)** — full DEBUG by default (only shown in UI)
- **Do not mirror full DEBUG logs to Wi-Fi** — it may affect performance

## Current Status

The project now uses a full ESP-IDF approach for all UART operations and modern AsyncWebServer:
- **Device 1 (Main UART)**: Full ESP-IDF with DMA and event task ✅
- **Device 2 (Secondary)**: ESP-IDF with DMA polling mode ✅
- **Device 3 (Mirror/Bridge/Log)**: ESP-IDF with DMA polling mode ✅
- **USB Device**: Arduino Serial (proven stable) ✅
- **USB Host**: ESP-IDF implementation ✅
- **UART Logger**: ESP-IDF with DMA polling mode ✅
- **Permanent Network Mode**: Fully implemented and configurable ✅
- **Web Server**: ESPAsyncWebServer for non-blocking operations ✅

## Priority 2 - Device 4 Network Implementation

- [ ] **Device 4 Network Implementation** (implement as single unit)
  - UDP Bridge Mode for MAVLink-over-WiFi (client + server)
  - Network logging capabilities via UDP
  - Will use built-in AsyncUDP (no external library needed)
  - Configure via web interface (target IP, port, role selection)
  - Integration with existing device role system
  - **Implementation approach**:
    - Create device4_network.cpp/h
    - Add to hybrid refactoring structure
    - Support both client and server modes
    - Add Device4Task for network packet handling
  - **Future preparation**:
    - Create base `NetworkChannel` class for future TCP support
    - Design config structure with room for TCP parameters
    - Abstract `LogTransport` interface for different transports

### Alternative Captive Portal Implementation
- **Current**: DNSServer + tDnsProcess task (150ms polling)  
- **Alternative**: AsyncWebServer CaptiveRequestHandler class
- **Benefits**: No separate DNS task, better async integration
- **Implementation**:
  ```cpp
  class BridgeCaptiveHandler : public AsyncWebHandler {
      bool canHandle(AsyncWebServerRequest *request) const override { return true; }
      void handleRequest(AsyncWebServerRequest *request) { request->redirect("/"); }
  };
  server.addHandler(new BridgeCaptiveHandler()).setFilter(ON_AP_FILTER);
  ```
- **Device-specific endpoints**: /generate_204 (Android), /hotspot-detect.html (iOS)
- **Priority**: After main ESPAsyncWebServer migration is stable

## Priority 3 - WiFi Client Mode

### 3.1 - Basic WiFi Client
- [ ] **Basic WiFi Client Implementation**
  - Connect ESP32 to existing WiFi network instead of creating AP
  - Store WiFi credentials in LittleFS config file
  - Web interface for WiFi network selection and password entry
  - Basic connection status indication
  - **Future preparation**: WiFi mode enum (AP, Client, AP+Client)

### 3.2 - Auto-connect and Fallback
- [ ] **Auto-connect and Fallback Logic**
  - Auto-connect on boot with saved credentials
  - Automatic fallback to AP mode on connection failure
  - Configurable retry attempts and timeouts
  - Advanced status indication (connected/disconnected/searching)
  - **Future preparation**: State machine for network mode transitions

## Priority 4 - Multi-Board Support

- [ ] **ESP32-S3 Super Mini Support**
  - Add board detection system with compile-time configuration
  - Create separate PlatformIO environments for different boards:
    - `waveshare-s3-zero` (full features including USB Host)
    - `super-mini` (all features except USB Host, RGB LED on GPIO48)
  - Implement runtime USB Host capability checking
  - Update web interface to hide USB Host option on unsupported boards
  - Benefits:
    - Broader hardware compatibility
    - User choice between full features vs ultra-compact size
    - Same codebase supports multiple popular ESP32-S3 boards
  - **Board Comparison**:
    - **Waveshare ESP32-S3-Zero**: 25x24mm, USB Host support, WS2812 LED (GPIO21)
    - **ESP32-S3 Super Mini**: 22x18mm, 8MB PSRAM, RGB LED (GPIO48), no USB Host
  - Implementation approach:
    - Compile-time board detection via PlatformIO build flags
    - Runtime validation of USB Host mode availability
    - Conditional web interface options based on board capabilities
    - Unified documentation with board-specific notes

## Priority 5 - Protocol Optimizations

### 5.1 - Protocol Detection Framework
- [ ] **Protocol Detection Framework**
  - Create base `ProtocolDetector` class
  - Define interface for packet boundary detection
  - Support for registering multiple detectors
  - Integration points in main data flow
  - **Future preparation**: Architecture ready for SBUS and other protocols

### 5.2 - MAVLink Parser with Hardware Optimization
- [ ] **MAVLink Packet Detection**
  - Implement `MavlinkDetector` using Protocol Framework
  - Simple header check to detect packet length (~30 lines)
  - No CRC validation or message decoding - just boundaries
  - **Hardware Packet Detection Integration**:
    - Dynamic UART idle timeout based on MAVLink timing
    - Pattern detection for MAVLink sync bytes
    - Optimize `rx_timeout_thresh` for MAVLink packets
  - Can be enabled/disabled via config option
  - Reduces latency by eliminating timeout waiting

### 5.3 - Device 3 Adaptive Buffering
- [ ] **Device 3 Adaptive Buffering**
  - Currently uses simple batch transfer (64-byte blocks)
  - Implement adaptive buffering using Protocol Framework
  - Apply protocol detectors for intelligent buffering
  - Add packet boundary preservation for detected protocols
  - Improve Mirror/Bridge mode performance for packet-based protocols
  - Benefits for various use cases:
    - **Industrial**: Modbus RTU frame timing preservation
    - **Marine**: Complete NMEA sentences
    - **IoT**: Protocol-aware routing
    - **RS-485**: Intelligent gateway operation

## Priority 6 - SBUS Protocol Support

- [ ] **SBUS Mode** - UART to/from SBUS converter
  - Convert standard UART to SBUS protocol (100000 baud, 8E2, inverted)
  - Convert SBUS to standard UART (configurable baud rate)
  - **Uses Protocol Framework from Priority 5.1**
  - **Device Roles**:
    - `D1_UART_TO_SBUS`: Device 1 receives UART, Device 2 outputs SBUS
    - `D1_SBUS_TO_UART`: Device 1 receives SBUS, Device 2 outputs UART
  - **Use cases**:
    - Connect non-SBUS flight controllers to SBUS-only receivers
    - Use SBUS receivers with devices expecting standard UART
    - Protocol conversion for legacy equipment
  - **Advanced Network Integration**:
    - **Remote SBUS over Network Infrastructure**: 
      - Transmit: `SBUS receiver → ESP32 (SBUS→UART) → Network device → Internet`
      - Receive: `Internet → Network device → ESP32 (UART→SBUS) → Flight controller`
      - ESP32 acts as protocol converter, network transmission via external devices
      - Enables SBUS control over unlimited distances using existing network infrastructure
  - **Technical Implementation**:
    - Implement `SbusProtocol` class using Protocol Framework
    - Hardware inverter required for true SBUS signal levels
    - SBUS frame parsing/generation (25 bytes, specific format)
    - Configurable channel mapping and signal processing
    - Timing-critical operations on SBUS side, relaxed timing on UART side
  - **Note**: SBUS cannot be transmitted directly over network due to inverted signal and strict timing requirements

## Priority 7 - TCP Client Mode

- [ ] **TCP Client Mode**
  - Connect to remote TCP servers
  - Automatic reconnection on disconnect
  - Configurable server address and port via web interface
  - Connection status monitoring
  - **Implementation**: Use AsyncTCP library
  - Benefits: Reliable connection to remote servers
  - Use cases: Cloud logging, remote monitoring systems

## Out of Priority (Do when in the mood)

- [ ] **Fix Memory Leaks** *(Low priority - objects live for entire runtime)*
  - Add cleanup/shutdown functions for Device 2 and Device 3
  - Add cleanup for UART logger in logging.cpp
  - Consider using smart pointers (unique_ptr) for serial objects
  - Note: Not critical as these objects are never destroyed during normal operation

- [ ] **USB Backpressure Implementation**
  - Prevent UART reading when USB buffer is full
  - Reduce data loss at high speeds without flow control
  - Add dynamic threshold based on baud rate
  - Show warnings for high-speed configurations without flow control
  - Consider: `if (usbInterface->availableForWrite() < bufferIndex) skip_uart_read();`

## Future Considerations

- [ ] **Advanced Network Features**
  - **TCP Server Mode** - Accept TCP connections
    - Multiple client support with connection management
    - Authentication options for secure access
    - Connection timeout and cleanup handling
    
  - **WebSocket Real-time Interface** *(AsyncWebServer enables this)*
    - Real-time log streaming via WebSocket
    - Live statistics updates without polling
    - Bidirectional communication for control commands
    
  - **Network Discovery**
    - mDNS/Bonjour support for easy device discovery
    - SSDP (Simple Service Discovery Protocol)
    - Custom UDP broadcast announcement

- [ ] **Advanced Configuration**
  - Configurable GPIO pins via web interface
  - Support for different ESP32 board variants
  - Import/Export configuration files
  - Configuration profiles for common use cases

- [ ] **Hardware Packet Detection Improvements**
  - Current: Fixed 10 character timeout for idle detection
  - Improvements:
    - Dynamic timeout based on protocol hints
    - Pattern detection for text protocols
    - Adaptive timeout adjustment
    - FIFO threshold optimization

- [ ] **High-Speed Testing**
  - Test operation at 921600 and 1000000 baud
  - Profile CPU usage and latency
  - Verify packet integrity under load
  - Document any limitations

## Libraries and Dependencies

### Current Dependencies
```ini
lib_deps =
    bblanchon/ArduinoJson@^7.4.2      # JSON parsing and generation
    fastled/FastLED@^3.10.1            # WS2812 LED control
    arkhipenko/TaskScheduler@^3.7.0   # Task scheduling ✅
    ESP32Async/ESPAsyncWebServer@^3.7.10  # Async web server ✅
    ESP32Async/AsyncTCP@^3.4.5        # TCP support for async server ✅
```

### Planned Dependencies
```ini
lib_deps =
    # Built-in libraries (no installation needed)
    # - AsyncUDP (included in ESP32 Arduino Core)
    # - mDNS (included in ESP32 Arduino Core)
```

## Notes

- Current adaptive buffering achieves 95%+ efficiency for most protocols
- USB Auto mode needs VBUS detection implementation
- Consider security implications before adding network streaming modes
- Maintain backward compatibility with existing installations
- Version 2.6.0 introduces ESPAsyncWebServer for better performance and concurrent handling
- DMA implementation enables hardware-based packet detection and minimal packet loss
- Web interface modularization improves maintainability and development workflow
- TaskScheduler implementation significantly simplifies periodic task management
- Library selection focuses on well-maintained, performance-oriented solutions
- Bridge mode renaming provides clearer architecture for future expansion
- Permanent network mode enables always-on Wi-Fi operation for production deployments
- AsyncWebServer migration improves memory usage and enables advanced features like WebSockets