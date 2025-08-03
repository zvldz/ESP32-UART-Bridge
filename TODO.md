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

- [x] **USB Backpressure Implementation** ✅ COMPLETED
  - ✅ Behavioral USB port state detection implemented (v2.10.0)
  - ✅ Early data dropping when COM port is closed prevents buffer overflow
  - ✅ Dynamic thresholds based on buffer availability patterns
  - ✅ Honest API returns 0 when unable to write, allowing caller data management
  - ✅ Automatic protocol detector reset on port state changes
  - ✅ Enhanced diagnostics - UART FIFO overflow now indicates real performance issues only

### Network Discovery

- [x] **mDNS/Bonjour support** ✅ COMPLETED (implemented in v2.8.0) - for easy device discovery

## PENDING TASKS 🔄

### Priority 3 - Multi-Board Support

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

### Priority 4 - Protocol Optimizations (Remaining)

#### 4.2 - MAVLink Parser - Advanced Features
- [ ] **Priority 4.2.1 - Performance Analysis**
  - Detailed profiling under high load conditions
  - Real-world performance optimization if needed
  - Document measured improvements vs adaptive buffering
  - Latency benchmarking with different MAVLink message rates

- [ ] **Priority 4.2.2 - Validation Depth Decision**
  - Implement extended validation based on false positive rate analysis
  - Add optional CRC check for packet integrity validation
  - Make validation level configurable (header-only vs full validation)
  - Balance between performance and reliability

- [ ] **Priority 4.2.3 - Extended MAVLink Features**
  - Message type statistics and frequency analysis
  - System/Component ID tracking and validation
  - Configurable validation levels per use case
  - MAVLink message filtering capabilities

- [ ] **Priority 4.2.4 - Performance Optimizations**
  - Adaptive algorithms for high packet rate scenarios
  - Caching mechanisms for repeated packet patterns
  - Hardware integration preparation for future ESP32 features
  - Memory usage optimization

- [ ] **Priority 4.2.5 - Resynchronization Enhancement**
  - Implement synchronization state tracking
  - Smarter recovery strategies after data corruption
  - Prevent data loss during resync operations
  - Configurable resync search window size

#### 4.3 - Hardware Packet Detection Improvements
- [ ] **Hardware-level Protocol Optimization**
  - Dynamic timeout based on detected protocol (not just MAVLink)
  - Pattern detection for text protocols using `uart_enable_pattern_det_baud_intr()`
  - FIFO threshold optimization for different packet sizes
  - Benefits ALL protocols, not protocol-specific
  - Implementation in `uart_dma.cpp` configuration

#### 4.4 - Device 3 Integration
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

#### 4.5 - Multi-Protocol Architecture
- [ ] **Advanced Protocol Management**
  - Per-device protocol configuration (different protocols per Device)
  - Independent protocol detectors per interface
  - Support for protocol conversion (SBUS↔MAVLink, Modbus↔Text)
  - Handle complex routing scenarios with protocol translation
  - **Use Cases**:
    - Device 1: MAVLink, Device 2: SBUS conversion
    - Device 1: Modbus RTU, Device 3: JSON over network
    - Device 1: NMEA GPS, Device 2: Binary protocol conversion

### Priority 5 - SBUS Protocol Support

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

- [ ] **Advanced Configuration**
  - Configurable GPIO pins via web interface
  - Support for different ESP32 board variants

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
    fastled/FastLED@^3.10.1            # WS2812 LED control
    arkhipenko/TaskScheduler@^3.7.0   # Task scheduling
    ESP32Async/ESPAsyncWebServer@^3.7.10  # Async web server
    ESP32Async/AsyncTCP@^3.4.5        # TCP support for async server
```


