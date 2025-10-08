# TODO / Roadmap

## ACTIVE TASKS 🔄

### PLATFORM SUPPORT - Before Final Cleanup 🟠

#### ESP32-S3 Super Mini Support 🟡 PARTIALLY TESTED

- [x] **Hardware Adaptation**
  - [x] Pin mapping for S3 Super Mini board
  - [x] Adjust for different GPIO availability
  - [x] LED functionality verified (GPIO48 WS2815)
  - [ ] Verify USB-CDC functionality

- [x] **Build Configuration**
  - [x] Add platformio environment for S3 Super Mini
  - [x] Conditional compilation with board flags
  - [x] Web interface board type detection

- [ ] **Testing on S3 Super Mini** 🟡 BASIC TESTING COMPLETED
  - [x] Basic ESP32 operation verified
  - [x] WiFi and web interface working
  - [x] LED control functional
  - [x] UDP logging operational
  - [ ] Verify all UART interfaces work (Device 2, 3, 4)
  - [ ] Test SBUS with hardware inverter (critical for RC)
  - [ ] Check power consumption
  - [ ] Full protocol testing (MAVLink, SBUS, etc.)

**Status**: Super Mini support implemented but not fully tested. Basic functionality (WiFi, web, LEDs, UDP logs) confirmed working. UART and SBUS functionality requires hardware testing with actual devices.

#### XIAO ESP32-S3 Support ✅ PARTIALLY TESTED

- [x] **Hardware Adaptation** ✅ COMPLETED
  - [x] Pin mapping for XIAO ESP32-S3 board (D0-D10 castellated pins)
  - [x] GPIO mapping documented (Device1: D3/D4, Device2: D8/D9, Device3: D6/D7, RTS/CTS: D0/D1)
  - [x] LED functionality (single color LED on GPIO21, blink-only mode with inverted logic)
  - [x] Adjust for different GPIO availability (11 GPIO pins available)
  - [x] External antenna support configuration
  - [x] Consider compact form factor constraints

- [x] **Build Configuration** ✅ COMPLETED
  - [x] Add platformio environment for XIAO ESP32-S3 (xiao_production, xiao_debug)
  - [x] Conditional compilation with board flags (BOARD_XIAO_ESP32_S3)
  - [x] Web interface board type detection and D-pin display
  - [x] SDK configuration for XIAO variant

- [x] **Testing on XIAO ESP32-S3** 🟡 BASIC MODE TESTED (Device1 UART + Device2 USB)
  - [x] Basic ESP32 operation verified ✅
  - [x] WiFi and web interface functionality ✅
  - [x] LED control (blink patterns only, no RGB) ✅
  - [x] Device 1 (UART Bridge) on D3/D4 tested with flight controller ✅
  - [x] Device 2 (USB) tested with Mission Planner ✅
  - [ ] External antenna range/stability testing
  - [ ] UDP logging operational
  - [ ] Verify Device 3 UART interface (D6/D7 pins)
  - [ ] Test RTS/CTS flow control (D0/D1 pins)
  - [ ] Test SBUS with hardware inverter
  - [ ] Test SBUS over WiFi/UDP with external antenna
  - [ ] Check power consumption (important for small board)
  - [ ] Full protocol testing (MAVLink, SBUS, etc.)
  - [ ] Thermal testing (compact board heat dissipation)

**Status**: XIAO ESP32-S3 support fully implemented and basic configuration tested (Device1 UART Bridge + Device2 USB working with flight controller and Mission Planner). Pin mapping verified: all GPIO pins are available on castellated edge holes. Web interface correctly displays D-pin names (D3/D4 for Device1, D8/D9 for Device2, D6/D7 for Device3). LED blink-only mode working. Need extended testing for Device3, RTS/CTS, network modes, and protocols.

**Note**: XIAO ESP32-S3 is even more compact than Super Mini. Has external antenna connector which is beneficial for network operations like SBUS over UDP/WiFi, improving range and reliability. This is important as boards like Zero with PCB antennas can have unstable ping times causing SBUS packet loss when timing requirements (14ms frame rate) are not met. External antenna should provide more stable connection for time-critical protocols.

### FUTURE PROTOCOLS & FEATURES 🔵

#### Protocol-driven Optimizations
- [ ] CRSF (future): Minimal latency requirements
- [ ] Modbus RTU (future): Inter-frame timing preservation

#### System Reliability & Memory Management

- [ ] **Periodic Reboot System**
  - [ ] Implement daily scheduled reboot (TaskScheduler)
    - Configurable interval (default: 24 hours)
    - Scheduled at low-activity time (3:00 AM)
    - Graceful shutdown with connection cleanup
    - Pre-reboot statistics logging
    - 5-second graceful shutdown delay
    - Crash log with memory state
    - RTC memory preservation for post-reboot analysis
  - [ ] Web interface configuration
    - Enable/disable periodic reboot
    - Configurable interval (12h, 24h, 48h, never)
    - Next reboot countdown display

- [ ] **Emergency Memory Protection**
  - [ ] Critical memory threshold monitoring (TaskScheduler)
    - Critical threshold: 20KB (immediate reboot)
    - Warning threshold: 50KB (log warnings)
    - Check interval: 5 seconds
  - [ ] Memory statistics enhancement
    - Heap fragmentation tracking
    - Largest free block monitoring
    - Historical memory usage trends

