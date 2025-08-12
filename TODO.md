# TODO / Roadmap

## COMPLETED ✅

### Priority 1 - Configuration Import/Export ✅ COMPLETED

- [x] **Export Configuration** ✅ COMPLETED
  - Download current config as JSON file
  - Endpoint: `/config/export`
  - Default filename: "esp32-bridge-config-[ID].json" with unique ID
  - Includes ALL settings including passwords
  - Reuses existing JSON serialization from config.cpp
  
- [x] **Import Configuration** ✅ COMPLETED
  - Upload JSON config file via web interface
  - Endpoint: `/config/import`
  - Form-based upload (similar to OTA update)
  - Validation of JSON structure and config version
  - Apply settings and automatic reboot
  
- [x] **Web Interface Updates** ✅ COMPLETED
  - New section "Configuration Backup" 
  - Button "Export Config" → downloads current configuration
  - Form "Import Config" with file selector
  - Progress indication during import
  
- [x] **Implementation benefits** ✅ ACHIEVED:
  - Quick device provisioning
  - Configuration backup/restore
  - Share configurations between devices
  - No need to reconfigure after firmware updates

### Priority 2 - WiFi Client Mode ✅ COMPLETED

#### 2.1 - Basic WiFi Client ✅ COMPLETED
- [x] **Basic WiFi Client Implementation** ✅ COMPLETED
  - Connect ESP32 to existing WiFi network instead of creating AP
  - Store WiFi credentials in LittleFS config file
  - Web interface for WiFi network selection and password entry
  - Basic connection status indication
  - WiFi mode enum (AP, Client) implemented

#### 2.2 - Auto-connect and Fallback ✅ COMPLETED
- [x] **Auto-connect and Fallback Logic** ✅ COMPLETED
  - Auto-connect on boot with saved credentials
  - Triple click logic for mode switching between Standalone/Client/AP
  - Configurable retry attempts and timeouts (5 retries, 10s intervals)
  - Advanced status indication (LED: searching/connected/error)
  - State machine for network mode transitions implemented
  - Device 4 network awareness with EventGroup synchronization
  - Temporary/permanent network mode support

### Priority 4 - Protocol Optimizations (Partial)

#### 4.1 - Protocol Detection Framework ✅ COMPLETED
- [x] **Protocol Detection Framework** ✅ COMPLETED
  - Created base `ProtocolDetector` class with virtual interface
  - Implemented protocol pipeline with comprehensive processing hooks
  - Added `ProtocolType` enum and protocol configuration support
  - Integrated protocol-aware logic into bridge processing and adaptive buffering
  - Added protocol lifecycle management with initialization and maintenance
  - All functions implemented as stubs with zero performance impact
  - **Architecture ready** for Phase 4.2+ protocol implementations

#### 4.2 - MAVLink Parser with Hardware Optimization ✅ COMPLETED
- [x] **MAVLink Packet Detection** ✅ COMPLETED
  - Implemented `MavlinkDetector` using Protocol Framework
  - MAVLink v1/v2 header validation and packet boundary detection
  - Protocol statistics tracking (packets, errors, sizes, rates)
  - Configuration management with version migration (v6→v7)
  - Web interface integration with Protocol Optimization dropdown and statistics display
  - Error handling with resync logic (search for next start byte)
  - **Performance Impact**: Eliminates UART FIFO overflows and reduces latency significantly
  - **Benefits**: Perfect for high-baud MAVLink streams (115200+), preserves packet boundaries

### Out of Priority


### Network Discovery

- [x] **mDNS/Bonjour support** ✅ COMPLETED (implemented in v2.8.0) - for easy device discovery

## PENDING TASKS 🔄

### Priority 4 - FastMAVLink Implementation 🔄 IN PROGRESS

**Goal**: Replace problematic `fmav_parse_to_msg()` with independent frame buffer approach to eliminate packet loss and CRC errors caused by buffer memmove operations.

**Progress Status:**
- [x] ✅ **Root Cause Analysis** - Identified buffer corruption from memmove() conflicts
- [x] ✅ **Solution Design** - Independent 296-byte frame buffer approach 
- [x] ✅ **Code Implementation** - Replaced with `fmav_parse_and_check_to_frame_buf()`
- [x] ✅ **Header File Cleanup** - Removed complex tracking structures and std::map usage
- [x] ✅ **Error Handling Simplification** - Basic error counters instead of detailed statistics
- [x] ✅ **Architecture Cleanup** - Removed updateDetailedStats(), checkSequenceLoss(), logPacketInfo()

