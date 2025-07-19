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

#### Device 2 — Secondary Communication Channel
- Can be:
  - Disabled
  - UART over GPIO 8/9
  - USB Device mode (connected to PC)
  - USB Host mode (connected to external USB-serial adapter)
- Can be a participant in a bidirectional UART bridge with Device 1.
- Not used for logging.

#### Device 3 — Logging or Mirror UART
- Can be:
  - Disabled
  - Mirror of Device 1 (unidirectional 1 → 3)
  - Full UART bridge with Device 1 (bidirectional)
  - Dedicated UART log channel (TX only) at 115200 baud
- Uses fixed UART pins 11/12.

#### Device 4 — Wi-Fi / Network Channel
- Can be:
  - Disabled
  - Network bridge (e.g. MAVLink-over-UDP)
  - Logging over Wi-Fi (UDP logs)
- Wi-Fi must be active for this device to be used.

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

## Priority 2 - Future Features

- [ ] **Alternative Data Modes**
  - **UDP Bridge Mode** - UART over WiFi UDP
    - Configure target IP and port
    - Useful for wireless telemetry
  - **TCP Server Mode** - Accept TCP connections
    - Multiple client support
    - Authentication options

- [ ] **High-Speed UART Optimization** *(For 921600+ baud)*
  - Consider increasing adaptive buffer beyond 1024 bytes
  - Profile current implementation at maximum speeds
  - Evaluate DMA benefits (requires more ESP-IDF integration)
  - Note: Current implementation works well up to 500000 baud

- [ ] **Protocol-Specific Optimizations**
  - **MAVLink Mode** - Parse and optimize MAVLink packets
    - Beneficial at high speeds (>460800 baud)
    - Can reduce latency by sending complete packets immediately
    - Potential 10-20% throughput improvement
    - Eliminates 5ms pause waiting for packet boundaries
  - **SBUS Mode** - RC receiver protocol support
    - 100000 baud, 8E2, inverted signal
    - Auto-detection and configuration

- [ ] **Advanced Configuration**
  - Configurable GPIO pins via web interface
  - Support for different ESP32 board variants
  - Import/Export configuration files
  - Configuration profiles for common use cases

- [ ] **Code Cleanup and Optimization**
  - Consider extracting button handling to separate module (button_handler.cpp/h)
  - Review uartbridge.cpp for potential optimizations
  - HTML templates could be compressed or minified
  - Consider using LittleFS compression for web resources

## Notes

- Current adaptive buffering achieves 95%+ efficiency for most protocols
- USB Auto mode needs VBUS detection implementation
- Consider security implications before adding network streaming modes
- Maintain backward compatibility with existing installations
- Version 2.3.0 provides clean foundation for Priority 1 implementation
- Version 2.3.1 completes the statistics system refactoring
- Version 2.3.2 provides modern web interface architecture
- Version 2.3.3 resolves critical performance issues with MAVLink