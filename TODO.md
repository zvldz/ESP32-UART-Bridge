# TODO / Roadmap

## PROJECT STATUS SUMMARY

**Current Version**: v2.18.1  
**Current Phase**: Phase 8 - Testing SBUS Implementation  
**Roadmap**:
1. Phase 9-10: Multi-source & Failsafe SBUS
2. Documentation: Update README & Web Help for SBUS
3. Platform Support: ESP32-S3 Super Mini
4. Stabilization: Code cleanup & optimization

## ACTIVE TASKS 🔄

### CURRENT PHASE - Completing SBUS Implementation

#### Phase 8 - Testing (IMMEDIATE) 🔴 READY FOR TESTING

**⚠️ Critical Phase:** Architectural issues found here are easier to fix than after Phase 9-10 implementation. Each test requires complete ESP reconfiguration, wiring changes, and firmware flashing.

- [x] **Physical SBUS Testing** (Phase 4-5 features)
  - [x] SBUS_IN from RC receiver → SBUS_OUT to flight controller
  - [x] Verify 71 FPS generation rate with oscilloscope
  - [x] Test failsafe activation after 100ms signal loss
  - [x] Validate all 16 channels + flags transmission

- [x] **UART Transport Testing** (Phase 6 - UartSbusParser)
  - [x] ESP1 (SBUS_IN) → UART → ESP2 (UART→SBUS_OUT)
  - [x] Test various baudrates (115200, 460800, 921600)
  - [x] Measure end-to-end latency

- [ ] **UDP/WiFi Transport Testing** (Phase 7 - UdpSbusParser)
  - [ ] ESP1 (SBUS_IN) → UDP → ESP2 (UDP→SBUS_OUT)
  - [ ] Measure WiFi latency (target <50ms)
  - [ ] Test with different network conditions
  - [ ] Verify operation in both AP and Client modes

- [x] **Statistics Validation**
  - [x] Verify Real vs Generated frame percentages
  - [x] Check unchanged frame detection accuracy
  - [x] Validate failsafe event counting
  - [x] Confirm TX byte statistics for all devices

#### Phase 9 - Multi-Source Support 🟡 NEXT

**⚠️ Risk:** May require SbusHub architecture changes if current single-state design incompatible with multiple simultaneous sources.

- [ ] **Implement multi-source SBUS handling**
  - [ ] Remove single-source limitation
  - [ ] Add source priority/selection logic
  - [ ] Handle concurrent SBUS inputs (Physical + UART + UDP)
  - [ ] **Device Role Configuration**:
    - Device 1: SBUS output to flight controller
    - Device 2: Primary SBUS receiver input
    - Device 3: Secondary SBUS receiver input
    - Device 4: Network SBUS stream (UDP) as backup

#### Phase 10 - SBUS Failsafe/Redundancy 🟡 PLANNED

- [ ] **SBUS Failsafe Mode**
  - [ ] Automatic failover between multiple SBUS sources
  - [ ] Monitor all SBUS inputs for valid packets
  - [ ] Auto-switch on primary loss (>100ms timeout)
  - [ ] Auto-return to primary when signal restored
  - [ ] Send SBUS failsafe frame when all inputs lost
  - [ ] **Technical Requirements**:
    - Glitch-free switching between sources
    - Preserve SBUS failsafe flags from receivers
    - Configurable timeout thresholds
    - Status indication (LED/Web) showing active source

### DOCUMENTATION UPDATE - After SBUS Completion 🟠 IMPORTANT

#### Update User Documentation

- [ ] **README.md Main File**
  - [ ] Add SBUS protocol section with features
  - [ ] Document all SBUS device role combinations
  - [ ] Add SBUS transport methods (Physical, UART, UDP)
  - [ ] Include SBUS configuration examples
  - [ ] Add wiring diagrams for SBUS connections
  - [ ] Note about hardware inverter requirements

- [ ] **Web Interface Help Page**
  - [ ] Update device role descriptions for SBUS modes
  - [ ] Add SBUS statistics explanation
  - [ ] Document SBUS failsafe behavior
  - [ ] Add troubleshooting section for SBUS
  - [ ] Include optimal settings for different use cases

- [ ] **Configuration Examples**
  - [ ] SBUS RC receiver → Flight controller
  - [ ] SBUS over UART between ESP32s
  - [ ] SBUS over WiFi/UDP setup
  - [ ] Multi-source SBUS with failover (after Phase 10)

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

