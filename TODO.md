# TODO / Roadmap

## ACTIVE TASKS 🔄

### PLATFORM SUPPORT 🟠

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
  - [ ] BLE: testing (build `supermini_ble_*` added, hardware identical to Zero)

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
  - [ ] BLE: testing (build `xiao_ble_*` added, hardware identical to Zero)

**Status**: XIAO ESP32-S3 support fully implemented and basic configuration tested (Device1 UART Bridge + Device2 USB working with flight controller and Mission Planner). Pin mapping verified: all GPIO pins are available on castellated edge holes. Web interface correctly displays D-pin names (D3/D4 for Device1, D8/D9 for Device2, D6/D7 for Device3). LED blink-only mode working. Need extended testing for Device3, RTS/CTS, network modes, and protocols.

**Note**: XIAO ESP32-S3 is even more compact than Super Mini. Has external antenna connector which is beneficial for network operations like SBUS over UDP/WiFi, improving range and reliability. This is important as boards like Zero with PCB antennas can have unstable ping times causing SBUS packet loss when timing requirements (14ms frame rate) are not met. External antenna should provide more stable connection for time-critical protocols.

### FUTURE PROTOCOLS & FEATURES 🔵

#### Web Interface Improvements

- [x] ~~**RC Channel Monitor**~~ — DONE v2.18.15
  - Future: MAVLink RC_CHANNELS (msg 65) as additional data source (1-2 Hz from FC)

- [ ] **Show connected clients info in AP mode**
  - Number of connected stations
  - List of client IP addresses
  - IP of current web interface user

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

  - [ ] Configurable send rate for binary SBUS output (D2_SBUS_OUT, D3_SBUS_OUT, D4_SBUS_UDP_TX binary)
    - Currently binary SBUS outputs at native 70 Hz (14ms frame timing)
    - Some FC or receivers may benefit from throttled rate (50 Hz typical)
    - D4 binary (format=0) also needs rate control — currently only text format (format=1) has it
    - Reuse existing rate selector UI (10-70 Hz)

### Future Considerations (Low Priority)

#### BLE Remaining Tasks

  - [ ] **LOW**: Check encryption before sending notifications (`desc.sec_state.encrypted`)

  **Reference**: BLE GATT SBUS (NUS + Just Works pairing + bond persistence + WiFi coexistence) — implemented.
  Heap: S3 ~60KB free (min ~38KB), MiniKit ~42KB free (min ~16KB). WiFi + BLE + UDP + WebServer work simultaneously.

#### MP Plugin Improvements

  - [ ] **MEDIUM**: MAVLink telemetry via BLE NUS (MP plugin)

    **Use case**: ESP connected to drone via UART (MAVLink), sends telemetry via BLE to laptop.
    Laptop keeps WiFi for internet (maps, remote access), BLE works in parallel.
    Alternative: DroneBridge and similar use external BLE→vCOM bridge — plugin is cleaner.
    ```
    [Drone] →UART→ [ESP32-S3 BLE] →BLE NUS→ [Laptop: MP plugin]
                                               ↕ WiFi (internet, maps)
    ```

    **Approach**: WinRT `ICommsSerial` in plugin, integration into standard MP dropdown

    **MP API (verified, public, accessible from plugin):**
    - `MissionPlanner.Comms.SerialPort.GetCustomPorts` — `public static Func<List<string>>`
      - Plugin adds BLE devices to port list (appear as "BLE_ESP-Bridge_XXXX")
    - `Host.MainForm.CustomPortList` — `public Dictionary<Regex, Func<string, string, ICommsSerial>>`
      - Plugin registers factory: regex `BLE_.*` → creates `BleNusSerial`
    - User selects BLE port in standard dropdown, clicks Connect — MP works as usual

    **Implementation:**
    - [ ] Class `BleNusSerial : Stream, ICommsSerial` (~200-300 lines)
      - WinRT BLE API (Windows.Devices.Bluetooth) — no external DLLs
      - MemoryStream buffer for incoming data (notification callback)
      - Read() with timeout (deadline pattern, like CommsBLE/UdpSerial)
      - Write() via NUS RX characteristic
      - Open()/Close() — BLE connect/disconnect
      - Thread-safe buffer (lock, notification callback vs MP read thread)
    - [ ] Registration in Init(): GetCustomPorts += GetBleNusPorts, CustomPortList.Add(...)
    - [ ] Device discovery: reuse GetPairedNusDevicesAsync() from current plugin
    - [ ] Handle disconnect (IsOpen=false, exception from Read)

    **Throughput:** BLE NUS MTU 256 → ~15-20 KB/s. MAVLink 57600 baud ≈ 5.7 KB/s — 3x headroom.

    **ESP side:** no changes needed — BLE NUS already implemented, MAVLink data transparent for NUS.

    **Reference**: MP has experimental CommsBLE (since 2020, SimpleBLE, not in releases, BUSL-1.1 license).
    Our approach — WinRT, no license restrictions, no external DLLs.
    MP API: MainV2.cs:522 (CustomPortList), CommsSerialPort.cs:312 (GetCustomPorts).

