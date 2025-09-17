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

- [ ] **Physical SBUS Testing** (Phase 4-5 features)
  - [ ] SBUS_IN from RC receiver → SBUS_OUT to flight controller
  - [ ] Verify 71 FPS generation rate with oscilloscope
  - [ ] Test failsafe activation after 100ms signal loss
  - [ ] Validate all 16 channels + flags transmission

- [ ] **UART Transport Testing** (Phase 6 - UartSbusParser)
  - [ ] ESP1 (SBUS_IN) → UART → ESP2 (UART→SBUS_OUT)
  - [ ] Test various baudrates (115200, 460800, 921600)
  - [ ] Measure end-to-end latency
  - [ ] Verify frame integrity over long UART cables

- [ ] **UDP/WiFi Transport Testing** (Phase 7 - UdpSbusParser)
  - [ ] ESP1 (SBUS_IN) → UDP → ESP2 (UDP→SBUS_OUT)
  - [ ] Measure WiFi latency (target <50ms)
  - [ ] Test with different network conditions
  - [ ] Verify operation in both AP and Client modes

- [ ] **Statistics Validation**
  - [ ] Verify Real vs Generated frame percentages
  - [ ] Check unchanged frame detection accuracy
  - [ ] Validate failsafe event counting
  - [ ] Confirm TX byte statistics for all devices

#### Phase 9 - Multi-Source Support 🟡 NEXT

**⚠️ Risk:** May require SbusHub architecture changes if current single-state design incompatible with multiple simultaneous sources.

- [ ] **Implement multi-source SBUS handling**
  - [ ] Remove single-source limitation
  - [ ] Add source priority/selection logic
  - [ ] Handle concurrent SBUS inputs (Physical + UART + UDP)

#### Phase 10 - SBUS Failsafe/Redundancy 🟡 PLANNED

- [ ] **SBUS Failsafe Mode**
  - [ ] Automatic failover between multiple SBUS sources
  - [ ] Monitor both SBUS inputs for valid packets
  - [ ] Auto-switch on primary loss (>100ms timeout)
  - [ ] Auto-return to primary when signal restored

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

#### ESP32-S3 Super Mini Support

- [ ] **Hardware Adaptation**
  - [ ] Pin mapping for S3 Super Mini board
  - [ ] Adjust for different GPIO availability
  - [ ] Test with reduced flash/RAM if applicable
  - [ ] Verify USB-CDC functionality

- [ ] **Build Configuration**
  - [ ] Add platformio environment for S3 Super Mini
  - [ ] Adjust partition table if needed
  - [ ] Configure appropriate CPU frequency
  - [ ] Test both single and dual-core modes

- [ ] **Testing on S3 Super Mini**
  - [ ] Verify all UART interfaces work
  - [ ] Test SBUS with hardware inverter
  - [ ] Validate WiFi performance
  - [ ] Check power consumption

### STABILIZATION PHASE - After Platform Support 🟢 

#### Code Cleanup & Optimization

- [ ] **Remove Debug Code** (from Priority 1.7)
  - [ ] Find and remove temporary diagnostic prints
  - [ ] Clean up experimental code blocks
  - [ ] Remove "TEMPORARY DIAGNOSTIC" sections
  - [ ] Finalize production-ready code

- [ ] **Architecture Simplification** (from Priority 1.8)
  - [ ] Evaluate bridge_processing.h removal
  - [ ] Simplify uartBridgeTask
  - [ ] Consolidate initialization
  - [ ] Minimize to Pipeline + USB only

- [ ] **Final Code Cleanup**
  - [ ] Remove unnecessary diagnostic code
  - [ ] Clean up commented-out code blocks
  - [ ] Optimize memory usage
  - [ ] Ensure consistent code style

### COMPLETED FEATURES ✅

#### Protocol-driven Optimizations ✅ COMPLETED
- [x] MAVFtp: Extended timeouts (20ms) - COMPLETED
- [x] SBUS: Implemented with 14ms timing (71 FPS) - COMPLETED v2.18.0-2.18.1
- [ ] CRSF (future): Minimal latency requirements
- [ ] Modbus RTU (future): Inter-frame timing preservation
- [ ] Device 3 (Mirror) can utilize same optimizations
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
  - **IMPLEMENTED BUT NOT TESTED (v2.18.1)**:
    - [x] **Phase 6 - UART→SBUS Bridge** 🟡 NEEDS TESTING
      - UartSbusParser for receiving SBUS over regular UART
      - Flows: UART2→Device3_SBUS_OUT, UART3→Device2_SBUS_OUT
      - Any baudrate support (115200-921600)
    - [x] **Phase 7 - UDP→SBUS Bridge** 🟡 NEEDS TESTING
      - UdpSbusParser for receiving SBUS over WiFi
      - Network Bridge integration
      - Target <50ms latency
    - [x] **SBUS→UDP Transport** 🟡 NEEDS TESTING
      - Sending SBUS frames via UDP packets
      - Raw 25-byte transmission
  - **PENDING**:
    - [ ] **Phase 8**: Real-world testing of all transport modes (CURRENT)
    - [ ] **Phase 9**: Multi-source support (currently single-source only)
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

