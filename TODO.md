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