#### UART Improvements 🔵 LOW PRIORITY

- [ ] **Hardware Flow Control CTS/RTS detection**
  - Currently: if flow control enabled in config but CTS/RTS not physically connected, TX blocks waiting for CTS signal
  - Other devices often auto-detect and fall back to no flow control
  - Idea: check CTS pin state before enabling hardware flow control
    - Configure CTS as input with pull-up
    - If CTS stays HIGH (pull-up wins) → likely not connected → disable flow control
    - If CTS is LOW → assume device connected and holding CTS
  - Not 100% reliable (CTS LOW could mean "not connected but grounded" or "connected, wait")
  - Low priority: UI checkbox now works correctly, users can disable manually

#### Memory Optimization (MiniKit) 🔵 LOW PRIORITY

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

  **Heap reference (MiniKit v2.18.12):**
  - WiFi+BT+WebServer: ~35KB free (BT stack takes ~40KB static)
  - BT only (no WiFi): ~70KB free
  - WiFi only (no BT in sdkconfig): ~76KB free

#### New Protocol Support 🔵 LOW PRIORITY

- [~] **CRSF Protocol** - RC channels and telemetry for ELRS/Crossfire systems (Phase 1+1.5+2+3 done)

  **Primary use case**: ELRS RX → UART → ESP → WiFi/USB → Raspberry Pi (binary RC data)
  **Secondary**: text output for debugging/visualization and GCS plugin compatibility

  **Connection notes:**
  - UART between ELRS RX and ESP is full-duplex (separate TX/RX wires)
  - For RX-only (RC channels from ELRS RX): single wire sufficient, no need to send anything back
  - ELRS RX sends RC frames at constant rate regardless of whether it receives telemetry
  - No ACK mechanism in CRSF — all data is fire-and-forget
  - Telemetry on the radio screen is lost without reverse channel, but irrelevant if GCS provides it via MAVLink
  - Each CRSF frame has exactly one type — no mixed-type frames, safe for line-based filtering

  **Architecture — configurable filters instead of separate roles:**

  One CRSF input role per device (e.g. `D1_CRSF_IN`) — no need for separate roles per frame type.
  Bitmask filter selects which frame groups to forward/convert:

  | Bit | Group | Frame types | Description |
  |-----|-------|-------------|-------------|
  | 0 | RC Channels | 0x16 | 16ch x 11bit, 26 bytes (27 with ELRS 4.0+ armStatus) |
  | 1 | Link Stats | 0x14 | RSSI, SNR, LQ, TX power |
  | 2 | Flight Telemetry | 0x08,0x09,0x1E,0x21 | Battery, baro, attitude, flight mode |
  | 3 | Sensors | 0x02,0x07,0x0A,0x0C,0x0D,0x0E | GPS, vario, airspeed, RPM, temp, cells |
  | 4 | VTX/MSP | 0x32,0x7A-0x7C | VTX control, MSP passthrough |
  | 5 | Config | 0x28-0x2E | Device ping/info, param read/write, ELRS status |
  | 6 | Reserved | | |
  | 7 | Passthrough | | Forward all frames raw (overrides other bits) |

  Filter stored as `uint8_t crsfFilter` in config.
  Multiple filters can be active simultaneously (e.g., RC + Link Stats + Battery).

  **Text output format** (for debugging and GCS plugin compatibility):
  Each frame type uses a unique line prefix. Consumers filter by prefix, ignore unknown lines.
  Compatible with existing SBUS text plugins (RC Override ignores non-`RC` lines).
  ```
  RC 1500,1500,1000,1500,1500,1500,1500,1500,992,992,992,992,172,172,172,172\r\n
  LQ 99,-45,8,250\r\n
  BAT 16.2,12.5,1450,85\r\n
  ATT 15.2,-3.1,180.0\r\n
  GPS 49.123456,16.654321,250,12.5,180\r\n
  FM ACRO\r\n
  ```
  Multiple filter groups active = interleaved lines of different types in one stream.

  Phase 1 — text output (all frame types, no filters): ✅ DONE
  - [x] `D1_CRSF_IN = 2` in Device1Role enum. UART1 at 420000 baud, 8N1, non-inverted. GPIO 4 RX
  - [x] `D2_USB_CRSF_TEXT = 7` in Device2Role enum. Text CRSF output via USB
  - [x] `CrsfParser` — frame parser (addr 0xC8, len, type, payload, CRC8 validation). Own code, no external libs
  - [x] Text output for ALL parsed frame types (RC, LQ, BAT, GPS, ATT, FM, ALT)
  - [x] Output rate limiter via `outRate` field (renamed from sbusRate, shared SBUS/CRSF)
  - [x] Web UI: Device 1/2 dropdowns, role constraints (three-mode architecture: UART/SBUS/CRSF)
  - [x] diagnostics.cpp role names, config.cpp load/save, web_api.cpp
  - [x] CRSF statistics in web UI (validFrames, invalidFrames, crcErrors, lastActivity)
  - [x] Protocol optimization auto-detect (CRSF > SBUS > None)
  - [x] Alpine.js UI unification (all device selects use x-for with computed arrays)
  Phase 1.5 — additional text outputs: ✅ DONE
  - [x] UDP CRSF Text output (`D4_CRSF_TEXT = 5`) — sendDirect generic path in UdpSender
  - [x] BLE CRSF Text output (`D5_BT_CRSF_TEXT = 3`) — BLE_ENABLED only
  - [x] Per-output independent rate limiting (CrsfOutput struct, RC only, telemetry unrestricted)
  - [x] Web UI: device4/5 options, outRate selectors, role validation
  - [x] Network Logs selector restricted to D4_LOG_NETWORK role only

  Phase 2 — binary output: ✅ DONE
  - [x] Raw CRSF forward (binary frames via USB/UART3, no text conversion)
  - [x] `D2_USB_CRSF_BRIDGE = 8` — binary CRSF via USB (TX only, no rate limiting)
  - [x] `D3_CRSF_BRIDGE = 6` — binary CRSF via UART3 at 420000 baud (TX only)
  - [x] sendRawToOutputs() — raw frame forwarding before buffer consume (zero-copy)
  - [x] processSenders skip for binary outputs (sendDirect path, no queue)

  Phase 3 — Bidirectional communication: ✅ DONE
  - [x] Device 1 CRSF_IN: TX pin enabled (bidirectional UART)
  - [x] Device 3 CRSF_BRIDGE: RX pin enabled (bidirectional UART3)
  - [x] Input buffer allocation for D2_USB_CRSF_BRIDGE and D3_CRSF_BRIDGE
  - [x] Reverse input flows: USB/UART3 → RawParser → Uart1Sender → ELRS RX

  Phase 4 — filters and telemetry extraction:
  - [ ] Filter bitmask UI (checkboxes in web interface to select frame types)
  - [ ] Telemetry extraction: battery, GPS, attitude (structured data, not just text)

  Phase 5 — VTX control (requires both TX+RX wires, not single-wire):
  - [ ] VTX control via MSP-over-CRSF (band/channel/power)

  **Protocol reference:**
  - UART: 420000 baud (ELRS) / 416666 (Crossfire), **8N1, non-inverted**. Hardcode 420000 for Phase 1, configurable later
  - Frame: `[Addr][Length][Type][Payload...][CRC8]`, max 64 bytes total
  - Addr = destination: 0xC8 (flight controller), 0xEE (CRSF RX). RX→FC traffic uses 0xC8
  - RC Channels (type 0x16): 16ch x 11bit = 22 byte payload, total 26 bytes
  - Channel range: 0-2047 (11-bit). Typical: 172=988us, 992=1500us, 1811=2012us
  - ELRS 4.0+: optional armStatus byte (len=0x19, total 27 bytes)
  - CRC8: polynomial 0xD5 (DVB-S2), covers Type + Payload
  - RC frames sent at constant rate (50-1000Hz depending on ELRS config)
  - Full-duplex on RX-to-FC segment, half-duplex inside RC TX (not relevant)
  - Failsafe: absence of frames = failsafe (no flag bits like SBUS)

  **Memory footprint:** ~340 bytes RAM (parser state + 64-byte frame buffer + CRC table).
  No new heap allocations — reuses existing pipeline senders (UDP, USB, UART, BLE).
  Text formatting uses stack buffers (~120 bytes, temporary). Fits all builds including BLE.

  **Test bench:** Radio TX (internal ELRS module) → ELRS RX → ESP32 Zero (Device 1).
  Wiring: GND, VCC (5V or 3.3V depending on RX), data TX→RX (+ optional RX→TX for Phase 3).
  3.3V logic, non-inverted — no level shifter or inverter needed (unlike SBUS).

  **Key differences from SBUS:**
  - Non-inverted (standard UART, no hardware hacks)
  - Variable frame length (CRC required for validation)
  - Bidirectional telemetry (battery, GPS, RSSI, LQ) — optional for our use case
  - Baudrate 420000 vs 100000
  - Multiple frame types in one stream (vs SBUS = only RC channels)

  **Reference**: [ExpressLRS crsf_protocol.h](https://github.com/ExpressLRS/ExpressLRS/blob/master/src/lib/CrsfProtocol/crsf_protocol.h), [CRSF wiki](https://github.com/crsf-wg/crsf/wiki), [TBS spec](https://github.com/tbs-fpv/tbs-crsf-spec), Betaflight `crsf.c`
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

## Build & CI/CD

### Debug Toolchain Paths

```
# ESP32 (MiniKit) - xtensa-esp32
TOOLCHAIN: ~/.platformio/packages/toolchain-xtensa-esp-elf/bin/
addr2line: xtensa-esp32-elf-addr2line.exe -pfiaC -e .pio/build/minikit_debug/firmware.elf <addresses>
size:      xtensa-esp32-elf-size.exe .pio/build/minikit_production/firmware.elf
nm:        xtensa-esp32-elf-nm.exe .pio/build/minikit_production/firmware.elf

# ESP32-S3 (Zero, SuperMini, XIAO) - xtensa-esp32s3
addr2line: xtensa-esp32s3-elf-addr2line.exe -pfiaC -e .pio/build/zero_debug/firmware.elf <addresses>
size:      xtensa-esp32s3-elf-size.exe .pio/build/zero_production/firmware.elf
```

## Libraries and Dependencies

### Current Dependencies
```ini
lib_deps =
    bblanchon/ArduinoJson@^7.4.2      # JSON parsing and generation
    fastled/FastLED@^3.10.3           # WS2812 LED control
    arkhipenko/TaskScheduler@^4.0.3   # Task scheduling
    ESP32Async/ESPAsyncWebServer@3.8.1 # Async web server (3.9+ has crashes, frozen)
    ESP32Async/AsyncTCP@3.4.8          # TCP support (frozen with webserver)

# BLE builds use ESP-IDF NimBLE component (configured in sdkconfig), no external lib
```