### STABILIZATION PHASE - After Platform Support 🟢


### COMPLETED FEATURES ✅

#### Protocol-driven Optimizations ✅ COMPLETED
- [x] MAVFtp: Extended timeouts (20ms) - COMPLETED
- [x] SBUS: Implemented with 14ms timing (71 FPS) - COMPLETED v2.18.0-2.18.1
- [ ] CRSF (future): Minimal latency requirements
- [ ] Modbus RTU (future): Inter-frame timing preservation
- Architecture ready - just add new protocol implementations

### Priority 6 - SBUS Protocol Support ✅ IMPLEMENTED v2.18.0-2.18.1

- [x] **SBUS Mode** - UART to/from SBUS converter ✅ COMPLETED
  - [x] Convert standard UART to SBUS protocol (100000 baud, 8E2, inverted) ✅
  - [x] Convert SBUS to standard UART (configurable baud rate) ✅
  - [x] **Uses Protocol Framework from Priority 4.1** ✅
  - **IMPLEMENTED FEATURES (v2.18.0-2.18.1)**:
    - [x] SbusParser - Full SBUS frame parsing with validation
    - [x] SbusHub - State-based architecture (replaced queue-based SbusSender)
    - [x] Auto-protocol detection based on device roles
    - [x] Advanced statistics (real vs generated frames)
    - [x] Failsafe support (100ms timeout)
    - [x] Multi-transport conflict detection
    - [x] **Phase 6 - UART→SBUS Bridge** (UartSbusParser)
    - [x] **Phase 7 - UDP→SBUS Bridge** (UdpSbusParser)
    - [x] **SBUS→UDP Transport** (Raw 25-byte transmission)
  - **Device Roles**:
    - `D1_UART_TO_SBUS`: Device 1 receives UART, Device 2 outputs SBUS
    - `D1_SBUS_TO_UART`: Device 1 receives SBUS, Device 2 outputs UART
  - **Use cases**:
    - Connect non-SBUS flight controllers to SBUS-only receivers
    - Use SBUS receivers with devices expecting standard UART
    - Protocol conversion for legacy equipment
    - Remote SBUS over network infrastructure

### FUTURE PROTOCOLS & FEATURES 🔵

#### System Reliability & Memory Management

- [ ] **Periodic Reboot System**
  - [ ] Implement daily scheduled reboot (TaskScheduler)
    - Configurable interval (default: 24 hours)
    - Scheduled at low-activity time (3:00 AM)
    - Graceful shutdown with connection cleanup
    - Pre-reboot statistics logging
  - [ ] Web interface configuration
    - Enable/disable periodic reboot
    - Configurable interval (12h, 24h, 48h, never)
    - Next reboot countdown display

- [ ] **Emergency Memory Protection**
  - [ ] Critical memory threshold monitoring (TaskScheduler)
    - Critical threshold: 20KB (immediate reboot)
    - Warning threshold: 50KB (log warnings)
    - Check interval: 5 seconds
  - [ ] Emergency reboot implementation
    - 5-second graceful shutdown delay
    - Crash log with memory state
    - RTC memory preservation for post-reboot analysis
  - [ ] Memory statistics enhancement
    - Heap fragmentation tracking
    - Largest free block monitoring
    - Historical memory usage trends

#### Final Code Cleanup

- [ ] **Final Code Cleanup** - After all features are implemented
  - [x] Standardize code indentation to 4 spaces across all files ✅ COMPLETED
  - [ ] Remove unnecessary diagnostic code and debug prints (from Priority 1.7)
    - [ ] Find and remove temporary diagnostic prints
    - [ ] Clean up experimental code blocks
    - [ ] Remove "TEMPORARY DIAGNOSTIC" sections
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

- [ ] **CRSF Protocol** - Minimal latency requirements
- [ ] **Modbus RTU** - Inter-frame timing preservation
- [ ] **NMEA GPS** - GPS data parsing and routing

#### Advanced Protocol Management

- [ ] **Protocol Conversion Features**
  - [x] SBUS↔UART conversion (implemented)
  - [ ] SBUS↔MAVLink conversion
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


**Architecture Status**: Parser + Sender refactoring completed successfully. Memory pool implementation with Meyers singleton provides thread-safe operation. Pipeline architecture stable and optimized.

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