**Current Implementation:**
- Uses independent frameBuffer[296] to avoid conflicts with adaptive buffer memmove()
- Byte-by-byte processing with persistent parser state
- Simplified error handling with basic diagnostic counters
- No parser resets after successful packets (automatic IDLE transition)

**Remaining Tasks:**
- [ ] **Field Testing** - Validate <1% packet loss with Mission Planner
- [ ] **MAVFtp Verification** - Test file transfer reliability  
- [ ] **Performance Validation** - Ensure no regressions under high load
- [ ] **Long-term Stability** - 24+ hour continuous operation test
- [ ] **Documentation Update** - Update CHANGELOG.md with final results

**Success Criteria:**
- Packet loss < 1% (vs previous 547 losses)
- CRC errors < 10 (vs previous 349 errors) 
- Detection errors < 100 (vs previous 13,314 errors)
- MAVFtp file transfers work reliably
- No SIGNATURE_ERROR messages in logs

**Next Priority**: After stabilization, proceed to MAVLink Routing (Priority 5)

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

### Priority 6 - Hardware Packet Detection Improvements

- [ ] **Hardware-level Protocol Optimization**
  - Dynamic timeout based on detected protocol (not just MAVLink)
  - Pattern detection for text protocols using `uart_enable_pattern_det_baud_intr()`
  - FIFO threshold optimization for different packet sizes
  - Benefits ALL protocols, not protocol-specific
  - Implementation in `uart_dma.cpp` configuration

### Priority 6 - Device 3 Integration

- [ ] **Device 3 Adaptive Buffering with Protocol Support**
  - Currently uses simple batch transfer (64-byte blocks)
  - Implement adaptive buffering using Protocol Framework
  - Share protocol detector with main channel (Device 1→2)
  - Add packet boundary preservation for detected protocols
  - Extend statistics for multi-device scenarios
  - Improve Mirror/Bridge mode performance for packet-based protocols
  - Benefits for various use cases:
    - **Industrial**: Modbus RTU frame timing preservation
    - **Marine**: Complete NMEA sentences
    - **IoT**: Protocol-aware routing
    - **RS-485**: Intelligent gateway operation

### Priority 7 - SBUS Protocol Support

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

### Priority 7.1 - SBUS Failsafe Mode (после базового SBUS)

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

### Priority 7.2 - SBUS Hybrid Failover with Network Backup

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

### Priority 8 - Multi-Protocol Architecture

- [ ] **Advanced Protocol Management**
  - Per-device protocol configuration (different protocols per Device)
  - Independent protocol detectors per interface
  - Support for protocol conversion (SBUS↔MAVLink, Modbus↔Text)
  - Handle complex routing scenarios with protocol translation
  - **Use Cases**:
    - Device 1: MAVLink, Device 2: SBUS conversion
    - Device 1: Modbus RTU, Device 3: JSON over network
    - Device 1: NMEA GPS, Device 2: Binary protocol conversion

### Priority 9 - Multi-Board Support

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

- [ ] **Protocol Pipeline Refactoring** - After MAVLink stabilization
  - Move MAVLink-specific functions from protocol_pipeline.h to MavlinkDetector
    - `detectMavftpMode()` - purely MAVLink logic
    - `isCriticalPacket()` - MAVLink message priority
    - Message ID extraction logic
  - Make protocol_pipeline.h truly protocol-agnostic
  - Add proper protocol type checking before MAVLink-specific calls
  - Consider virtual methods in ProtocolDetector base class:
    - `isHighPriorityPacket()`
    - `updateProtocolState()`
    - `requiresExtendedTimeout()` - for adaptive_buffer.h
  - Remove MAVLink-specific logic from adaptive_buffer.h:
    - Replace direct `mavftpActive` checks with detector virtual method
    - Replace hardcoded "MAVLINK" strings with dynamic protocol name
  - Move MAVLink-specific fields from BridgeContext to MavlinkDetector:
    - `bool mavftpActive`
    - `uint32_t mavftpCount`
    - `uint32_t lastMavftpTime`
  - **Note**: Marked with "MAVLINK-SPECIFIC" comments in code

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
- [ ] MAVLink packet priorities (commands > telemetry > bulk)
- [ ] UDP: 1 MAVLink packet = 1 datagram (requires parsing)
- [ ] Separate critical/normal queues for USB
- [ ] Adaptive timeouts based on msgid
- [ ] MAVFtp session detection → bulk mode

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


