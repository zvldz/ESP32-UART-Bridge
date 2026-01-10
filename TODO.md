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
  - [x] Verify Device 3 UART interface (GPIO16/17 pins) ✅
  - [x] Test SBUS with hardware inverter (Device 3 SBUS_IN → BT SBUS Text) ✅
  - [ ] Full protocol testing (MAVLink, SBUS, etc.)

**Status**: ESP32 MiniKit (WROOM-32) support implemented and tested. WiFi and web interface working. Quick reset detection working (3 quick resets = network mode). Device 2 = USB via external chip (no native UART2). USB TX/RX verified with CP2104 — no packet loss after buffer initialization fix.

**Note**: Tested with CP2104 USB-UART chip. CH9102 may have issues with DTR/RTS signals causing unwanted resets — not yet verified. ESP32 MiniKit uses classic WROOM-32 chip without PSRAM, resulting in significantly less available RAM compared to S3 variants.

#### Memory Optimization (MiniKit) — Usage Scenarios

  Current situation: ~35KB free heap with WiFi+BT+WebServer enabled

  - [ ] **Scenario A: WiFi only (D5_NONE)**
    - Release BT memory: `esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)` — frees ~30KB
    - Expected heap: ~65KB — sufficient for stable WiFi+UDP+WebServer
    - Use case: network telemetry without Bluetooth

  - [ ] **Scenario B: BT only (no network)**
    - Release WiFi memory: `esp_wifi_deinit()` + release — frees ~40KB
    - Expected heap: ~75KB — sufficient for stable BT operation
    - Use case: standalone BT serial adapter (SBUS text to phone)
    - WebServer only on quick reset for configuration

  - [ ] **Scenario C: Headless WiFi+UDP+BT**
    - Disable AsyncWebServer in normal operation — saves ~15-30KB runtime heap
    - Keep WiFi for UDP bridge, enable BT simultaneously
    - Expected heap: ~50-65KB
    - WebServer only on quick reset (for configuration)
    - mDNS still needed for quick reset discovery
    - Use case: full bridge functionality (UART↔UDP + UART↔BT)

  Implementation notes:
  - Memory release is one-way (cannot re-enable without reboot) — fits "save = reboot" model
  - Prefer runtime selection based on config (D5 role, D4 role) — avoid separate builds
  - Separate build configs only as last resort
  - Map file analysis: ESPAsyncWebServer ~109KB flash, web_api+web_interface ~97KB flash (static RAM minimal ~65 bytes)

  **Memory notes (MiniKit ESP-IDF BT):**
  - UART1_TX_RING_SIZE reduced to 4KB for MiniKit (was 8KB) — may need to revert if issues

  **Heap measurements (MiniKit):**
  - v2.8.11 (no BT in sdkconfig, WiFi): **76KB** free
  - v2.18.12 (CONFIG_BT_ENABLED=y, D5_NONE, WiFi): **~35KB** free — ~40KB consumed by BT stack static allocation
  - v2.18.x (D5_BT_BRIDGE, standalone mode, no WiFi): **~70KB** free, Min=70KB, MaxBlock=55KB — good margin for buffers

  **Binary size comparison (xtensa-esp32-elf-size):**
  ```
  Section   v2.8.11      v2.18.12     Difference
  text      977 KB       1,267 KB     +290 KB (flash)
  data      178 KB       239 KB       +61 KB
  bss       47 KB        68 KB        +21 KB (RAM)
  ```
  CONFIG_BT_ENABLED=y adds ~21KB .bss (static RAM) even without BT initialization.
  Remaining ~20KB — likely .data sections of BT stack.

### DOCUMENTATION 📝

- [ ] **Update README with MiniKit Bluetooth**
  - Device 5 roles: BT Bridge, BT SBUS Text
  - SSP pairing (no PIN)
  - Use case: wireless telemetry to Android apps, RC Override plugin

### FUTURE PROTOCOLS & FEATURES 🔵

#### Web Interface Improvements

- [ ] **Show connected clients info in AP mode**
  - Number of connected stations
  - List of client IP addresses
  - IP of current web interface user

