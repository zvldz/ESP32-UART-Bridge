# TODO / Roadmap

## ACTIVE TASKS 🔄

### Priority 1 - Protocol Transport Optimization

#### 1.1 - UDP Batching Enhancement ✅ COMPLETED (v2.15.3)
- [x] **Protocol-driven UDP Batching** ✅ COMPLETED
  - ✅ Implemented MAVFtp-aware batching (20ms vs 5ms for normal telemetry)
  - ✅ Collect multiple atomic packets into single UDP datagram (up to MTU)
  - ✅ Use hints.keepWhole from parser for packet integrity
  - ✅ Added UDP batching disable config option for legacy GCS compatibility
  - ✅ Universal naming: mavlink* → atomic* for all packet types

#### 1.2 - Device 4 Pipeline Integration ✅ COMPLETED (v2.15.3)
- [x] **Pipeline Architecture Stabilization** ✅ COMPLETED
  - ✅ Fixed sender indices (IDX_USB=0, IDX_UART3=1, IDX_UDP=2)
  - ✅ PacketSource routing (SOURCE_TELEMETRY, SOURCE_LOGS, SOURCE_DEVICE4)
  - ✅ Buffer separation (telemetry vs log buffers)
  - ✅ Bridge processing fix for Logger mode
  - ✅ WiFi connectivity universalization

#### 1.3 - Protocol-driven Optimizations 🔄 PARTIALLY IMPLEMENTED
- [x] MAVFtp: Extended timeouts (20ms) - COMPLETED
- [ ] SBUS/CRSF (future): Minimal latency requirements
- [ ] Modbus RTU (future): Inter-frame timing preservation
- [ ] Device 3 (Mirror) can utilize same optimizations
- Architecture ready - just add new protocol implementations

#### 1.4 - UART Refactoring ✅ COMPLETED (v2.15.4)
- [x] **Device3 UART Pipeline Integration** ✅ COMPLETED
  - ✅ Extended Pipeline from 3 to 4 sender slots (IDX_DEVICE2_USB, IDX_DEVICE2_UART2, IDX_DEVICE3, IDX_DEVICE4)
  - ✅ Created Uart2Sender and Uart3Sender classes with specialized functionality
  - ✅ Updated Pipeline routing to support Device2 USB/UART2 mutual exclusivity
  - ✅ Unified Device1 input processing through telemetryBuffer for all Device2 types
  - ✅ Added Device3 Bridge RX polling in main UART task loop
  - ✅ Completely removed Device3 task architecture (device3_task.cpp/h, mutexes, buffers)
  - ✅ Preserved D3_UART3_LOG mode for direct logging system writes
  - ✅ Supports all valid UART combinations: Device2 (USB OR UART2), Device3 (UART3), Device4 (UDP)
  - ✅ **Statistics Refactoring**: Migrated Device3 statistics to Uart3Sender static members (rxBytes, txBytes)
  - ✅ **Diagnostics Update**: Updated diagnostics.cpp to use new Uart3Sender statistics interface

#### 1.5 - LED Centralization ✅ COMPLETED (v2.15.5)
- [x] **Remove LED calls from protocols/senders** ✅ COMPLETED
  - ✅ Removed all LED calls from protocols/senders throughout codebase
  - ✅ Created unified LED monitoring task in TaskScheduler (50ms interval)
  - ✅ LED task reads global statistics from all devices using snapshot comparison
  - ✅ Implemented LedSnapshot structure for efficient state tracking
  - ✅ Bridge mode awareness - task enabled/disabled based on bridge mode

#### 1.6 - Statistics Refactoring ✅ COMPLETED (v2.15.5)
- [x] **Create unified DeviceStatistics structure** ✅ COMPLETED
  - ✅ Migrated all devices to atomic-based DeviceStatistics with DeviceCounter structure
  - ✅ Removed scattered global variables and UartStats structure
  - ✅ Unified web interface statistics using atomic loads (memory_order_relaxed)
  - ✅ Created single source of truth: global DeviceStatistics g_deviceStats
  - ✅ Eliminated critical sections and spinlocks for lock-free performance
  - ✅ Updated all protocol senders to use global statistics directly

