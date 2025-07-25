# TODO / Roadmap

## Current Device Architecture

### Device Roles

Each device can operate in one of several roles, or be disabled (`None`). Roles and pin assignments are configured through the web interface using dropdown selectors.

#### Device 1 — Main UART Interface (Required)
- **Always enabled**, uses fixed UART TX/RX on GPIO 4/5.
- Protocol is generic UART (e.g. MAVLink, NMEA), not limited to MAVLink.
- This device **does not participate in logging**.
- Uses ESP-IDF DMA implementation exclusively.

#### Device 2 — Secondary Communication Channel
- Can be:
  - Disabled
  - UART over GPIO 8/9 (uses UartDMA with polling)
  - USB Device mode (uses Arduino Serial)
  - USB Host mode (uses ESP-IDF USB Host API)
- Can be a participant in a bidirectional UART bridge with Device 1.
- Not used for logging.

#### Device 3 — Logging or Mirror UART
- Can be:
  - Disabled
  - Mirror of Device 1 (unidirectional 1 → 3)
  - Full UART bridge with Device 1 (bidirectional)
  - Dedicated UART log channel (TX only) at 115200 baud
- Uses fixed UART pins 11/12.
- Uses UartDMA with polling (including logger mode).

#### Device 4 — Wi-Fi / Network Channel (Future)
- Can be:
  - Disabled
  - Network bridge (e.g. MAVLink-over-UDP)
  - Logging over Wi-Fi (UDP logs)
- Wi-Fi must be active for this device to be used.
- **Will use AsyncUDP (built into ESP32 Arduino Core) for implementation**

**Note**: Only one role can be active per device. Conflicting configurations (e.g. enabling both mirror and log on Device 3) must be blocked in the UI.

### Logging System

Three distinct logging channels, each with its own configurable verbosity level:

- **Web (GUI) Logs** — displayed in the browser, optionally with filters.
- **UART Logs** — output through UART (Device 3, TX pin 12).
- **Network Logs** — transmitted over UDP when Wi-Fi is active (Device 4).

Each channel can be enabled or disabled independently and can have its own log level (e.g. ERROR, WARNING, DEBUG).

#### Logging Policies (Default Suggestions)
- **UART logs (Device 3)** — log only errors by default
- **Network logs (Device 4)** — log only errors or be disabled
- **Web logs (GUI)** — full DEBUG by default (only shown in UI)
- **Do not mirror full DEBUG logs to Wi-Fi** — it may affect performance

## Current Status

The project now uses a full ESP-IDF approach for all UART operations and modern AsyncWebServer:
- **Device 1 (Main UART)**: Full ESP-IDF with DMA and event task ✅
- **Device 2 (Secondary)**: ESP-IDF with DMA polling mode ✅
- **Device 3 (Mirror/Bridge/Log)**: ESP-IDF with DMA polling mode ✅
- **USB Device**: Arduino Serial (proven stable) ✅
- **USB Host**: ESP-IDF implementation ✅
- **UART Logger**: ESP-IDF with DMA polling mode ✅
- **Permanent Network Mode**: Fully implemented and configurable ✅
- **Web Server**: ESPAsyncWebServer for non-blocking operations ✅

## Priority 2 - Device 4 Network Implementation

- [ ] **Device 4 Network Implementation** (implement as single unit)
  - UDP Bridge Mode for MAVLink-over-WiFi (client + server)
  - Network logging capabilities via UDP
  - Will use built-in AsyncUDP (no external library needed)
  - Configure via web interface (target IP, port, role selection)
  - Integration with existing device role system
  - **Implementation approach**:
    - Create network_bridge.cpp/h
    - Add to hybrid refactoring structure
    - Support both client and server modes
    - Add Device4Task for network packet handling
  - **Future preparation**:
    - Create base `NetworkChannel` class for future TCP support
    - Design config structure with room for TCP parameters
    - Abstract `LogTransport` interface for different transports
  - **Note**: See detailed implementation plan in `Device 4 Network Implementation - Detailed Plan.md`

### Alternative Captive Portal Implementation
- **Current**: DNSServer + tDnsProcess task (150ms polling)  
- **Alternative**: AsyncWebServer CaptiveRequestHandler class
- **Benefits**: No separate DNS task, better async integration
- **Implementation**:
  ```cpp
  class BridgeCaptiveHandler : public AsyncWebHandler {
      bool canHandle(AsyncWebServerRequest *request) const override { return true; }
      void handleRequest(AsyncWebServerRequest *request) { request->redirect("/"); }
  };
  server.addHandler(new BridgeCaptiveHandler()).setFilter(ON_AP_FILTER);
  ```
- **Device-specific endpoints**: /generate_204 (Android), /hotspot-detect.html (iOS)
- **Priority**: After main ESPAsyncWebServer migration is stable

## Priority 3 - Configuration Import/Export

- [ ] **Export Configuration**
  - Download current config as JSON file
  - Endpoint: `/config/export`
  - Default filename: "esp32-bridge-config.json"
  - Includes ALL settings including passwords
  - Reuses existing JSON serialization from config.cpp
  
- [ ] **Import Configuration**  
  - Upload JSON config file via web interface
  - Endpoint: `/config/import`
  - Form-based upload (similar to OTA update)
  - Validation of JSON structure and config version
  - Apply settings and automatic reboot
  
- [ ] **Web Interface Updates**
  - New section "Configuration Backup" 
  - Button "Export Config" → downloads current configuration
  - Form "Import Config" with file selector
  - Progress indication during import
  
