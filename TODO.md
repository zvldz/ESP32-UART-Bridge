# TODO / Roadmap

## Priority 1 — Logging System and Device Roles

### Logging Channels

There are three distinct logging channels, each with its own configurable verbosity level:

- **Web (GUI) Logs** — displayed in the browser, optionally with filters.
- **UART Logs** — output through UART (Device 3, TX pin 11).
- **Network Logs** — transmitted over UDP when Wi-Fi is active (Device 4).

Each channel can be enabled or disabled independently and can have its own log level (e.g. ERROR, WARNING, DEBUG).

### Device Roles

Each device can operate in one of several roles, or be disabled (`None`). Roles and pin assignments must be clearly configured through the web interface using a dropdown selector.

#### Device 1 — Main UART Interface (Required)
- **Always enabled**, uses fixed UART TX/RX on GPIO 4/5.
- Protocol is generic UART (e.g. MAVLink, NMEA), not limited to MAVLink.
- This device **does not participate in logging**.
- **Now uses ESP-IDF DMA implementation exclusively** ✅

#### Device 2 — Secondary Communication Channel
- Can be:
  - Disabled
  - UART over GPIO 8/9 (**now uses UartDMA with polling**) ✅
  - USB Device mode (uses Arduino Serial)
  - USB Host mode (uses ESP-IDF USB Host API)
- Can be a participant in a bidirectional UART bridge with Device 1.
- Not used for logging.

#### Device 3 — Logging or Mirror UART
- Can be:
  - Disabled
  - Mirror of Device 1 (unidirectional 1 → 3)
  - Full UART bridge with Device 1 (bidirectional)
  - Dedicated UART log channel (TX only) at 115200 baud
- Uses fixed UART pins 11/12.
- **Now uses UartDMA with polling (including logger mode)** ✅

#### Device 4 — Wi-Fi / Network Channel
- Can be:
  - Disabled
  - Network bridge (e.g. MAVLink-over-UDP)
  - Logging over Wi-Fi (UDP logs)
- Wi-Fi must be active for this device to be used.
- **Will use AsyncUDP (built into ESP32 Arduino Core) for implementation**

**Note**: Only one role can be active per device. Conflicting configurations (e.g. enabling both mirror and log on Device 3) must be blocked in the UI.

### Logging Policies (Default Suggestions)
- **UART logs (Device 3)** — log only errors by default
- **Network logs (Device 4)** — log only errors or be disabled
- **Web logs (GUI)** — full DEBUG by default (only shown in UI)
- **Do not mirror full DEBUG logs to Wi-Fi** — it may affect performance

### Device 3 Adaptive Buffering Optimization
- **Current state**: Device 3 uses simple byte-by-byte transfer without adaptive buffering
- **TODO**: Implement adaptive buffering for Device 3 Mirror/Bridge modes similar to Device 2
- **Benefits**: Better packet boundary preservation for protocols like MAVLink
- **Implementation**: Add buffering logic in device3Task() with configurable thresholds

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