#### Final Code Cleanup

- [ ] **Release Package Requirements** 📦
  - [ ] Each release archive must contain both firmware.bin AND firmware.elf
  - [ ] firmware.bin - for flashing to device
  - [ ] firmware.elf - for crash debugging with addr2line (debug symbols)
  - [ ] Both files must match exactly (same build)
  - [ ] Recommended structure: `release_vX.Y.Z_<board>_<variant>.zip` containing both files
  - [ ] Consider adding CHANGELOG.md and debug instructions to release archive

- [ ] **Add WiFi AP name uniqueness** - Before final cleanup
  - [ ] Add unique suffix to WiFi AP name (like mDNS naming)
  - [ ] Use MAC address last bytes or random suffix
  - [ ] Prevent AP name conflicts in multi-device environments
  - [ ] Update web interface to show unique AP name


- [x] **PSRAM memory optimization** ✅ COMPLETED - Buffer and JSON optimization
  - [x] Move large JsonDocument allocations to PSRAM for config/web operations
  - [x] Move non-critical log buffer to PSRAM when available
  - [x] Add PSRAM fallback to internal RAM when PSRAM unavailable
  - [x] Optimize buffer sizes based on device role and protocol type
  - [x] Add PSRAM status reporting to all diagnostic functions

- [ ] **Additional Memory Optimizations** - Future improvements
  - [x] String class optimization - Eliminated String concatenation in hot paths (replaced with snprintf/char buffers)
  - [ ] Evaluate feasibility of moving more non-critical data to PSRAM
  - [ ] Static analysis of memory usage patterns
  - [ ] Consider compile-time protocol selection to reduce unused code
  - [ ] Evaluate template-based buffer management for type safety

- [ ] **Final Code Cleanup** - After all features are implemented
  - [x] Standardize code indentation to 4 spaces across all files ✅ COMPLETED
  - [ ] Remove unnecessary diagnostic code and debug prints
    - [ ] Find and remove temporary diagnostic prints added during development
    - [ ] Clean up experimental code blocks from testing phases
    - [ ] Remove "TEMPORARY DIAGNOSTIC" sections (keep permanent diagnostics)
    - [ ] Remove commented-out diagnostic blocks that are no longer needed
  - [ ] Clean up commented-out code blocks
  - [ ] Run cppcheck static analysis to find unused functions and variables
  - [ ] Create script to remove trailing whitespace and convert tabs to 4 spaces in all source files
  - [ ] Optimize memory usage and reduce code duplication
  - [ ] Simplify overly complex functions
  - [ ] Remove experimental/unused features
  - [ ] Update code comments to reflect final implementation
  - [ ] Ensure consistent code style across all files
  - [ ] Remove temporary workarounds that are no longer needed
  - [ ] Finalize production-ready code
  - **Note**: This should be done as the very last step to avoid breaking in-development features

#### New Protocol Support

- [ ] **CRSF Protocol** - RC channels and telemetry for ELRS/Crossfire systems
  - [ ] RC channel monitoring for automated switching (video frequencies, ELRS bands)
  - [ ] Telemetry data extraction (RSSI, link quality, GPS, voltage, current)
  - [ ] Integration with VTX control (SmartAudio/IRC Tramp) for video channel/power management
  - [ ] Support for video RX control (button emulation, SPI direct control)
  - [ ] Automatic dual-band switching for ELRS setups based on RC channel input
- [ ] **Modbus RTU** - Inter-frame timing preservation
- [ ] **NMEA GPS** - GPS data parsing and routing

#### Advanced Protocol Management

- [ ] **Protocol Conversion Features**
  - [ ] SBUS↔UART conversion (NOT implemented - removed in Phase 2)
    - [ ] Transport SBUS frames over UART at different baudrates
    - [ ] Compatibility with hardware UART-SBUS converters (direct wiring)
    - [ ] Compatibility with software SBUS-UART converters
    - [ ] Compatibility with socat and ser2net for network transport
    - [ ] Parse SBUS frames from UART stream and feed to SbusRouter
    - [ ] Planned combinations: D2_UART2+D3_SBUS_OUT, D3_UART3_BRIDGE+D2_SBUS_OUT
  - [x] SBUS over WiFi/UDP with frame filtering and validation (implemented)
  - [ ] Modbus↔JSON conversion
  - [ ] NMEA→Binary conversion
  - Per-device protocol configuration
  - Independent protocol detectors per interface

### Future Considerations

- [ ] **Advanced Network Features**
  - **TCP Client Mode** - Connect to remote servers
    - Automatic reconnection on disconnect
    - Configurable server address and port
    - **Implementation**: Use AsyncTCP library
    - Use cases: Cloud logging, remote monitoring
    - **Note**: Low priority - UDP covers most use cases
    
  - **TCP Server Mode** - Accept TCP connections
    - Multiple client support with connection management
    - Authentication options for secure access
    - Connection timeout and cleanup handling

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
    fastled/FastLED@^3.10.2           # WS2812 LED control
    arkhipenko/TaskScheduler@^3.8.5   # Task scheduling
    ESP32Async/ESPAsyncWebServer@^3.8.1  # Async web server
    ESP32Async/AsyncTCP@^3.4.8        # TCP support for async server
```


