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

## Completed ✅

- [x] **Remove DEBUG_MODE** - Completed in v2.3.0
  - Removed all DEBUG_MODE checks from code
  - Bridge always active in all modes
  - Diagnostics converted to log levels
  
- [x] **Remove CONFIG_FREERTOS_UNICORE** - Completed in v2.3.0
  - Now only supports multi-core ESP32
  - UART task on core 0, Web task on core 1
  
- [x] **Code Organization** - Completed in v2.3.0
  - Extracted diagnostics to separate module (diagnostics.cpp/h)
  - Moved system utilities to system_utils.cpp/h
  - Moved RTC variables and crash update to crashlog.cpp/h
  - main.cpp reduced from ~600 to ~450 lines

## Priority 2 - Future Features

- [ ] **Alternative Data Modes**
  - **UDP Bridge Mode** - UART over WiFi UDP
    - Configure target IP and port
    - Useful for wireless telemetry
  - **TCP Server Mode** - Accept TCP connections
    - Multiple client support
    - Authentication options

- [ ] **High-Speed UART Optimization** *(For 921600+ baud)*
  - Consider increasing adaptive buffer beyond 256 bytes
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