### Priority 6.1 - SBUS Failsafe Mode (после базового SBUS)

- [ ] **SBUS Failsafe/Redundancy Mode**
  - Automatic failover between multiple SBUS sources
  - **Device Role Configuration**:
    - Device 1: SBUS output to flight controller
    - Device 2: Primary SBUS receiver input
    - Device 3: Backup SBUS receiver input
  - **Failover Logic**:
    - Monitor both SBUS inputs for valid packets
    - Use Device 2 (primary) when signal is good
    - Auto-switch to Device 3 (backup) on primary loss (>100ms timeout)
    - Auto-return to primary when signal restored
    - Send SBUS failsafe frame when both inputs lost
  - **Technical Requirements**:
    - Simultaneous monitoring of multiple SBUS streams
    - Glitch-free switching between sources
    - Preserve SBUS failsafe flags from receivers
    - Configurable timeout thresholds
    - Status indication (LED/Web) showing active source
  - **Use Cases**:
    - Long-range RC with diversity receivers
    - Critical control systems requiring redundancy
    - Competition systems with backup RC link
  - **Implementation Approach**:
    - Extend SBUS protocol handler for multi-input
    - Add failover state machine
    - Implement packet validation and timeout detection
    - Ensure frame-perfect switching without control glitches

### Priority 6.2 - SBUS Hybrid Failover with Network Backup

- [ ] **SBUS Hybrid Failover with Network Backup**
  - Multi-source SBUS with automatic failover including network channels
  - **Architecture**:
    - Device 1: SBUS output to flight controller
    - Device 2: Primary RC receiver (local RF link)
    - Device 3: Secondary RC receiver (optional)
    - Device 4: Network SBUS stream (UDP) as backup channel
  - **Failover Priority Logic**:
    - Primary: Local RC receiver (lowest latency)
    - Secondary: Network stream via WiFi/4G/Satellite
    - Tertiary: Backup RC receiver (if configured)
    - Failsafe: Predetermined values when all sources lost
  - **Network Backup Features**:
    - Automatic switch to network when RF signal lost
    - Latency compensation and jitter buffering
    - Seamless return to RF when signal restored
    - Status indication of active source
  - **Use Cases**:
    - Long-range operations beyond RF range
    - Operations in RF-challenging environments
    - Remote piloting over internet
    - Commercial UAVs with redundant control paths
  - **Technical Challenges**:
    - Synchronize multiple SBUS streams
    - Handle network latency (20-200ms)
    - Smooth transitions without control glitches
    - Priority management with hysteresis

### FUTURE PROTOCOLS & FEATURES 🔵

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

### Code Refactoring and Cleanup


- [ ] **Final Code Cleanup** - After all features are implemented
  - Remove unnecessary diagnostic code and debug prints
  - Clean up commented-out code blocks
  - Optimize memory usage and reduce code duplication
  - Simplify overly complex functions
  - Remove experimental/unused features
  - Update code comments to reflect final implementation
  - Ensure consistent code style across all files
  - Remove temporary workarounds that are no longer needed
  - **Note**: This should be done as the very last step to avoid breaking in-development features

## Future Protocol Pipeline Enhancements

### Next Phase Improvements
- [ ] **Priority Queue Implementation** - Replace std::queue with priority_queue for better packet prioritization
- [ ] **Dynamic CPU Affinity** - Move tasks between cores based on load balancing requirements  
- [ ] **Batch Processing** - Process multiple packets per call for improved efficiency
- [ ] **Flow Control** - Add feedback from Senders to Parser for congestion management
- [ ] **Advanced Memory Management** - Monitor memory pool utilization and implement dynamic pool resizing

### TaskScheduler Integration Tasks
- [ ] **Pipeline Statistics Logging** - Add periodic logging every 30 seconds
- [ ] **Memory Pool Monitoring** - Check pool health every minute
- [ ] **Queue Depth Monitoring** - Track sender queue depths every 10 seconds  
- [ ] **Stuck Packet Cleanup** - Automatic cleanup of stalled packets

**Architecture Status**: Parser + Sender refactoring completed successfully. Memory pool implementation with Meyers singleton provides thread-safe operation. Ready for advanced protocol features.

## Libraries and Dependencies

### Current Dependencies
```ini
lib_deps =
    bblanchon/ArduinoJson@^7.4.2      # JSON parsing and generation
    fastled/FastLED@^3.10.2            # WS2812 LED control
    arkhipenko/TaskScheduler@^3.7.0   # Task scheduling
    ESP32Async/ESPAsyncWebServer@^3.8.0  # Async web server
    ESP32Async/AsyncTCP@^3.4.7        # TCP support for async server
```


