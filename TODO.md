# TODO / Roadmap

## ACTIVE TASKS 🔄

### Build & Release 🔴 HIGH PRIORITY

- [x] **GitHub Release: добавить все бинарники для полной прошивки**
  - Добавлены `bootloader.bin` и `partitions.bin` для каждой сборки
  - Добавлены BLE сборки: zero-ble, supermini-ble, xiao-ble, minikit-ble
  - Workflow обновлён: цикл по всем env вместо ручного копирования

### Before Final Cleanup 🟠

- [x] Проверить `vTaskDelay(2000)` TEST hack в main.cpp — убран
- [x] Code cleanup: убрать диагностические forceSerialLog (ble_dump_bonds и т.д.)
- [x] Убрать `BLE_WIFI_COEXIST_TEST` — коэксистенция подтверждена, mutual exclusion удалён
- [ ] После cleanup: проверить UART функциональность (регрессия)

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
  - [ ] BLE: тестирование (сборка `supermini_ble_*` добавлена, аппаратно идентичен Zero)

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
  - [ ] BLE: тестирование (сборка `xiao_ble_*` добавлена, аппаратно идентичен Zero)

**Status**: XIAO ESP32-S3 support fully implemented and basic configuration tested (Device1 UART Bridge + Device2 USB working with flight controller and Mission Planner). Pin mapping verified: all GPIO pins are available on castellated edge holes. Web interface correctly displays D-pin names (D3/D4 for Device1, D8/D9 for Device2, D6/D7 for Device3). LED blink-only mode working. Need extended testing for Device3, RTS/CTS, network modes, and protocols.

**Note**: XIAO ESP32-S3 is even more compact than Super Mini. Has external antenna connector which is beneficial for network operations like SBUS over UDP/WiFi, improving range and reliability. This is important as boards like Zero with PCB antennas can have unstable ping times causing SBUS packet loss when timing requirements (14ms frame rate) are not met. External antenna should provide more stable connection for time-critical protocols.

#### ESP32 MiniKit Support ✅ COMPLETED

**Status**: ESP32 MiniKit (WROOM-32) fully implemented and tested. Features: WiFi, web interface, quick reset detection (3 resets = network mode), Bluetooth SPP bridge, SBUS support. Device 2 = USB via external chip (CP2104 tested). All protocols verified (MAVLink, SBUS binary/text).

**Note**: CH9102 may have DTR/RTS issues — not yet verified. WROOM-32 has limited RAM (~160KB heap) compared to S3 variants.

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

  **Heap reference (MiniKit v2.18.12):**
  - WiFi+BT+WebServer: ~35KB free (BT stack takes ~40KB static)
  - BT only (no WiFi): ~70KB free
  - WiFi only (no BT in sdkconfig): ~76KB free

### FUTURE PROTOCOLS & FEATURES 🔵

#### UART Improvements

- [ ] **Hardware Flow Control CTS/RTS detection**
  - Currently: if flow control enabled in config but CTS/RTS not physically connected, TX blocks waiting for CTS signal
  - Other devices often auto-detect and fall back to no flow control
  - Idea: check CTS pin state before enabling hardware flow control
    - Configure CTS as input with pull-up
    - If CTS stays HIGH (pull-up wins) → likely not connected → disable flow control
    - If CTS is LOW → assume device connected and holding CTS
  - Not 100% reliable (CTS LOW could mean "not connected but grounded" or "connected, wait")
  - Low priority: UI checkbox now works correctly, users can disable manually

#### Web Interface Improvements

- [ ] **Show connected clients info in AP mode**
  - Number of connected stations
  - List of client IP addresses
  - IP of current web interface user

- [ ] **Captive Portal redirect to mDNS** 🟡 MEDIUM PRIORITY
  - Сейчас: Windows открывает msftconnecttest.com, редирект на `/`
  - Проблема: URL в браузере остаётся msftconnecttest.com
  - Решение: редирект на `http://{mdns_hostname}.local/`
  - Файл: web_interface.cpp, handleNotFound()
  - Нюанс: mDNS может не работать на старых устройствах, fallback на IP

- [ ] **Browser Time Sync for Crashlog** 🟡 MEDIUM PRIORITY
  - Get current time from browser when web UI is opened
  - Store synced timestamp + millis() reference point
  - Implement getCurrentTime() to calculate current time based on millis() delta
  - Record real timestamp in crashlog (or 0 if browser was never opened)
  - Useful for debugging crashes with actual date/time instead of just uptime
  - No RTC required — time is "good enough" after first browser connection

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

  - [ ] **LOW**: Проверка шифрования перед отправкой нотификаций (`desc.sec_state.encrypted`)
  - [ ] **LOW**: Сохранение последнего BLE устройства в настройках MP плагина

  **Reference**: BLE GATT SBUS (NUS + PIN pairing + bond persistence + WiFi coexistence) — реализовано.
  Heap: S3 ~60KB free (min ~38KB), MiniKit ~42KB free (min ~16KB). WiFi + BLE + UDP + WebServer работают одновременно.

#### MP Plugin Improvements

  - [ ] **LED индикация: отдельный цвет для "дрон не готов"**
    - Сейчас: при отсутствии телеметрии LED мигает разными цветами, причина не ясна
    - Проблема: UpdateLed() отражает только связь с ESP, не учитывает IsVehicleReady()
    - Нужно: добавить проверку IsVehicleReady() в UpdateLed(), отдельный цвет для "ESP ok, vehicle not ready"
    - Текущие состояния: DarkGray (off), Red blink (connecting), LimeGreen (all ok), Orange (no data)
    - Новое состояние: ESP data flowing + vehicle not ready → отдельный цвет (не Orange, не Green)
    - Цвет: TBD (Yellow слабо виден, Orange уже занят, нужен контрастный — возможно DodgerBlue)
    - Файл: `plugins/RcOverride_v2_BLE.cs`, метод `UpdateLed()`

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

## Libraries and Dependencies

### Current Dependencies
```ini
lib_deps =
    bblanchon/ArduinoJson@^7.4.2      # JSON parsing and generation
    fastled/FastLED@^3.10.3           # WS2812 LED control
    arkhipenko/TaskScheduler@^4.0.3   # Task scheduling
    ESP32Async/ESPAsyncWebServer@^3.9.1  # Async web server
    ESP32Async/AsyncTCP@^3.4.9        # TCP support for async server

# BLE builds only (zero_ble_*, supermini_ble_*, xiao_ble_*):
    h2zero/NimBLE-Arduino@^2.3.2      # BLE stack for Nordic UART Service
```


