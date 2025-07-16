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

## Priority 2 - Enhanced Features

- [ ] **Multi-Port UART Bridge/Splitter Mode**
  - **Phase 1 - Core Multi-Port (Higher Priority)**:
    - **Device 1** (Primary): GPIO 4/5 (UART1) - current pins
    - **Device 2** (Flexible):
      - Mode A: USB Serial (current behavior)
      - Mode B: UART0 on GPIO 8/9 (left side of board)
    - **Device 3** (Auxiliary): GPIO 11/12 (UART2, right side of board)
      - Off/Bridge/Monitor/Debug modes
      - Debug port fixed at 115200 baud
    - **Pin selection rationale**:
      - All pins have through-holes (easy soldering)
      - Paired pins are adjacent for clean wiring
      - Left/right separation for better cable management
    - **Operating modes**: Simple bridge, Splitter, Multi-bridge
    - **Benefits**: Hardware debug always available, solves USB Host logging
  
  - **Phase 2 - Network Port (Lower Priority)**:
    - **Device 4** (Network): Virtual port over WiFi
    - Requires "WiFi Mode Improvements" implementation first
    - TCP/UDP/WebSocket support
    - Extends bridge to network connectivity
  
  - **Configuration**:
    - Web interface for mode selection
    - Bridge baud rate for hardware ports
    - Device 3 mode selector
    - Independent log levels for web/UART
  
  - **Code Improvements**:
    - Remove DEBUG_MODE conditionals
    - Unified logging system
    - Clean separation of bridge and debug data

- [ ] **Backup Configuration Recovery**
  - Implement automatic recovery from backup.json when config.json is corrupted
  - Currently backup is created but never used
  - Add integrity check on config load

- [ ] **WiFi Mode Improvements**
  - "Persistent WiFi mode" checkbox in web interface
  - Option to disable automatic 20-minute timeout
  - Manual WiFi on/off control
  - Save WiFi mode preference

- [ ] **USB Device Selection (Host Mode)**
  - Web interface to list connected USB devices
  - Allow selection when multiple CDC devices connected
  - Show device VID/PID and descriptive names
  - Currently only connects to first CDC device found

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