# TODO / Roadmap

## Recently Completed ✅

- [x] **USB Host Mode Support** - COMPLETED
  - USB Host mode for connecting USB modems/devices to ESP32
  - Device/Host/Auto mode selection in web interface
  - Hybrid Arduino + ESP-IDF implementation
  - Auto-detection of CDC devices (bInterfaceClass == 0x02)
  - Seamless integration with existing UART bridge logic

## Priority 1 - Code Optimization & Cleanup

- [ ] **Fix Spurious Bootloader Mode Entry**
  - **Option 1: Disable DTR/RTS bootloader trigger**
    - Complete immunity to spurious resets
    - Firmware upload only via BOOT+RESET buttons
    - Most reliable but less convenient
  
  - **Option 2: Smart DTR/RTS filtering**
    - Analyze timing patterns of legitimate bootloader sequence
    - Implement debouncing/filtering for DTR/RTS signals
    - Allow normal resets but block bootloader entry
    - Study USB_SERIAL_JTAG peripheral registers for configuration options
    - May need ESP-IDF modifications or workarounds
  
  - Start with Option 2, fallback to Option 1 if not feasible

- [ ] **Remove Legacy Code** *(Partially completed)*
  - ✅ TODO comments cleaned from defines.h
  - ✅ Unused #defines removed
  - [ ] Check all files for remaining TODO comments
  - [ ] Review for any commented-out code blocks
  - [ ] Remove unused function declarations

- [ ] **Code Size Reduction**
  - main.cpp is getting large (~600 lines)
  - Consider extracting button handling to separate module
  - Review uartbridge.cpp for potential optimizations
  - HTML templates could be compressed or minified


## Priority 2 — Logging System and Device Roles

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
## Priority 3 - Performance Optimization

- [ ] **High-Speed UART Optimization** *(For 921600+ baud)*
  - Consider increasing adaptive buffer beyond 256 bytes
  - Profile current implementation at maximum speeds
  - Evaluate DMA benefits (requires more ESP-IDF integration)
  - Note: Current implementation works well up to 500000 baud

- [ ] **Diagnostic System Refactoring**
  - Make diagnostic levels configurable via web interface
  - Separate production vs debug diagnostics
  - Add performance metrics display
  - Memory usage trends over time

## Priority 4 - Future Features

- [ ] **Alternative Data Modes**
  - **UDP Bridge Mode** - UART over WiFi UDP
    - Configure target IP and port
    - Useful for wireless telemetry
  - **TCP Server Mode** - Accept TCP connections
    - Multiple client support
    - Authentication options
  - **WebSocket Streaming** - Real-time data in browser
    - Built-in terminal emulator
    - Data visualization

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

## Low Priority / Nice to Have

- [ ] **UI Improvements**
  - Dark theme toggle
  - Mobile-responsive improvements
  - Real-time data graphs
  - Connection quality indicators

- [ ] **Documentation**
  - Video tutorials
  - Protocol-specific setup guides
  - Troubleshooting flowchart
  - Performance tuning guide

## Notes

- Current adaptive buffering achieves 95%+ efficiency for most protocols
- USB Auto mode needs VBUS detection implementation
- Consider security implications before adding network streaming modes
- Maintain backward compatibility with existing installations