### v2.4.0 (ESP-IDF Migration) - COMPLETED ✅
- [x] **Remove Arduino Framework Dependencies for Device1**
  - Migrated Device 1 UART to ESP-IDF driver with DMA
  - Removed all conditional compilation (#ifdef USE_UART_DMA)
  - Deleted uart_arduino.cpp wrapper
  - Improved UART performance with hardware packet detection

- [x] **Configuration System Update**
  - Updated Config struct to use ESP-IDF native types (uart_word_length_t, uart_parity_t, uart_stop_bits_t)
  - Increased config version to 3 with automatic migration
  - Added conversion functions between ESP-IDF enums and web interface strings

- [x] **Code Cleanup and Optimization**
  - Created UsbBase class to eliminate code duplication between USB Host and Device
  - Simplified UartInterface with direct ESP-IDF configuration
  - Removed serial_config_decoder.h dependency for Device 1
  - Improved error handling and logging

- [x] **Performance Improvements**
  - Hardware DMA for Device 1 UART with packet boundary detection
  - Zero-copy ring buffer implementation
  - Reduced CPU usage through event-driven architecture
  - Native ESP-IDF drivers for better performance
  - Increased DMA buffers and task priority for high-throughput scenarios

- [x] **USB Implementation Decision**
  - Kept Arduino Serial for USB Device (stable and sufficient)
  - USB Host already uses ESP-IDF
  - Created common base class for code reuse
  - USB performance not critical compared to UART

### v2.5.0 (Complete ESP-IDF Migration + Performance) - COMPLETED ✅
- [x] **Complete ESP-IDF Migration**
  - Migrated Device 2 UART to ESP-IDF/DMA ✅
  - Migrated Device 3 UART to ESP-IDF/DMA ✅
  - Migrated UART logger to ESP-IDF/DMA ✅
  - Removed HardwareSerial dependencies completely ✅
  - Removed serial_config_decoder.h ✅

- [x] **DMA Polling Mode Implementation**
  - Added polling mode support to UartDMA class ✅
  - Device 2/3 use polling mode (no event tasks) ✅
  - Added `pollEvents()` method for non-blocking operation ✅
  - Optimized for minimal resource usage ✅

- [x] **Configuration Architecture Improvement**
  - Created `UartConfig` structure for UART parameters ✅
  - Separated DMA configuration from UART configuration ✅
  - Fixed scope conflicts with global config ✅
  - Clean parameter passing without global dependencies ✅

- [x] **Dynamic Adaptive Buffer Sizing**
  - Buffer size now adjusts based on baud rate (256-2048 bytes) ✅
  - 256 bytes for ≤19200 baud
  - 512 bytes for ≤115200 baud
  - 1024 bytes for ≤460800 baud
  - 2048 bytes for >460800 baud
  - Prevents packet fragmentation at high speeds
  - Maintains low latency at all speeds

### v2.5.1 (Web Interface Modularization) - COMPLETED ✅
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

### v2.5.2 (Phase 1 Code Refactoring) - COMPLETED ✅ (July 2025)
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

### v2.5.3 (Phase 2 Code Refactoring - Hybrid Approach) - COMPLETED ✅ (July 2025)
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

## Current Status

The project now uses a full ESP-IDF approach for all UART operations:
- **Device 1 (Main UART)**: Full ESP-IDF with DMA and event task ✅
- **Device 2 (Secondary)**: ESP-IDF with DMA polling mode ✅
- **Device 3 (Mirror/Bridge/Log)**: ESP-IDF with DMA polling mode ✅
- **USB Device**: Arduino Serial (proven stable) ✅
- **USB Host**: ESP-IDF implementation ✅
- **UART Logger**: ESP-IDF with DMA polling mode ✅

Version 2.5.3 includes:
- Complete ESP-IDF migration for all UART devices
- Hardware DMA with packet boundary detection
- Dynamic adaptive buffer sizing based on baud rate
- Zero packet loss under normal conditions
- Minimal CPU usage and excellent performance up to 1Mbps
- Modular web interface with separated JavaScript files
- Safe OTA updates with Device 3 handling
- **Phase 1 refactoring complete**: uartbridge.cpp reduced by 145 lines
- **Phase 2 refactoring complete**: uartbridge.cpp reduced from ~555 to ~250 lines
- Clean modular architecture with specialized headers

## Priority 2 - Next Phase

- [ ] **Implement TaskScheduler** *(Next step)*
  - Replace all manual timer checks with TaskScheduler
  - Will simplify periodic tasks (diagnostics, statistics, LED updates)
  - Library: `arkhipenko/TaskScheduler@^3.7.0`
  - Expected to save ~100-150 lines of timer management code
  - Benefits:
    - Cleaner code structure
    - Easy to add/remove periodic tasks
    - Better timing accuracy
    - Reduced cognitive load

- [ ] **Migrate to ESPAsyncWebServer** *(With or after Device 4 implementation)*
  - Current: Synchronous WebServer
  - Target: ESPAsyncWebServer for better performance
  - Libraries needed:
    - `https://github.com/ESP32Async/ESPAsyncWebServer.git`
    - `https://github.com/ESP32Async/AsyncTCP.git`
  - Benefits:
    - Non-blocking web requests
    - WebSocket support for real-time logs
    - Server-Sent Events for live statistics
    - Better suited for permanent WiFi mode
  - Expected to save ~200 lines in web_interface.cpp

- [ ] **Device 4 Network Implementation**
  - UDP Bridge Mode for MAVLink-over-WiFi
  - Network logging capabilities
  - Will use built-in AsyncUDP (no external library needed)
  - Implementation approach:
    - Create device4_network.cpp/h
    - Add to hybrid refactoring structure
    - Support both client and server modes
    - Configure via web interface (target IP, port)

- [ ] **Fix Memory Leaks** *(Low priority - objects live for entire runtime)*
  - Add cleanup/shutdown functions for Device 2 and Device 3
  - Add cleanup for UART logger in logging.cpp
  - Consider using smart pointers (unique_ptr) for serial objects
  - Note: Not critical as these objects are never destroyed during normal operation

- [ ] **Device 3 Adaptive Buffering**
  - Currently uses simple batch transfer (64-byte blocks)
  - Implement adaptive buffering similar to main UART bridge task
  - Add packet boundary detection for better protocol handling
  - Would improve Mirror/Bridge mode performance for packet-based protocols
  
  **Analysis (July 2025)**: While initially considered low priority for MAVLink/drone use cases,
  this feature becomes important when viewing ESP32 UART Bridge as a universal serial device router:
  - **Industrial automation**: Modbus RTU requires precise inter-frame timing
  - **Marine electronics**: NMEA 0183 needs complete sentence preservation  
  - **IoT/Smart Home**: Bidirectional routing between coordinators and local controllers
  - **RS-485 networks**: ESP32 as intelligent gateway between network segments
  
  **Recommendation**: Implement in future version, starting with simple packet boundary flags.
  Current batch mode (64-byte blocks) is sufficient for most current use cases.

- [ ] **High-Speed Testing** *(Still TODO from v2.5.0)*
  - Test operation at 921600 and 1000000 baud
  - Profile CPU usage and latency
  - Verify packet integrity under load
  - Document any limitations

## Priority 3 - Future Features

- [ ] **USB Backpressure Implementation**
  - Prevent UART reading when USB buffer is full
  - Reduce data loss at high speeds without flow control
  - Add dynamic threshold based on baud rate
  - Show warnings for high-speed configurations without flow control
  - Consider: `if (usbInterface->availableForWrite() < bufferIndex) skip_uart_read();`

- [ ] **Alternative Data Modes**
  - **UDP Bridge Mode** - UART over WiFi UDP
    - Configure target IP and port
    - Useful for wireless telemetry
    - **Implementation**: Use built-in AsyncUDP from ESP32 Arduino Core
    - Support both unicast and broadcast modes
  - **TCP Server Mode** - Accept TCP connections
    - Multiple client support
    - Authentication options
    - **Implementation**: Use AsyncTCP library
    - Consider connection management and timeouts

- [ ] **Protocol-Specific Optimizations**
  - **MAVLink Mode** - Lightweight packet boundary detection
    - Only detect packet boundaries, no full parsing
    - ~30 lines of code vs ~50KB for full parser
    - Reduces latency by sending complete packets immediately
    - Eliminates 5ms pause waiting for packet boundaries
    - **Implementation**: Simple header check to detect packet length
    - No CRC validation, no message decoding - just boundaries
    - Can be enabled/disabled via config option
  - **SBUS Mode** - UART to/from SBUS converter
    - Convert standard UART to SBUS protocol (100000 baud, 8E2, inverted)
    - Convert SBUS to standard UART (configurable baud rate)
    - **Roles to add**:
      - `D1_UART_TO_SBUS`: Device 1 receives UART, Device 2 outputs SBUS
      - `D1_SBUS_TO_UART`: Device 1 receives SBUS, Device 2 outputs UART
    - **Use cases**:
      - Connect non-SBUS flight controllers to SBUS-only receivers
      - Use SBUS receivers with devices expecting standard UART
      - Protocol conversion for legacy equipment
      - **Remote SBUS over network**: Enable SBUS transmission over any network
        - Transmit side: `SBUS receiver → ESP32 (SBUS→UART) → Network device → Internet`
        - Receive side: `Internet → Network device → ESP32 (UART→SBUS) → Flight controller`
        - ESP32 acts only as protocol converter, not network bridge
        - Network transmission handled by external devices (4G modems, routers, etc.)
        - Enables SBUS control over unlimited distances
    - **Implementation**:
      - Hardware inverter required for true SBUS signal
      - SBUS frame parsing/generation (25 bytes, specific format)
      - Configurable channel mapping
      - Timing-critical on SBUS side, relaxed on UART side
    - **Note**: SBUS cannot be transmitted directly over network due to inverted signal and strict timing requirements

- [ ] **Advanced Configuration**
  - Configurable GPIO pins via web interface
  - Support for different ESP32 board variants
  - Import/Export configuration files
  - Configuration profiles for common use cases
  - **Implementation**: Extended config structure with pin mapping

- [ ] **WebSocket Real-time Interface** *(After ESPAsyncWebServer migration)*
  - Real-time log streaming via WebSocket
  - Live statistics updates without polling
  - Bidirectional communication for control
  - **Implementation**: Part of ESPAsyncWebServer features

- [ ] **Network Discovery**
  - mDNS/Bonjour support for easy device discovery
  - SSDP (Simple Service Discovery Protocol)
  - Custom UDP broadcast announcement
  - **Libraries**: Built-in mDNS support in ESP32

## Libraries and Dependencies

### Current Dependencies
```ini
lib_deps =
    bblanchon/ArduinoJson@^7.4.2      # JSON parsing and generation
    fastled/FastLED@^3.10.1            # WS2812 LED control
```

### Planned Dependencies
```ini
lib_deps =
    # For task scheduling (Priority 2)
    arkhipenko/TaskScheduler@^3.7.0
    
    # For async web server (Priority 2)
    https://github.com/ESP32Async/ESPAsyncWebServer.git
    https://github.com/ESP32Async/AsyncTCP.git
    
    # Built-in libraries (no installation needed)
    # - AsyncUDP (included in ESP32 Arduino Core)
    # - mDNS (included in ESP32 Arduino Core)
```

## Notes

- Current adaptive buffering achieves 95%+ efficiency for most protocols
- USB Auto mode needs VBUS detection implementation
- Consider security implications before adding network streaming modes
- Maintain backward compatibility with existing installations
- Version 2.5.2 completes Phase 1 refactoring with excellent maintainability
- Version 2.5.3 completes Phase 2 refactoring with clean modular architecture
- DMA implementation enables hardware-based packet detection and minimal packet loss
- Web interface modularization improves maintainability and development workflow
- Library selection focuses on well-maintained, performance-oriented solutions