#### 1.7 - Diagnostic Cleanup 🔄 PENDING
- [ ] **Remove Debug Code** 🔄 PENDING
  - Find and remove temporary diagnostic prints
  - Clean up experimental code blocks
  - Remove "TEMPORARY DEBUG" sections
  - Finalize production-ready code

### Priority 1.8 - Architecture Simplification (after UART refactoring) 🔄 PENDING

- [ ] **Evaluate bridge_processing.h removal**
  - After UART refactoring - assess remaining code
  - After LED centralization - even less code  
  - After full Protocol Pipeline integration
  - Consider creating BridgeProcessor class
  - Or remove file completely
- [ ] **Simplify uartBridgeTask**
  - Minimize to Pipeline + USB only
  - Consider renaming to pipelineTask
  - Or move to main.cpp if small enough
- [ ] **Consolidate initialization**
  - Merge device_init.cpp logic where appropriate
  - Simplify context initialization

### Priority 5 - MAVLink Routing for Multi-GCS Scenarios

- [ ] **Implement MAVLink Message Routing**
  - **Problem**: When multiple GCS connected (USB + UDP), all messages are duplicated to all interfaces causing:
    - Excessive bandwidth usage (especially during parameter/log downloads)
    - Command conflicts from different GCS
    - Response confusion between GCS instances
  - **Solution**: Intelligent routing based on target_sysid/target_compid:
    - Track request source (which interface)
    - Route responses only to requesting GCS
    - Broadcast messages (HEARTBEAT, STATUS) - to all interfaces
    - Unicast responses (PARAM_VALUE, FILE_TRANSFER_PROTOCOL) - only to requester
  - **Configuration Parameters**:
    - `ROUTING_ENABLED` - enable/disable routing
    - `ROUTING_MODE`:
      - `BROADCAST_ALL` - all to all (current behavior)
      - `SMART_ROUTING` - intelligent routing
      - `FILTER_HEAVY` - filter heavy messages (MAVFtp, parameters)
  - **Use Cases**:
    - Simultaneous Mission Planner (USB) + QGroundControl (UDP)
    - Control channel redundancy
    - Bandwidth saving on slow channels
    - Isolation of critical commands from diagnostics
  - **Priority**: Medium (system works without it, but would be more efficient with routing)
  - **Dependencies**: After protocol detection fixes (current task)
  - **Implementation Approach**:
    - Create routing table mapping sysid/compid to interface
    - Intercept MAVLink headers for routing decisions
    - Maintain session state for request/response pairs
    - Add web interface for routing configuration

### Priority 6 - SBUS Protocol Support

- [ ] **SBUS Mode** - UART to/from SBUS converter
  - Convert standard UART to SBUS protocol (100000 baud, 8E2, inverted)
  - Convert SBUS to standard UART (configurable baud rate)
  - **Uses Protocol Framework from Priority 4.1**
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

### Priority 7 - Multi-Protocol Architecture

- [ ] **Advanced Protocol Management**
  - Per-device protocol configuration (different protocols per Device)
  - Independent protocol detectors per interface
  - Support for protocol conversion (SBUS↔MAVLink, Modbus↔Text)
  - Handle complex routing scenarios with protocol translation
  - **Use Cases**:
    - Device 1: MAVLink, Device 2: SBUS conversion
    - Device 1: Modbus RTU, Device 3: JSON over network
    - Device 1: NMEA GPS, Device 2: Binary protocol conversion

### Priority 8 - Multi-Board Support

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

- [x] **Flow Control System Improvements** ✅ COMPLETED (v2.15.7)
  - ✅ Replaced complex auto-detection system with simple ESP-IDF implementation
  - ✅ Integrated flow control status into UART interface with `getFlowControlStatus()` method
  - ✅ Hardware flow control enabled only for UART1 with proper GPIO configuration (RTS=GPIO6, CTS=GPIO7)
  - ✅ User-controlled activation via web interface instead of automatic detection
  - ✅ Direct ESP-IDF `UART_HW_FLOWCTRL_CTS_RTS` usage with optimized FIFO threshold (100 bytes)
  - ✅ Object-oriented architecture eliminates global variables and complex detection logic
  - **Status**: COMPLETED - Simple, reliable implementation using ESP-IDF built-in support

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