- [ ] **Implementation benefits**:
  - Quick device provisioning
  - Configuration backup/restore
  - Share configurations between devices
  - No need to reconfigure after firmware updates

## Priority 4 - WiFi Client Mode

### 4.1 - Basic WiFi Client
- [ ] **Basic WiFi Client Implementation**
  - Connect ESP32 to existing WiFi network instead of creating AP
  - Store WiFi credentials in LittleFS config file
  - Web interface for WiFi network selection and password entry
  - Basic connection status indication
  - **Future preparation**: WiFi mode enum (AP, Client, AP+Client)

### 4.2 - Auto-connect and Fallback
- [ ] **Auto-connect and Fallback Logic**
  - Auto-connect on boot with saved credentials
  - Automatic fallback to AP mode on connection failure
  - Configurable retry attempts and timeouts
  - Advanced status indication (connected/disconnected/searching)
  - **Future preparation**: State machine for network mode transitions

## Priority 5 - Multi-Board Support

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

## Priority 6 - Protocol Optimizations

### 6.1 - Protocol Detection Framework
- [ ] **Protocol Detection Framework**
  - Create base `ProtocolDetector` class
  - Define interface for packet boundary detection
  - Support for registering multiple detectors
  - Integration points in main data flow
  - **Future preparation**: Architecture ready for SBUS and other protocols

### 6.2 - MAVLink Parser with Hardware Optimization
- [ ] **MAVLink Packet Detection**
  - Implement `MavlinkDetector` using Protocol Framework
  - Simple header check to detect packet length (~30 lines)
  - No CRC validation or message decoding - just boundaries
  - **Hardware Packet Detection Integration**:
    - Dynamic UART idle timeout based on MAVLink timing
    - Pattern detection for MAVLink sync bytes
    - Optimize `rx_timeout_thresh` for MAVLink packets
  - Can be enabled/disabled via config option
  - Reduces latency by eliminating timeout waiting
  - **Note**: Works together with adaptive buffering, not replaces it

### 6.3 - Hardware Packet Detection Improvements
- [ ] **Hardware-level Protocol Optimization**
  - Dynamic timeout based on detected protocol (not just MAVLink)
  - Pattern detection for text protocols using `uart_enable_pattern_det_baud_intr()`
  - FIFO threshold optimization for different packet sizes
  - Benefits ALL protocols, not protocol-specific
  - Implementation in `uart_dma.cpp` configuration

### 6.4 - Device 3 Adaptive Buffering
- [ ] **Device 3 Adaptive Buffering**
  - Currently uses simple batch transfer (64-byte blocks)
  - Implement adaptive buffering using Protocol Framework
  - Apply protocol detectors for intelligent buffering
  - Add packet boundary preservation for detected protocols
  - Improve Mirror/Bridge mode performance for packet-based protocols
  - Benefits for various use cases:
    - **Industrial**: Modbus RTU frame timing preservation
    - **Marine**: Complete NMEA sentences
    - **IoT**: Protocol-aware routing
    - **RS-485**: Intelligent gateway operation

## Priority 7 - SBUS Protocol Support

- [ ] **SBUS Mode** - UART to/from SBUS converter
  - Convert standard UART to SBUS protocol (100000 baud, 8E2, inverted)
  - Convert SBUS to standard UART (configurable baud rate)
  - **Uses Protocol Framework from Priority 6.1**
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

## Out of Priority (Do when in the mood)

- [ ] **Fix Memory Leaks** *(Low priority - objects live for entire runtime)*
  - Add cleanup/shutdown functions for Device 2 and Device 3
  - Add cleanup for UART logger in logging.cpp
  - Consider using smart pointers (unique_ptr) for serial objects
  - Note: Not critical as these objects are never destroyed during normal operation

- [ ] **USB Backpressure Implementation**
  - Prevent UART reading when USB buffer is full
  - Reduce data loss at high speeds without flow control
  - Add dynamic threshold based on baud rate
  - Show warnings for high-speed configurations without flow control
  - Consider: `if (usbInterface->availableForWrite() < bufferIndex) skip_uart_read();`

## Future Considerations

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
    
  - **WebSocket Real-time Interface** *(AsyncWebServer enables this)*
    - Real-time log streaming via WebSocket
    - Live statistics updates without polling
    - Bidirectional communication for control commands
    
  - **Network Discovery**
    - mDNS/Bonjour support for easy device discovery
    - SSDP (Simple Service Discovery Protocol)
    - Custom UDP broadcast announcement

- [ ] **Advanced Configuration**
  - Configurable GPIO pins via web interface
  - Support for different ESP32 board variants
  - Configuration profiles for common use cases

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

### Planned Dependencies
```ini
lib_deps =
    # Built-in libraries (no installation needed)
    # - AsyncUDP (included in ESP32 Arduino Core)
    # - mDNS (included in ESP32 Arduino Core)
```

## Notes

- Current adaptive buffering achieves 95%+ efficiency for most protocols
- USB Auto mode needs VBUS detection implementation
- Consider security implications before adding network streaming modes
- Maintain backward compatibility with existing installations
- Version 2.6.0 introduces ESPAsyncWebServer for better performance and concurrent handling
- DMA implementation enables hardware-based packet detection and minimal packet loss
- Web interface modularization improves maintainability and development workflow
- TaskScheduler implementation significantly simplifies periodic task management
- Library selection focuses on well-maintained, performance-oriented solutions
- Bridge mode renaming provides clearer architecture for future expansion
- Permanent network mode enables always-on Wi-Fi operation for production deployments
- AsyncWebServer migration improves memory usage and enables advanced features like WebSockets
- Protocol optimizations will work together with adaptive buffering for best performance