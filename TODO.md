# TODO / Roadmap

## PENDING TASKS 🔄

### Priority 1 - PyMAVLink Migration ⚠️ PARTIALLY COMPLETED

#### 1.1 - PyMAVLink Library Migration ✅ COMPLETED
- [x] **FastMAVLink → PyMAVLink Migration** ✅ COMPLETED
  - Complete removal of FastMAVLink library and dependencies
  - Full PyMAVLink integration with proper compilation flags
  - Architecture simplification with removal of protocol detection system
  - MAVLink-specific logic moved from pipeline to MavlinkParser
  - Enhanced error handling for non-MAVLink streams

#### 1.2 - CircularBuffer Optimization ✅ COMPLETED
- [x] **Fixed tail=511 deadlock** ✅ COMPLETED - Linearization via tempLinearBuffer
- [x] **Removed shadow buffer system** ✅ COMPLETED - Saved 296 bytes heap per buffer  
- [x] **Simplified consume logic** ✅ COMPLETED - No more boundary jump heuristics
  - Data wrapping now handled transparently with memcpy to temp buffer
  - Parser always gets contiguous view up to 296 bytes
  - Clean consume: always bytesProcessed amount

#### 1.3 - Protocol-Aware Transport Optimization 🔄 PENDING  
- [ ] **Protocol-driven UDP Batching** 🔄 PENDING
  - Replace hardcoded BATCH_TIMEOUT_US with getBatchTimeoutMs() from parser
  - Implement MAVFtp-aware batching (20ms vs 5ms for normal telemetry)
  - Collect multiple MAVLink packets into single UDP datagram (up to MTU)
  - Use hints.keepWhole from parser for packet integrity
  - Add UDP_BATCH_DISABLE config option for legacy GCS compatibility
- [ ] **Protocol-driven Optimizations** 🔄 PENDING (partially implemented)
  - ✅ MAVFtp: Extended timeouts (20ms) - COMPLETED
  - 🔄 SBUS/CRSF (future): Minimal latency requirements
  - 🔄 Modbus RTU (future): Inter-frame timing preservation
  - Device 3 (Mirror) can utilize same optimizations
  - Architecture ready - just add new protocol implementations

#### 1.4 - USB Batching Implementation ✅ COMPLETED
- [x] **USB Batch Transmission** ✅ COMPLETED
  - Collect multiple packets and send with single write() call
  - Reduce system call overhead for high-throughput scenarios  
  - Implement adaptive batching based on queue depth and timing
  - Added pending buffer for partial write protection
  - Implemented N/X/T thresholds (4 packets/448 bytes/5ms timeout)
  - Complete architecture rewrite with helper methods

#### 1.5 - Temporary Diagnostic Cleanup 🔄 PENDING
- [ ] **Remove Debug Code** 🔄 PENDING
  - Find and remove temporary diagnostic prints
  - Clean up experimental code blocks
  - Remove "TEMPORARY DEBUG" sections
  - Finalize production-ready code

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

- [ ] **USB Backpressure Enhancement** - Revisit behavioral port detection
  - **Background**: Complex USB port state detection was implemented in v2.10.0 but removed due to stability issues
  - **Previous Implementation**:
    - Behavioral detection using write buffer availability patterns
    - Dynamic thresholds (ASSUME_CLOSED_THRESHOLD=20, FIRST_ATTEMPT_THRESHOLD=5)
    - Early data dropping when COM port closed
    - Automatic protocol detector reset on port state changes
  - **Issues Encountered**:
    - Stability problems on different systems
    - False positives in port closure detection
    - Over-complexity for marginal benefit
  - **Current State**: Reverted to simple `Serial.availableForWrite()` approach
  - **Future Approach** (if needed):
    - Simpler heuristics with longer observation windows
    - Configurable sensitivity levels
    - Better testing across different USB host systems
    - Optional feature (disabled by default)
  - **Priority**: Low - current simple approach works well for most cases

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

### Circular Buffer Optimizations (v2.6.x)
- [x] Basic circular buffer implementation
- [x] Gap-based traffic detector  
- [ ] Buffer utilization metrics monitoring in web UI
- [ ] Configurable drop thresholds via web config

### Protocol-aware Optimizations (v2.7.0)
- [x] MAVLink packet priorities (commands > telemetry > bulk) ✅ IMPLEMENTED in parser architecture
- [x] UDP: 1 MAVLink packet = 1 datagram ✅ IMPLEMENTED via UdpSender batching
- [x] Separate critical/normal queues ✅ IMPLEMENTED via PacketSender priority handling
- [ ] Adaptive timeouts based on msgid
- [ ] MAVFtp session detection → bulk mode

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

### Protocol Pipeline TODO (from refactoring plan)
- [ ] **Slab Allocator Monitoring** - Monitor memory pool fragmentation during long operations
- [ ] **Priority Queue Enhancement** - Replace std::queue with priority_queue for better priority handling
- [ ] **Dynamic CPU Affinity** - Move tasks between cores based on load
- [ ] **Batch Processing** - Process multiple packets per call for efficiency
- [ ] **Flow Control Implementation** - Add feedback from Senders to Parser for rate control
- [ ] **TaskScheduler Integration** - Add periodic tasks for:
  - Pipeline statistics logging every 30 seconds
  - Memory pool health checks every minute  
  - Queue depth monitoring every 10 seconds
  - Automatic cleanup of stuck packets

**Architecture Status**: Parser + Sender refactoring completed successfully. Memory pool implementation with Meyers singleton provides thread-safe operation. Ready for advanced protocol features.

### USB Optimizations (v2.7.x)
- [ ] Async USB transmission queue
- [ ] USB flow control detection
- [ ] Alternative: USB bulk endpoints instead of CDC

## Libraries and Dependencies

### Current Dependencies
```ini
lib_deps =
    bblanchon/ArduinoJson@^7.4.2      # JSON parsing and generation
    fastled/FastLED@^3.10.1            # WS2812 LED control
    arkhipenko/TaskScheduler@^3.7.0   # Task scheduling
    ESP32Async/ESPAsyncWebServer@^3.7.10  # Async web server
    ESP32Async/AsyncTCP@^3.4.5        # TCP support for async server
```