- [ ] **JavaScript Architecture Refactoring** 🔵 TECHNICAL DEBT

  **Problem**: Web UI code grew organically, accumulated technical debt:
  - Toggle logic scattered across 4 files (main.js, status-updates.js, form-utils.js, crash-log.js)
  - Mixed responsibilities in StatusUpdates (status + logs + SBUS controls + protocol stats)
  - Duplicate patterns (each toggle function repeats: isOpening → loadFragment → rememberedToggle → postAction)
  - Inconsistent element caching (some cached in init, some fetched dynamically)
  - Build functions generate HTML as strings (buildSystemInfo, buildDeviceStats, renderSbusStats)

  **Proposed structure**:
  ```
  utils.js          - pure utilities (formatting, fetch, localStorage)
  ui-sections.js    - unified toggle/restore logic for all collapsible sections
  status-display.js - status and statistics display only
  device-config.js  - device configuration (keep as is)
  form-handler.js   - form handling (save, backup, firmware)
  main.js           - initialization only
  ```

  **Benefits**:
  - Single place for section toggle logic
  - Clear separation of concerns
  - Easier to maintain and extend
  - Reduced code duplication

  **Priority**: Low — current code works, refactor when adding new features

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

#### BLE SBUS for ESP32-S3 🔵 FUTURE

- [ ] **BLE GATT SBUS Output (ESP32-S3 only)**

  **Why**: ESP32-S3 has BLE but no Bluetooth Classic. BLE enables wireless SBUS to GCS with custom plugins.

  **Use case**: SBUS from RC receiver to Mission Planner/QGC via BLE
  ```
  [SBUS RX] → [ESP32-S3] → BLE GATT notify → [MP/QGC Plugin]
  ```

  **Advantages over WiFi:**
  - Phone/PC keeps WiFi for internet
  - Lower latency than Classic SPP (7.5-30ms vs 20-100ms)
  - Works on all S3 boards (Zero, SuperMini, XIAO)

  **Technical details:**
  - NimBLE stack (~20-30KB static RAM, lighter than Bluedroid)
  - GATT Server with SBUS characteristic
  - Notifications (ESP → Client), no pairing required for unencrypted data
  - SBUS frame (25 bytes) fits in single BLE packet (MTU ≥ 25)
  - Throughput ~20KB/s — sufficient for SBUS (70Hz × 25 bytes = 1.75KB/s)

  **Implementation (ESP side):**
  - [ ] Add NimBLE to sdkconfig for S3 boards
  - [ ] BluetoothBLE class (bluetooth/bluetooth_ble.h/.cpp)
  - [ ] SBUS GATT service and characteristic
  - [ ] Integration with protocol pipeline (SENDER_BLE)
  - [ ] D5_BLE_SBUS role for S3 boards

  **Implementation (GCS side):**
  - [ ] MP Plugin: BLE transport using Windows.Devices.Bluetooth API
  - [ ] Device discovery and connection UI
  - [ ] GATT characteristic subscription for SBUS data
  - [ ] Parse SBUS/Text format → RC_CHANNELS_OVERRIDE

  **Memory considerations:**
  - S3 boards: ~70-76KB free → with NimBLE ~45-55KB free
  - May require Scenario C (no WebServer in runtime) for stability
  - Alternative: separate builds with/without BLE (`zero_ble_production`)

  **Note**: Requires custom GCS plugin — no virtual COM port like Classic SPP.
  Plugin already needed for RC Override, adding BLE transport is incremental work.

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

## Build & CI/CD

### Debug Toolchain Paths

```
# ESP32 (MiniKit) - xtensa-esp32
TOOLCHAIN: C:/Users/vladn/.platformio/packages/toolchain-xtensa-esp-elf/bin/
addr2line: xtensa-esp32-elf-addr2line.exe -pfiaC -e .pio/build/minikit_debug/firmware.elf <addresses>
size:      xtensa-esp32-elf-size.exe .pio/build/minikit_production/firmware.elf
nm:        xtensa-esp32-elf-nm.exe .pio/build/minikit_production/firmware.elf

# ESP32-S3 (Zero, SuperMini, XIAO) - xtensa-esp32s3
addr2line: xtensa-esp32s3-elf-addr2line.exe -pfiaC -e .pio/build/zero_debug/firmware.elf <addresses>
size:      xtensa-esp32s3-elf-size.exe .pio/build/zero_production/firmware.elf
```

### GitHub Actions Improvements
- [x] **Add MiniKit to GitHub Actions auto-build**
  - Added `minikit_production` to build workflow
  - Firmware files: `firmware-minikit.bin`, `firmware-minikit.elf`

- [x] **Exclude TODO.md from source archive**
  - Added to `.gitattributes`: `TODO.md export-ignore`

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


