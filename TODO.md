# TODO / Roadmap

## ACTIVE TASKS 🔄

### PLATFORM SUPPORT - Before Final Cleanup 🟠

#### ESP32-S3 Super Mini Support 🟡 PARTIALLY TESTED

- [x] **Hardware Adaptation**
  - [x] Pin mapping for S3 Super Mini board
  - [x] Adjust for different GPIO availability
  - [x] LED functionality verified (GPIO48 WS2815)
  - [x] Verify USB-CDC functionality

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

#### ESP32 MiniKit Support 🟡 BASIC TESTING COMPLETED

- [x] **Hardware Adaptation** ✅ COMPLETED
  - [x] Pin mapping for ESP32 MiniKit board (ESP32-WROOM-32)
  - [x] GPIO mapping documented (Device1: GPIO4/5, Device3: GPIO16/17, RTS/CTS: GPIO18/19)
  - [x] LED functionality (single color LED on GPIO2, normal logic HIGH=ON)
  - [x] Adjust for different GPIO availability (GPIO6-11 unavailable - SPI flash)
  - [x] No PSRAM - fallback to internal RAM for all operations

- [x] **Build Configuration** ✅ COMPLETED
  - [x] Add platformio environment for ESP32 MiniKit (minikit_production, minikit_debug)
  - [x] Conditional compilation with board flags (BOARD_MINIKIT_ESP32)
  - [x] Web interface board type detection ("minikit")
  - [x] SDK configuration for MiniKit variant (sdkconfig.minikit_*.defaults)
  - [x] USB Host excluded (#if !defined for WROOM)

- [x] **Quick Reset Detection** ✅ COMPLETED (MiniKit has no BOOT button)
  - [x] NVS-based detection (RESET clears RTC memory on this board)
  - [x] 3 quick resets within 3 seconds activates network mode
  - [x] Boot loop protection (crashes/WDT clear counter)
  - [x] 4 uptime write points in main.cpp for reliable detection

- [ ] **Testing on ESP32 MiniKit** 🟡 BASIC TESTING COMPLETED
  - [x] Basic ESP32 operation verified ✅
  - [x] WiFi and web interface functionality ✅
  - [x] Quick reset detection working ✅
  - [x] LED control functional ✅
  - [x] Memory optimization — no OOM crashes after sdkconfig fix ✅
  - [x] Remove DEBUG diagnostics from quick_reset.cpp after verification ✅
  - [x] UDP logging operational ✅
  - [x] Verify Device 1 UART interface (GPIO4/5 pins) ✅
  - [x] UDP telemetry (MAVLink bridge) working ✅
  - [x] Device 2 USB TX/RX verified (tested with CP2104) ✅
  - [ ] Verify Device 3 UART interface (GPIO16/17 pins)
  - [ ] Test SBUS with hardware inverter
  - [ ] Full protocol testing (MAVLink, SBUS, etc.)

**Status**: ESP32 MiniKit (WROOM-32) support implemented and tested. WiFi and web interface working. Quick reset detection working (3 quick resets = network mode). Device 2 = USB via external chip (no native UART2). USB TX/RX verified with CP2104 — no packet loss after buffer initialization fix.

**Note**: Tested with CP2104 USB-UART chip. CH9102 may have issues with DTR/RTS signals causing unwanted resets — not yet verified. ESP32 MiniKit uses classic WROOM-32 chip without PSRAM, resulting in significantly less available RAM compared to S3 variants.

#### Bluetooth Classic SPP (MiniKit only)

- [ ] **Bluetooth SPP as alternative to WiFi**

  **Why MiniKit only**: ESP32-WROOM-32 has Bluetooth Classic, ESP32-S3 does not (only BLE).

  **Use case**: Wireless telemetry to Android apps (QGC, Tower)
  ```
  [FC] → UART → [MiniKit] → Bluetooth SPP → [Android: QGC/Tower]
  ```

  **Advantages over WiFi:**
  - Phone keeps WiFi for internet (no hotspot switching)
  - Simpler pairing (no IP configuration)
  - Lower power consumption
  - Less RAM usage (~20KB vs ~50KB for WiFi buffers)

  **Supported data:**
  - MAVLink telemetry (primary use case)
  - Raw protocol passthrough
  - Text logging

  **Latency note**: BT SPP has 20-100ms latency — acceptable for telemetry, NOT for RC control.

  **Implementation:**
  - `BluetoothSerial` library (built into ESP32 Arduino)
  - New transport layer (like UdpTransport, but for SPP)
  - Config option: WiFi vs Bluetooth mode (mutually exclusive)
  - Web UI for initial config via USB, then BT takes over

  **Pairing:**
  - PIN code required (unlike TX16S-RC which uses "Just Works")
  - `SerialBT.setPin("1234")` or configurable via Web UI
  - BT device name from config (default: "ESP-Bridge-XXXX" with MAC suffix)

### Device 3 SBUS_IN Role ✅ COMPLETED

- [x] **Add D3_SBUS_IN role to Device 3**
  - Enables SBUS input on Device 3 UART pins
  - Needed for ESP32 MiniKit compatibility (Device 2 = USB only)
  - Symmetric with existing D3_SBUS_OUT role

  **Implementation completed:**
  - [x] **SBUS Router integration**
    - Added SBUS_SOURCE_DEVICE3 to SbusRouter sources
    - Failsafe flag handling from Device 3 input
    - Frame timing with Timing Keeper
  - [x] **Output validation**
    - SBUS_IN requires at least one output configured (D1_SBUS_OUT, D3_SBUS_OUT, or UDP)
    - Web UI warns if SBUS_IN enabled without outputs
    - Prevents invalid config: D3_SBUS_IN + D3_SBUS_OUT on same device (conflict)
  - [x] **Role conflict checks**
    - D3_SBUS_IN conflicts with D3_SBUS_OUT (same UART pins)
    - D3_SBUS_IN conflicts with D3_UART3_* roles
    - Validation in config save and Web UI

### FUTURE PROTOCOLS & FEATURES 🔵

#### Web Interface Improvements

- [ ] **Show connected clients info in AP mode**
  - Number of connected stations
  - List of client IP addresses
  - IP of current web interface user

#### SBUS Text Format Output ✅ IMPLEMENTED

- [x] **SBUS → Text format for USB/UDP**
  - [x] Add config option `sbusTextFormat` (bool, default false)
  - [x] Implement `sbusToUs()` converter (raw 173-1811 → µs 1000-2000)
  - [x] Format RC string: `RC 1500,1500,...\r\n` (16 channels)
  - [x] Route SBUS text to USB/UART/UDP when enabled (via sendDirect)
  - [x] Web UI checkbox "Text format" for SBUS Output roles
  - [x] Web UI warning when SBUS Output configured without SBUS Input
  - [x] Protocol optimization auto-set to SBUS when SBUS roles active
  - **Use case**: Replace TX16S-RC Bluetooth dongle with WiFi/USB output
  - **Files**: sbus_text.h, packet_sender.h, uart_sender.h, usb_sender.h, udp_sender.h
  - [ ] **Requires hardware testing** with actual RC receiver

#### SBUS Output Format Selector (Future Enhancement)

- [x] **Replace checkbox with format dropdown** ✅ IMPLEMENTED (v2.18.12)

  Replaced `sbusTextFormat` (bool) with `sbusOutputFormat` (enum) dropdown:
  - Binary (SBUS) — standard 25-byte SBUS frame
  - Text (RC) — `RC 1500,...\r\n` for TX16S-RC plugin
  - MAVLink (RC Override) — RC_CHANNELS_OVERRIDE for direct FC control

- [x] **MAVLink RC_CHANNELS_OVERRIDE output** ✅ IMPLEMENTED (v2.18.12)

  Converts SBUS frames to MAVLink RC_CHANNELS_OVERRIDE packets:
  - system_id=255 (GCS), component_id=190 (UDP_BRIDGE)
  - target_system=1, target_component=1 (FC)
  - Works with Mission Planner/QGC on UDP port 14550
  - ArduPilot RC_OVERRIDE=1 enables automatic failover

- [ ] **Testing MAVLink RC_OVERRIDE** 🟡 NEEDS TESTING
  - [ ] Test with Mission Planner on port 14550
  - [ ] Verify RC channel values in MP RC Calibration
  - [ ] Test failsafe behavior (RC_OVERRIDE timeout)
  - [ ] Measure actual latency (expected ~5-15ms)

#### Advanced Protocol Management

- [ ] **Protocol Conversion Features**

  - [ ] **SBUS↔UART Bridge (ESP↔ESP transport)**

    **Use case**: Two ESP devices create a wired/wireless bridge for SBUS transport
    ```
    [SBUS Receiver] → ESP1 (SBUS IN → UART/UDP OUT) ~~~~ ESP2 (UART/UDP IN → SBUS OUT) → [FC]
    ```

    **UDP path — already implemented:**
    - ESP1: D1_SBUS_IN → D4_SBUS_UDP_TX (binary 25 bytes over UDP)
    - ESP2: D4_SBUS_UDP_RX → D3_SBUS_OUT (binary 25 bytes to FC)

    **UART path — implementation needed:**

    - [ ] **Output side (SBUS → UART)** — 80% ready
      - Infrastructure exists: UART 115200 8N1 init (from sbusTextFormat)
      - Need: "Raw binary" mode — passthrough 25 bytes without text conversion
      - Change: Add `sbusRawFormat` flag or modify sbusTextFormat logic
      - In sendDirect(): if raw mode, skip sbusFrameToText(), send binary directly

    - [ ] **Input side (UART → SBUS)** — needs new code
      - Need: Parse SBUS frames from standard UART stream (115200 8N1)
      - Reuse: `SbusParser` already exists for frame detection
      - New role: D2_UART_SBUS_IN or D3_UART_SBUS_IN
      - Flow: UART RX → SbusParser → validate → SbusRouter::feedFrame()
      - Challenge: No inter-frame gaps on standard UART, rely on 0x0F/0x00 markers

    **Implementation steps:**
    1. Add raw binary output mode (skip text conversion, keep 115200 8N1)
    2. Create new roles: D2_UART_SBUS_IN, D3_UART_SBUS_IN
    3. Connect SbusParser to UART input stream
    4. Register parsed frames with SbusRouter as new source
    5. Add web UI options for UART-SBUS bridge configuration

    **Existing code to reuse:**
    - `SbusParser` (protocols/sbus_parser.h) — frame detection
    - `SbusRouter` — source registration and routing
    - `initDevice*SBUS()` — UART configuration with 115200 mode
    - `sendDirect()` — output path in senders

    **ESP as hardware converter replacement:**
    ```
    Scenario A: ESP replaces SBUS→UART converter
    [SBUS Receiver] → ESP (SBUS IN 100000 8E2 INV) → UART OUT (115200 8N1) → [device]
    Role: D1_SBUS_IN + D2_UART_SBUS_OUT (raw binary, no text conversion)

    Scenario B: ESP replaces UART→SBUS converter
    [device] → UART (115200 8N1) → ESP (UART_SBUS_IN) → SBUS OUT (100000 8E2 INV) → [FC]
    Role: D2_UART_SBUS_IN + D3_SBUS_OUT

    Scenario C: ESP paired with external hardware converter
    [SBUS Receiver] → [HW SBUS→UART] → ESP (UART IN 115200) → SBUS OUT (100000 8E2) → [FC]

    Scenario D: Full ESP↔ESP wireless bridge
    [SBUS RX] → ESP1 (SBUS IN → UDP TX) ~~~WiFi~~~ ESP2 (UDP RX → SBUS OUT) → [FC]

    Scenario E: ESP↔ESP wired bridge (long cable)
    [SBUS RX] → ESP1 (SBUS IN → UART OUT 115200) ~~~wire~~~ ESP2 (UART IN → SBUS OUT) → [FC]
    ```
    - ESP can fully replace both SBUS→UART and UART→SBUS hardware converters
    - Compatible with existing hardware converters (direct wiring)
    - socat and ser2net for network transport
    - Any device that can send/receive 25-byte SBUS frames over standard UART

  - [x] SBUS over WiFi/UDP with frame filtering and validation (implemented)
  - [x] Configurable UDP source timeout for Timing Keeper (100-5000ms, default 1000ms) ✅ v2.18.12
  - [x] Configurable UDP send rate (10-70 Hz, default 50 Hz) for SBUS Output over WiFi ✅ v2.18.12

### Future Considerations (Low Priority)

#### New Protocol Support 🔵 LOW PRIORITY

- [ ] **CRSF Protocol** - RC channels and telemetry for ELRS/Crossfire systems
  - RC channel monitoring for automated switching (video frequencies, ELRS bands)
  - Telemetry data extraction (RSSI, link quality, GPS, voltage, current)
  - Integration with VTX control (SmartAudio/IRC Tramp)
- [ ] **Modbus RTU** - Inter-frame timing preservation
- [ ] **NMEA GPS** - GPS data parsing and routing
- [ ] **Per-device protocol configuration** - Independent protocol selection for each Device
- [ ] **Independent protocol detectors** - Separate MAVLink/SBUS/Raw detection per interface

#### System Reliability & Memory Management 🔵 LOW PRIORITY

- [ ] **Periodic Reboot System**
  - Configurable interval (12h, 24h, 48h, never)
  - Graceful shutdown with connection cleanup
  - Pre-reboot statistics logging

- [ ] **Emergency Memory Protection**
  - Critical threshold: 20KB (reboot after sustained low memory)
  - Warning threshold: 50KB (log warnings)
  - Heap fragmentation tracking

#### Advanced Network Features 🔵 LOW PRIORITY

- [ ] **TCP Client Mode** - Connect to remote servers
  - Automatic reconnection on disconnect
  - Use cases: Cloud logging, remote monitoring

- [ ] **TCP Server Mode** - Accept TCP connections
  - Multiple client support
  - Authentication options

## Libraries and Dependencies

### Current Dependencies
```ini
lib_deps =
    bblanchon/ArduinoJson@^7.4.2      # JSON parsing and generation
    fastled/FastLED@^3.10.3           # WS2812 LED control
    arkhipenko/TaskScheduler@^4.0.3   # Task scheduling
    ESP32Async/ESPAsyncWebServer@^3.9.1  # Async web server
    ESP32Async/AsyncTCP@^3.4.9        # TCP support for async server
```


