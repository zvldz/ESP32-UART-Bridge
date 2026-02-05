# CHANGELOG

## v2.18.14

### LED Indication
- **BLE + WiFi combined modes**: Visual feedback for all connection states
  - RGB LED (Zero, SuperMini): Color fade animation for combined modes
    - WiFi AP + BLE: Blue↔Purple fade (~2s cycle)
    - WiFi Client + BLE: Green↔Purple fade (~2s cycle)
    - BLE only: Static purple
  - Single-color LED (XIAO, MiniKit): Simplified "solid = working, blink = problem"
    - Stable states (AP, Client, AP+BLE, Client+BLE): Solid ON
    - BLE only (no WiFi): Fast blink (150ms)
    - WiFi transient states always visible (searching/error blinks)
- **New LED modes**: `LED_MODE_BLE_ONLY`, `LED_MODE_WIFI_AP_BLE`, `LED_MODE_WIFI_CLIENT_BLE`
- **Coordination API**: `led_set_wifi_mode()` and `led_set_ble_active()` for combined state management
- **Code cleanup**: Removed unused blink patterns (`BLINK_WIFI_CLIENT`, `BLINK_DOUBLE`, `BLINK_TRIPLE`) and timing constants

### Web Interface
- **Device 5 (BLE) statistics**: TX/RX byte counters in Device Statistics panel
  - Shows only when Device 5 role is active (SBUS_OUT_TEXT or SBUS_OUT_BIN)
  - Tracks both sendDirect (fast path) and queue-based transmissions
- **SBUS Fast Path fix**: Last Activity and Success Rate now work correctly
  - Added `lastFrameTime` tracking to `SbusFastParser` (was always showing "Never")
- **Unified device naming**: AP SSID now uses mDNS hostname (one name everywhere)
  - "Device Name" field (max 32 chars) sets: mDNS hostname, AP SSID, BLE device name
  - AP SSID field is read-only, auto-filled from Device Name
  - Simplifies configuration: one name = one device identity

### BLE Support
- **NimBLE NUS**: BLE Nordic UART Service for all boards (`BLE_ENABLED` flag)
  - Build environments: `zero_ble_*`, `supermini_ble_*`, `xiao_ble_*`, `minikit_ble_*`
  - GATT server with NUS TX (notify) and RX (write) characteristics
  - WiFi + BLE coexistence confirmed on ESP32-S3 (heap ~36KB min) and ESP32 MiniKit (heap ~16KB min)
- **Security**: Just Works pairing with bonding (no PIN prompt)
  - GATT characteristics require encryption + authentication
  - Bond persistence in NVS (up to 10 bonds), repeat pairing auto-handled
  - Changed from PIN pairing due to Android compatibility issues with static passkey
- **MiniKit BLE**: NimBLE ported to ESP32-WROOM-32 (BLE-only controller mode)
  - Separate sdkconfig with LWIP/WiFi memory optimization for low-RAM environment
- **MP Plugin (RcOverride_v2_BLE)**: BLE device scan/select/connect via WinRT API (compiled as DLL)
  - Device list filtered by NUS service (only paired ESP-Bridge devices shown)
  - LED tooltips: hover shows status (Disabled, Connecting, Connected data OK, no telemetry, etc.)
  - BLE auth failure detection: stops retries on pairing rejection (DarkRed LED)
  - BLE device list auto-refresh on first switch to BLE mode
  - General failure counter (5 consecutive failures → stop retries)
  - Proper BLE cleanup (service dispose, notification unsubscribe)
  - **Settings persistence**: Source type (COM/BLE/UDP), port, device ID saved to MP config.xml
  - **Auto-scan on load**: BLE devices scanned automatically if BLE mode was saved
  - **espFix**: `Handshake.RequestToSend` during port Open() prevents ESP32-S3 USB Serial/JTAG reset
    - Root cause: `ARDUINO_USB_MODE=1` (HWCDC) has hardware auto-reset on RTS transition
    - Same approach as MissionPlanner's `CHK_rtsresetesp32` setting
    - Applied to both v2 (COM/UDP) and v2_BLE plugins
- **MP Plugins crash fix**: Image disposal (`ArgumentException in Image.get_RawFormat`)
  - VideoTX_Perun: clone MP resources instead of direct use, dispose in Exit()
  - FuseSwitch_Perun: dispose icon in Exit()

### Code Cleanup
- **BLE+WiFi coexistence**: Always enabled, mutual exclusion removed. Fallback skip commented in `device_init.cpp`
- **BLE diagnostics**: Replaced `forceSerialLog` with `log_msg` at appropriate levels
- **Removed**: `ble_dump_bonds()` debug function, `BLE_WIFI_COEXIST_TEST` flag
- **Heap monitor**: Disabled by default, kept under `#ifdef DEBUG` for quick re-enable
- **Build flags**: `-D DEBUG` commented out in all builds (uncomment for forceSerialLog)

### Build & Release
- **GitHub Actions**: Release includes `bootloader.bin`, `partitions.bin`, `firmware.bin` + `.elf` for each board
- **BLE builds added**: `zero-ble`, `supermini-ble`, `xiao-ble`, `minikit-ble` (9 builds total)
- **New sdkconfig files**: `supermini_ble_*`, `xiao_ble_*` (production + debug)

### WiFi
- **WiFi AP Mode: Temporary mode** (new default)
  - New `WifiApMode` enum: `DISABLED`, `TEMPORARY`, `ALWAYS_ON`
  - `TEMPORARY`: WiFi starts at boot, auto-disables after 5 min with no clients
  - Timer cancelled when client connects, restarted when last client disconnects
  - Config migration from old `permanent` boolean field
  - Prevents lockout when ESP inside TX module bay (buttons inaccessible)

### Fixes
- **WiFi Client mode stability**:
  - Race condition fix: don't change state to `CLIENT_NO_SSID` if already connected
  - `wifiStopping` flag prevents event handler work during WiFi stop (crash fix)
  - AP client connect/disconnect event handling improved
- **WiFi event handler crash fix**: Replaced blocking `vTaskDelay()` with `esp_timer` for reconnect retry
  - Root cause: `vTaskDelay` inside event handler blocks `sys_evt` task with mutex held → assert failure
  - Crash manifested as `IllegalInstruction` at 0x00000000 when WiFi Client mode without AP available
- **Crashlog hardening**: RTC variable validation, ASCII string checks, JSON re-serialization
- **embed_html.py**: Removed cssmin dependency, simplified to gzip-only compression
- **Boot optimization**: Skip 500ms delay on normal boot (no click detected)
- **Memory diagnostics**: New format `HEAP: free/total (min=X) | PSRAM: free/total`

---

## v2.18.13

### MiniKit Build Split
- **BT/no-BT variants**: MiniKit now has separate builds
  - `minikit_bt_*` — with Classic Bluetooth SPP
  - `minikit_*` — without Bluetooth (more free RAM for WiFi)
- **New flag**: `MINIKIT_BT_ENABLED` for conditional BT compilation

### Build System
- **sdkconfig reorganization**: Moved to `sdkconfig/` directory for cleaner structure

### Web UI Fixes
- **Flow Control checkbox**: Fixed auto-enabling bug (string '0' was truthy)
- **Type consistency**: All select/number fields now use strings for Alpine.js x-model compatibility
- **MAVLink Routing checkbox**: Fixed visibility when switching to MAVLink protocol
- **Dirty checking**: Fixed false positives from type mismatches (string vs number)
- **Crash History**: Fixed "Loading..." text centering

---

## v2.18.12

### Web UI
- **Alpine.js refactoring**: Complete rewrite using Alpine.js framework
  - Reactive stores for app config, status, SBUS, crash history
  - Cleaner code: 97 getElementById calls → ~15
  - Split API: `/api/config` (static) + `/api/status` (polled)
  - JSON body for POST requests (was form-urlencoded)

### SBUS Conversion
- **OpenTX/EdgeTX compatible formula**: SBUS→µs conversion now matches radio display
  - 172 → 988µs, 992 → 1500µs, 1811 → 2012µs (was 1000-2000 Betaflight-style)

### MiniKit LED
- **BT connection indicator**: LED blinks when Bluetooth SPP client connected
  - Distinguishes from WiFi mode (solid LED)

---

## v2.18.12-prev

### New Features
- **SBUS Output Format Selector**: Dropdown with Binary/Text/MAVLink options
- **MAVLink RC_CHANNELS_OVERRIDE**: SBUS→MAVLink conversion for wireless RC control
- **UDP Send Rate**: Configurable 10-70 Hz for SBUS UDP Output
- **UDP Source Timeout**: Configurable 100-5000 ms for SBUS Input role
- **BT Send Rate**: Configurable 10-70 Hz for Bluetooth SBUS Text output

### MiniKit Bluetooth
- **Bluetooth SPP Bridge**: New Device 5 with two roles
  - BT Bridge — MAVLink/Raw telemetry output
  - BT SBUS Text — text format for RC Override plugin
  - SSP "Just Works" pairing (no PIN prompt)

### MiniKit Memory Optimization
- **DMA buffers reduced**: 2x smaller for MiniKit (16KB vs 32KB on S3)
- **Auto Device1 SBUS_IN**: When any device has SBUS role, Device 1 auto-switches to SBUS_IN
  - Saves ~14KB RAM (minimal DMA buffers instead of full UART bridge)
  - UART bridge is unused in SBUS mode anyway
- **SBUS→MAVLink conditional**: Disabled by default to save memory (uncomment `-D SBUS_MAVLINK_SUPPORT` in platformio.ini to enable)

### MiniKit Fixes
- **Fix spurious WiFi reset**: Disabled floating GPIO0 button handling
- **Quick reset always forces AP**: Triple RESET guarantees Web UI access
- **Fix Device 3 role validation**: SBUS_IN role was blocked by incorrect validation
- **Fix BT SBUS Text output**: Buffer size was too small (64 < 101 bytes needed)
- **Fix SBUS validation**: Added D3_SBUS_IN and D5_BT_SBUS_TEXT to validation
- **Fix SBUS + WiFi crash**: Reduced DMA buffers for all SBUS devices (512/0/1024 vs 4096/4096/8192)
  - Root cause: Large buffers caused heap fragmentation, WiFi tcp_accept failed

### Code Quality
- **SBUS conversion buffer refactored**: Lazy allocation in SbusRouter singleton
  - Memory saved when SBUS output conversion not used (~101 bytes)
  - Removed global g_sbusConvertBuffer variable

### Web UI
- **Device 4/5 configuration**: Renamed roles, MiniKit-specific options
- **Modular JS architecture**: Reduced code duplication
- **BT Send Rate slider**: Rate control for SBUS Text output (Device 5)
- **Auto Device1 role**: Device 1 automatically set to SBUS_IN when SBUS role selected on other devices
- **SBUS Output roles refactored**: Split into "SBUS Output" (binary) and "SBUS Text Output" for Device 3/4
  - Format selector dropdown replaced with composite role values
  - Rate selector shown only for Text output modes
- **Device 5 roles renamed**: "BT Bridge" → "Bridge", "BT SBUS Text" → "SBUS Text Output"
- **UART roles disabled in SBUS mode**: UART2/UART3 Bridge options disabled when Device 1 = SBUS Input

### Mission Planner Plugin
- **RC Override Plugin v2.0.1** (`plugins/rcoverride_v2.cs`):
  - COM and UDP sources
  - Port open retry with 2s interval (for BT SPP connection delay)
  - Lock port/source selection when connected
  - Dynamic port list refresh every 3s
  - Console diagnostics for debugging
  - Fix RC checkbox behavior: LED turns gray and port closes when unchecked
  - Fix controls unlock: COM port and Source dropdowns re-enabled when RC disabled

## v2.18.11

### New Features
- **WiFi AP Channel**: Configurable WiFi channel (1-11) for AP mode via Web UI
- **SBUS Text Format Output**: Optional text format `RC 1500,1500,...\r\n` for SBUS output roles
  - Checkbox "Text format" in Web UI for Device 2/3/4 SBUS outputs
  - Use case: Mission Planner RC Override plugin compatibility, debugging
- **Device 3 SBUS Input**: New D3_SBUS_IN role for SBUS input on Device 3 UART pins
  - Needed for MiniKit where Device 2 = USB only

### Fixes
- **MiniKit USB Packet Loss**: Fixed buffer initialization order — `setTxBufferSize()` must be called BEFORE `Serial.begin()` on UART
  - Root cause: Default UART TX buffer = 0 (only 128-byte hardware FIFO)
  - Result: ~13% packet loss during FTP bursts eliminated
- **MiniKit USB Block Detection**: Disabled for UART (availableForWrite returns constant, causing false positives)
- **SSID Generation**: Fixed logic - empty SSID triggers auto-generation, user-set "ESP-Bridge" is now respected
- **AP SSID Safety Check**: Prevents AP SSID from matching client network names

### Documentation
- **MiniKit CP2104**: All documentation updated to specify CP2104 chip (CH340/CH9102 not tested)
- **README**: Full MiniKit board documentation added (connections, quick start, LED, button functions)
- **Help page**: MiniKit pinout table and requirements added

### Web UI
- **SBUS Config Warning**: Shows warning when SBUS Output configured without SBUS Input source

### ESP32 MiniKit (WROOM-32) Support
- **New board support**: ESP32 MiniKit (ESP32-WROOM-32 based development board)
  - PlatformIO environments: `minikit_production`, `minikit_debug`
  - GPIO mapping: RTS/CTS on GPIO18/19, Device 3 UART on GPIO16/17, LED on GPIO2
  - No USB Host support (WROOM lacks USB OTG peripheral)
- **Quick Reset Detection**: Triple RESET for WiFi activation (replaces BOOT button)
  - NVS-based detection (RTC cleared on RESET)
  - Boot loop protection (crashes clear counter)
- **Memory Optimizations** for ~160KB usable heap (no PSRAM):
  - Reduced WiFi buffers in sdkconfig
  - Reduced LWIP TCP buffers (IPv6 disabled)
  - LOG_BUFFER_SIZE: 30 (vs 100 on S3)
  - UART1_TX_RING_SIZE: 4KB (vs 8KB on S3)
  - UDP log buffer: dynamic allocation only when needed
- **Single-color LED support**: Active HIGH logic for GPIO2
- **Web UI**: MiniKit board detection, Device 2 options filtered
- **Help page**: MiniKit pinout and documentation added

### Web UI
- **Utils refactoring**: JSON parse error handling moved to utils.js
- **Form utils**: Improved radio button and select initialization

## v2.18.10

### New Features
- **UDP Auto Broadcast**: Automatic broadcast IP detection from DHCP subnet (Client mode only)
- **Multiple UDP Targets**: Support up to 4 comma-separated target IPs

### Web UI
- Cleanup and fixes
- Unified checkbox styling with CSS class

### Fixes
- **mDNS Task Priority**: Increased from 1 to 5 for better reliability

### Code Quality
- EditorConfig added
- Release changelog extraction fix
- Code comments cleanup

## v2.18.9 (WiFi & Configuration Improvements)

### WiFi Enhancements
- **Multi-WiFi Networks Support**: Configure up to 5 WiFi networks with priority order
  - Automatic fallback to next network when connection fails
  - Priority-based scanning (tries networks in configured order)
  - Web UI with dynamic network list management (Add/Remove buttons)
  - Backward compatible: existing single-network configs auto-migrate to new format
  - Config version upgraded to v10 with automatic migration
- **Unique WiFi AP Name**: Auto-generated AP SSID with MAC suffix (e.g., `ESP-Bridge-11fc`)
  - Prevents AP name conflicts in multi-device environments
  - Generated on first boot, saved to config
- **Configurable mDNS Hostname**: User can set custom `.local` address
  - Works in both AP and Client modes
  - Auto-generated on first boot (e.g., `esp-bridge-11fc`)
  - Web UI field with validation (a-z, 0-9, hyphen only)
- **mDNS in AP Mode**: Added mDNS support for Access Point mode (was Client-only)
- **Connected Network Display**: System status now shows actual connected network name in Client mode

### Web UI Improvements
- **Auto-Reconnect after Save & Reboot**: Page automatically reconnects after device reboot
  - 8-second countdown during reboot
  - Up to 30 reconnection attempts with progress indicator
  - Automatic page reload when device comes back online
- **Button Alignment**: Centered Configuration Backup and Firmware Update buttons
- **Consistent Hint Styling**: Standardized hint text formatting across all sections

### Configuration Management
- **Factory Reset**: New web UI button to reset all settings to defaults
- **WiFi Reset via BOOT Button**: 5-second hold resets WiFi settings only (not full config)
  - Resets: mode, SSID, password, client credentials, mDNS hostname, TX power
  - Preserves: UART settings, device roles, protocol settings
- **Config Export Filename**: Now uses mDNS hostname (e.g., `esp-bridge-11fc-config.json`)

### Code Quality
- **WiFi TX Power Constant**: Added `DEFAULT_WIFI_TX_POWER` define (was magic number)
- **WiFi Reset Function**: Extracted `config_reset_wifi()` for code reuse

## v2.18.8 (Build Automation & Code Cleanup)

### CI/CD Improvements
- **GitHub Actions Multi-Board Build**: Automated firmware build for all supported boards
  - Builds zero_production, supermini_production, xiao_production in single workflow
  - Creates unified release zip with firmware.bin and firmware.elf for each board
  - Automatic GitHub Release creation on tag push
  - Release notes extracted from CHANGELOG.md

### Bug Fixes
- **WiFi TX Power UI**: Fixed visibility issue - TX Power selector now visible in both AP and Client modes (was hidden in AP mode)

### Code Quality
- **cppcheck Static Analysis**: Additional cleanup pass
  - Replaced C-style casts with `static_cast`/`reinterpret_cast`
  - Added `explicit` to single-argument constructors
  - Fixed shadow variables and improved const correctness
  - Removed unused legacy functions

### Dependency Updates
- **TaskScheduler**: 4.0.1 → 4.0.3
- **ESPAsyncWebServer**: 3.8.1 → 3.9.1
- **AsyncTCP**: 3.4.8 → 3.4.9

## v2.18.7 (Coredump to Flash - Crash Analysis) 🟢 COMPLETED

### Core Dump System
- **ESP-IDF Coredump to Flash**: Automatic crash backtrace capture and storage
  - **Flash Storage**: Crash data saved to dedicated 64KB coredump partition
  - **Post-Mortem Analysis**: Captures Program Counter, task name, exception cause/address, and backtrace (up to 16 addresses)
  - **Automatic Capture**: Triggers on PANIC, TASK_WDT, INT_WDT crashes
  - **SDK Configuration**: Enabled `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH` and `CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF` for all environments
  - **Auto-Clear**: Coredump erased after successful read to prevent re-processing

### Crash Log Enhancements
- **Extended Crash History**: JSON format now includes panic details and firmware version
  - **Exception Info**: Program Counter (PC), Task name, Exception Cause, Exception Address
  - **Firmware Version**: Stores version at crash time (not current version) via RTC variables
  - **Backtrace**: Array of memory addresses for addr2line debugging
  - **Backward Compatible**: Old crash entries without panic data continue to work

- **Web Interface Improvements**:
  - **New "Details" Column**: Shows PC address and task name for panic crashes
  - **Expandable Rows**: Click on crash entry to view full exception details with firmware version
  - **Backtrace Display**: Formatted memory addresses (4 per line for mobile readability)
  - **Copy Buttons**:
    - "Copy Exception" - copies complete exception details (version, PC, task, cause, backtrace) as text for reports/issues
    - "Copy addr2line" - copies ready-to-use addr2line command for terminal execution
  - **Exception Decoder**: Human-readable descriptions for common ESP32 exception causes (LoadProhibited, StoreProhibited, etc.)
  - **Mobile Responsive**: Hides "Min Heap" column on mobile, compact backtrace layout

### Debug Tools
- **Test Crash Endpoint**: `/test_crash` triggers intentional null pointer crash for testing
  - Demonstrates coredump capture workflow
  - Provides 2-second warning before crash
  - Useful for verifying crash analysis pipeline

### Technical Implementation
- **crashlog.cpp**:
  - Added `esp_core_dump_get_summary()` integration for backtrace capture
  - Added RTC variable `g_last_version[16]` to store firmware version at crash time
  - Updated `crashlog_update_variables()` to save current version to RTC memory
- **web_api.cpp**: Added `handleTestCrash()` endpoint for testing
- **crash-log.js**: Complete rewrite with expandable rows, exception text export, and copy functionality
- **index.html**: Updated table structure with new Details column
- **style.css**: Added panic details styling and mobile optimizations

### Documentation
- **Release Requirements**: Added note to TODO.md - releases must include both firmware.bin (for flashing) and firmware.elf (for crash debugging with addr2line)

### Code Quality Improvements
- **cppcheck Static Analysis Cleanup**: Fixed ~40 style warnings across codebase
  - Added `explicit` to single-argument constructors to prevent implicit conversions
  - Replaced C-style casts with `static_cast`/`reinterpret_cast` for type safety
  - Fixed shadow variables and improved const correctness
  - Removed 7 unused legacy functions (logging_get_config, webserver_cleanup, checkWiFiTimeout, getWebServer, wifiStop, wifiGetState, crashlog_format_uptime)
  - Fixed dead code bug in WiFi scan failure recovery (scanFailureCount reset before check)

### Dependency Updates
- **TaskScheduler**: Updated to v4.0.1 (from v3.8.5)

### Bug Fixes
- **WiFi Timeout in Permanent Mode**: Fixed 20-minute reboot bug caused by `isTemporaryNetwork` flag being overwritten in web_interface.cpp
- **Uptime Display**: Enabled formatted uptime display (shows "4m 47s", "2h 15m" instead of raw seconds)

### Known Issues
- Coredump functionality requires specific SDK configuration which may conflict with certain Arduino framework versions
- addr2line analysis requires exact firmware.elf file matching the crashed firmware

## v2.18.6 (XIAO ESP32-S3 Support) 🟡 IN DEVELOPMENT

### Platform Support
- **XIAO ESP32-S3 Board Support**: Initial implementation
  - **Pin Mapping**: Custom GPIO mapping for XIAO compact pinout
    - Device 1 (UART Bridge): GPIO4/5 (D3/D4 pins)
    - Device 2 (UART2/USB): GPIO8/9 (D8/D9 pins)
    - Device 3 (UART3): GPIO43/44 (D6/D7 pins)
    - RTS/CTS flow control: GPIO1/2 (D0/D1 pins)
  - **LED Support**: Single-color LED on GPIO21 with inverted logic (LOW=ON)
    - Blink-only mode (no RGB colors)
    - FastLED replaced with simple GPIO control for XIAO
  - **Build Configuration**: Added xiao_production and xiao_debug environments
  - **Web Interface**: Board type detection and D-pin display in device configuration
  - **Testing Status**: Basic mode tested (Device1 UART Bridge + Device2 USB with flight controller)

### Code Changes
- Updated defines.h with XIAO-specific pin definitions and comments
- Updated leds.cpp with conditional compilation for single-color LED support
- Updated web_api.cpp with XIAO board type detection
- Updated device-config.js with GPIO to D-pin mapping table for web interface
- Updated status-updates.js with XIAO board name display

### Documentation
- Updated README.md with XIAO board information and D-pin connections
- Updated help.html with XIAO pin mapping table and LED indicators
- Updated version to 2.18.6

## v2.18.5 (SBUS Phase 3 - Unified Senders + UDP Batching) ✅ COMPLETED

### SBUS Phase 3 Implementation ✅
- **Unified Sender Architecture**: Single sendDirect() method for all protocols
  - **Fast Path**: SBUS/Raw protocols use sendDirect() directly (no queue, minimal latency)
  - **Slow Path**: MAVLink/Logs use sendDirect() internally from processSendQueue()
  - **No Code Duplication**: Single transmission point, unified statistics

- **UDP Batching for SBUS**: Optimized network efficiency
  - **3-Frame Batching**: Combines 3 SBUS frames (75 bytes) into single UDP packet
  - **Packet Reduction**: ~178 → ~25 UDP packets/sec (85% reduction)
  - **Stale Protection**: 50ms timeout for incomplete batches
  - **Web Statistics**: Real-time batching metrics (avg/max frames per batch, efficiency)

- **Device4 SBUS UDP Support**: Full bidirectional SBUS over WiFi
  - **D4_SBUS_UDP_TX (role 3)**: Send SBUS frames over UDP with batching
  - **D4_SBUS_UDP_RX (role 4)**: Receive SBUS frames from UDP with failover support
  - **Web UI**: IP/Port configuration for both TX/RX modes
  - **Timing Keeper**: Frame repeat for RX mode to smooth WiFi jitter

- **Protocol Pipeline Optimization**: Improved fast/slow path separation
  - **Smart Queue Processing**: processSendQueue() skipped for SBUS roles
  - **Conflict Resolution**: Fixed atomicBatchPackets race condition
  - **Unified Flush**: Single flushBatch() function for all protocols

### UI Improvements
- Device 4 Network Configuration visibility for SBUS roles
- IP validation for TX roles, disabled for RX role
- SBUS WiFi Timing Keeper visibility for UDP RX mode
- Anti-flap log rate limiting (5sec) to reduce startup spam

### Bug Fixes
- Fixed UDP transport initialization for D4_SBUS_UDP_RX
- Fixed batching statistics calculation (efficiency display)
- Fixed web form validation for SBUS UDP roles
- Fixed SBUS Timing Keeper logic: save lastValidFrame only from UDP source (not from active source)
- Fixed SBUS Timing Keeper visibility: show only for D4_SBUS_UDP_RX (not for UART SBUS outputs)
- Optimized tSbusRouterTick task: enabled only for D4_SBUS_UDP_RX (reduced CPU usage for UART-only configs)
- **Fixed RAW protocol over UDP regression**: Removed incorrect "single input" restriction that blocked bidirectional data flow
- **Fixed buffer allocation bug**: Removed `D1_SBUS_IN` from `getOptimalBufferSize()` enum check that caused Device4 Network Bridge to use 256 bytes instead of 4096
- **Fixed USB Host regression**: Added `out_transfer_busy` flag to prevent `ESP_ERR_NOT_FINISHED` errors caused by Sender task writing data faster than USB transfers complete
- Various stability improvements

## v2.18.4 (SBUS Phase 2 Complete - Singleton Router + Failsafe) ✅ COMPLETED

### SBUS Phase 2 Implementation ✅ COMPLETED
- **Singleton Router Architecture**: Refactored from multiple routers to single global SbusRouter instance
  - **Smart Source Selection**: Automatic source switching based on quality metrics (0-100%)
  - **Priority System**: Device1 (priority 0) > Device2 (priority 1) > UDP (priority 2)
  - **Anti-Flapping**: 500ms delay prevents rapid source switching during signal degradation
  - **Mode Control**: Auto mode (automatic failover) and Manual mode (forced source selection)

- **Failsafe State Machine**: Full three-state failsafe implementation
  - **OK State**: Valid frames received, normal operation
  - **HOLD State**: Signal lost, holds last valid frame for 1 second
  - **FAILSAFE State**: Extended signal loss, outputs failsafe frame (all channels centered)
  - **Recovery**: Automatic return to OK state when valid frames resume

- **WiFi Timing Keeper**: UDP source stability enhancement
  - **20ms Frame Repeat**: Smooths WiFi jitter for UDP SBUS source only
  - **TaskScheduler Integration**: Periodic tick() execution via tSbusRouterTick task
  - **Configurable**: Controlled via config.sbusTimingKeeper flag
  - **Memory Efficient**: Minimal overhead, reuses last valid frame

- **Web API Extensions**: Full router control and monitoring
  - **GET /sbus/status**: Returns quality, priority, mode, state for all sources
  - **GET /sbus/set_source**: Manual source selection (0=Device1, 1=Device2, 2=UDP)
  - **GET /sbus/set_mode**: Mode control (0=Auto, 1=Manual)
  - **Removed Legacy**: Cleaned up old multi-router helper functions

- **Code Cleanup & Optimization**: Production-ready SBUS implementation
  - **Removed Dead Code**: Eliminated unused variables (lastOutputTime, manualMode, forcedSource)
  - **Removed Legacy Enums**: Cleaned up SbusRouterMode and other unused types
  - **Removed from SourceState**: Deleted lastFrame[25] array (saved 75 bytes per source)
  - **Removed from SbusFastParser**: Deleted static s_lastValidFrame, s_hasValidFrame, lastProcessTime
  - **Enhanced Logging**: Added sourceSwitches counter to source switch log messages
  - **Architectural Clarity**: Timing Keeper only for UDP confirmed as intentional design

### Protocol Isolation Fixes ✅ COMPLETED
- **UDP Protocol Detection**: Fixed UDP callback to check protocol optimization mode
  - **SBUS Mode**: Filters packets for valid SBUS frames (25/50 bytes, 0x0F header)
  - **RAW/MAVLink Mode**: Accepts all UDP packets without filtering
  - **Previous Bug**: All UDP packets were SBUS-filtered regardless of protocol mode
  - **Impact**: Now supports RAW and MAVLink protocols over UDP correctly

### Web Interface Improvements ✅ COMPLETED
- **UI Refactoring and Optimization**: Reorganized interface structure for better usability
  - **Advanced Configuration Block**: Consolidated Device 4, Protocol, UART, USB, Logging into single collapsible section
  - **Block Reordering**: Optimized layout - SBUS Source Selection first, then Logs, System Status, Crash History, Protocol Stats
  - **Dynamic Visibility**: UART/USB blocks show/hide based on device roles selection
  - **Visual Improvements**: Added icons to all configuration sections, removed redundant information
  - **Statistics Enhancements**: Added color coding for success rates, UDP Batching statistics in compact format
  - **System Status**: Made collapsible with state persistence
- **Phase 1 Improvements**: Collapsible blocks, dynamic labels, SBUS output device fixes, copy button positioning

## v2.18.3 (ESP32-S3 Super Mini Support + SBUS Fast Path + Memory Optimization) 🟡 READY FOR TESTING

### Release Summary
This release adds ESP32-S3 Super Mini hardware support, implements lightweight SBUS Fast Path architecture, and delivers significant memory optimizations. The codebase is feature-complete but requires hardware testing.

**Key Features:**
- ESP32-S3 Super Mini board support with adaptive LED control
- SBUS Fast Path architecture with 10x performance improvement and 15KB → 2KB memory reduction
- Memory optimization through intelligent buffer sizing and PSRAM utilization
- Enhanced diagnostics with comprehensive PSRAM reporting
- SBUS WiFi Timing Keeper for wireless stability

**Memory Improvements:**
- Protocol-aware buffer optimization for different device configurations
- PSRAM utilization for non-critical operations
- Large JSON document optimization for web operations
- Web interface memory optimization plan fully implemented (Stages A-E)
- Eliminated String concatenation in hot paths for reduced heap fragmentation

### Platform Support - ESP32-S3 Super Mini ✅ IMPLEMENTED (Not Fully Tested)
- **Multi-Board Architecture**: Added support for ESP32-S3 Super Mini with conditional compilation
  - **LED Pin Adaptation**: GPIO48 (WS2815) for Super Mini vs GPIO21 (WS2812) for S3-Zero
  - **Build Environments**: Added `supermini_production` and `supermini_debug` configurations
  - **Board Detection**: Runtime board identification via `boardType` and `usbHostSupported` fields
  - **USB Host Limitation**: Super Mini lacks USB Host hardware support

- **Web Interface Adaptation**: Dynamic USB mode control based on board capabilities
  - **Smart UI**: USB Host option automatically disabled on Super Mini boards
  - **Visual Indication**: "Host (Not supported on this board)" label for blocked options
  - **Server Protection**: Backend validation prevents USB Host mode selection on Super Mini
  - **Graceful Fallback**: Automatic Device mode selection with warning logging

- **Hardware Compatibility Notes**:
  - **UART Inversion**: Hardware UART inversion support untested on Super Mini
  - **SBUS Compatibility**: May require software-based signal inversion if hardware inversion fails
  - **Pin Compatibility**: All UART pins (4/5, 8/9, 11/12) remain identical between boards
  - **LED Protocol**: WS2815 (Super Mini) is compatible with WS2812 control protocol

### Performance Optimization - LED Task Management ✅ COMPLETED
- **Dynamic LED Task Control**: Implemented intelligent enable/disable of LED monitoring task
  - **Task Export**: Made `tLedMonitor` globally accessible via scheduler_tasks.h
  - **WiFi State Integration**: Automatic task management based on WiFi connection state
  - **Performance Gain**: ~50 iterations/second improvement in stable WiFi modes

- **LED Task Disable Conditions** (saves CPU cycles):
  - WiFi Client successfully connected (stable connection)
  - WiFi AP mode active (no animation needed)
  - Result: ~950+ iterations/second in stable modes

- **LED Task Enable Conditions** (animation required):
  - WiFi searching for network (visual feedback)
  - WiFi authentication errors (error indication)
  - WiFi reconnection attempts (activity indication)
  - Initial WiFi client startup (scanning feedback)

### Build Configuration
- **Production Build**: `pio run -e supermini_production`
- **Debug Build**: `pio run -e supermini_debug`
- **Original S3-Zero**: `pio run -e production` or `pio run -e debug` (unchanged)

### SBUS Fast Path Architecture ✅ COMPLETED
- **Fast Path Processing**: Lightweight SBUS frame processing with 10x performance improvement
  - **Virtual Method Integration**: Added `tryFastProcess()` to protocol parser base class
  - **Direct Output Routing**: SBUS frames bypass packet system for immediate forwarding
  - **Memory Optimization**: Reduced from 15KB RAM usage to ~2KB with minimal DMA buffers
  - **Frame Validation**: Built-in resync mechanism for corrupted frame recovery
  - **Source Tracking**: Multi-source support with LOCAL/UART/UDP identification

- **Device1 SBUS_IN Role**: First non-UART bridge role for Device1
  - **UART1 Reconfiguration**: Automatic setup for 100000 8E2 inverted mode
  - **Smart Router Integration**: Device1 input directly feeds SBUS output devices
  - **Context-Dependent Device4**: Adaptive roles (D4_SBUS_UDP_TX/RX) based on configuration
  - **Hardware Validation**: Prevents invalid combinations and provides user feedback

- **SBUS WiFi Timing Keeper**: Maintains wireless SBUS frame timing
  - **Static Frame Storage**: Preserves last valid SBUS frame for retransmission
  - **20ms Interval Timing**: Ensures consistent frame delivery over WiFi
  - **Conditional Operation**: Only active when timing keeper enabled and SBUS_OUT configured
  - **Memory Efficient**: Uses static class members instead of complex pipeline integration

### Memory Optimization & Performance Improvements ✅ COMPLETED
- **Buffer Size Optimization**: Dynamic buffer allocation based on device role and protocol
  - **SBUS Buffer Optimization**: Reduced SBUS device buffers for more efficient memory usage
  - **UDP SBUS Optimization**: Smaller buffers for UDP+SBUS configurations
  - **Intelligent Sizing Function**: `getOptimalBufferSize()` calculates appropriate buffer sizes
  - **Protocol-Aware Allocation**: Buffer sizes now consider protocol type and device role

- **PSRAM Memory Management**: Enhanced memory allocation with PSRAM support
  - **Log Buffer PSRAM**: Non-critical log buffer moved to PSRAM when available
  - **Automatic Fallback**: Graceful fallback to internal RAM when PSRAM unavailable
  - **Memory Type Reporting**: Diagnostic logging shows PSRAM vs internal RAM usage
  - **Performance Preserved**: DMA-critical buffers remain in fast internal RAM

- **JSON Document Optimization**: Large web interface JSON documents optimized for PSRAM
  - **Web API Optimization**: Config, logs, and status endpoints use PSRAM-aware allocation
  - **ArduinoJson v7 Integration**: Leverages ESP32 heap allocator for automatic PSRAM usage
  - **Memory Pressure Relief**: Temporary memory optimization during web operations
  - **Transparent Operation**: No API changes, automatic memory management

- **System Diagnostics Enhancement**: Extended memory reporting with PSRAM information
  - **PSRAM Status Reporting**: All diagnostic functions now show PSRAM usage (total/free)
  - **Comprehensive Memory View**: Internal RAM + PSRAM status in all system reports
  - **Memory Optimization Tracking**: Monitor memory usage improvements across optimizations

- **Web Interface Memory Optimization (Stages A-E)**: Complete memory optimization plan implementation
  - **Stage A - Streaming JSON Responses**: Eliminated large String allocations in /status and /logs endpoints
    - AsyncResponseStream for direct JSON serialization without intermediate buffers
    - Connection: close headers for immediate TCP resource release
    - Refactored with populateConfigJson() helper to avoid code duplication
  - **Stage B - Collapsible Sections**: Reduced unnecessary network requests with smart UI
    - Persistent collapsed state via localStorage (collapse:logs, collapse:protocolStats, collapse:crash)
    - Visible-only polling: /logs only requested when section is expanded
    - Unified toggle utilities: Utils.rememberedToggle() and Utils.restoreToggle()
  - **Stage C - Static Asset Caching**: Immutable cache with automatic versioning
    - Cache-Control: public, max-age=31536000, immutable for all static resources
    - Auto-versioning with 8-char hash (?v=XXXXXXXX) in embed_html.py
    - Cache invalidation on any source file change
  - **Stage D - PSRAM Config Operations**: Eliminated DRAM spikes during import/export
    - Import uses PSRAM buffer (32KB limit) without fallback to DRAM
    - Export streams directly via config_to_json_stream(Print&)
    - ImportData structure for clean PSRAM buffer handoff
    - HTTP 413 response for oversized configs
  - **Stage E - LWIP/IPv6 Tuning**: Reduced TCP memory footprint
    - IPv6 disabled (CONFIG_LWIP_IPV6=n) with related counters zeroed
    - TCP limits reduced: MAX_SOCKETS=8, MAX_ACTIVE_TCP=8, MAX_LISTENING_TCP=4
    - TCP buffers optimized: SND_BUF=2920, WND=2920, RECVMBOX=16
    - UDP settings preserved for AsyncUDP operation

- **String Operation Optimization**: Eliminated all String concatenations in hot paths
  - **Critical Path Optimizations** (executed frequently):
    - handleStatus(): uartConfig and lastActivity now use snprintf with char buffers
    - WiFi disconnect: Removed unused String variable
    - Device initialization: Direct log_msg formatting instead of String building
    - UDP statistics: char buffer for percentage formatting
  - **Additional Optimizations**:
    - crashlog_format_uptime(): Replaced String concatenation with snprintf
    - WiFi hostname generation: Removed unnecessary String() wrapper
    - Config export: Filename generation via snprintf instead of String concatenation
  - **Impact**: Reduced heap fragmentation, predictable memory usage, faster execution

### Testing Status & Pending Tasks 🟡
- **Super Mini Implementation**: Code changes completed but not tested on actual hardware
- **UART Inversion**: Hardware inversion functionality unverified on Super Mini platform
- **LED Functionality**: GPIO48 LED control implementation needs physical validation
- **SBUS Operation**: Protocol operation with potential software inversion requirement untested
- **SBUS Fast Path Testing**: Frame processing and timing keeper functionality need field validation
- **Memory Optimization Validation**: Buffer optimizations need stress testing with high data rates
- **Documentation Update**: README.md and web interface help pages not yet updated for Super Mini support

## v2.18.2 (Code Refactoring & WiFi TX Power Control) ✅ COMPLETED

### Code Organization & Memory Optimization ✅ COMPLETED
- **Constants Refactoring**: Moved domain-specific constants from defines.h to appropriate headers
  - **WiFi Constants**: RSSI thresholds relocated to wifi_manager.h
  - **LED Constants**: Timing and color definitions moved to leds.h
  - **SBUS Constants**: Protocol definitions moved to sbus_common.h
  - **Crash Log Constants**: File size limits moved to crashlog.h
  - **Web API Constants**: HTTP response constants moved to web_api.h

- **Memory Leak Fix**: Resolved Logger flow incorrectly processed in telemetry pipeline
  - **Root Cause**: Logger flow was processed as telemetry data causing frequent ParsedPacket allocations
  - **Pipeline Fix**: Modified processTelemetryFlow() to exclude flows with SOURCE_LOGS
  - **Performance**: Reduced memory leak from ~6 bytes/sec to ~1-2 bytes/sec
  - **Architecture**: Logger flow now processed separately from telemetry pipeline

- **Memory Optimization**: Improved String handling in diagnostics system
  - **String Operations**: Replaced String concatenation with char[] buffers in runAllStacksDiagnostics()
  - **Diagnostic Logging**: Optimized runBridgeActivityLog() to eliminate temporary String objects
  - **Performance**: Reduced potential heap fragmentation from periodic diagnostic tasks

### WiFi TX Power Configuration ✅ COMPLETED
- **Web Interface Control**: Added WiFi transmission power setting in web configuration
  - **Power Range**: 6 preset levels from Minimum (2dBm) to Maximum (20dBm)
  - **Real-time Config**: Immediate power adjustment without restart required
  - **Persistent Storage**: WiFi TX power setting saved to configuration file
  - **ESP32 Integration**: Uses esp_wifi_set_max_tx_power() API with 0.25dBm precision

- **Code Style Improvements**: Fixed constructor initialization list formatting consistency
  - **Comma Placement**: Standardized trailing comma style in UartDMA constructor
  - **Code Consistency**: Aligned with project coding standards

## v2.18.1 (SBUS Advanced Features & Transport Support) ✅ COMPLETED

### Enhanced SBUS Hub Architecture ✅ COMPLETED
- **Improved Statistics Tracking**: Advanced frame categorization and monitoring
  - **Real vs Generated Frames**: Distinction between frames with new data vs timing-maintenance frames
  - **Unchanged Frame Detection**: Channel hash-based detection of unchanged stick positions
  - **Failsafe Statistics**: Dedicated failsafe event counter and state tracking
  - **Timing Diagnostics**: Window-miss detection for transmission timing analysis
  - **Fresh Data Flag**: `hadNewDataSinceLastSend` flag for precise frame categorization
  
- **Optimized Frame Generation**: Enhanced timing and efficiency improvements  
  - **Standard SBUS Timing**: Corrected to 14ms intervals (71 FPS) for protocol compliance
  - **Continuous Generation**: Always-active frame generation for consistent receiver compatibility
  - **Improved Failsafe**: Better signal restoration detection and flag management
  - **Smart Routing**: Enhanced SBUS input routing logic for complex device configurations

### SBUS Transport Support ✅ COMPLETED
- **UART→SBUS Bridge**: Transport SBUS frames over regular UART connections
  - **UartSbusParser**: Dedicated parser for SBUS frames received via UART
  - **Bridge Flows**: UART2→Device3_SBUS_OUT and UART3→Device2_SBUS_OUT configurations
  - **Frame Validation**: Robust start/end byte validation for UART-transported SBUS data
  - **Use Case**: Enable SBUS over any baudrate UART connection between ESP32 devices

- **UDP→SBUS Bridge**: Wireless SBUS transmission over WiFi networks
  - **UdpSbusParser**: Specialized parser for SBUS frames in UDP packets  
  - **Network Bridge**: Device4 Network Bridge integration for wireless SBUS transport
  - **Atomic Packet Handling**: Proper UDP packet atomicity with full-buffer clearing on errors
  - **WiFi SBUS**: Real-time RC control over WiFi with <50ms latency targeting

### SBUS Routing Enhancements ✅ COMPLETED
- **UDP Transport Integration**: Full UDP routing support for SBUS data streams
  - **Routing Activation**: Enabled Device4 UDP routing in SBUS input calculations
  - **Transport Statistics**: UDP sender SBUS frame counting and diagnostic logging
  - **Network Compatibility**: SBUS frames transmitted as raw 25-byte UDP payloads

- **SBUS Input Validation**: Comprehensive input source validation
  - **Source Identification**: Named detection of Physical SBUS, UART bridges, and UDP inputs
  - **Conflict Prevention**: Single-source enforcement with detailed conflict reporting
  - **Input Validation**: Framework ensures proper SBUS configuration

### Diagnostic & Monitoring Improvements ✅ COMPLETED  
- **Enhanced Logging**: Comprehensive diagnostic output for SBUS operations
  - **UART1 Diagnostics**: SBUS frame transmission logging for protocol analysis
  - **UDP Diagnostics**: Network-based SBUS frame transmission monitoring  
  - **Parser Statistics**: Frame reception counting with invalid frame tracking
  - **Temporary Diagnostics**: Clearly marked diagnostic code for future cleanup

### Bug Fixes & Critical Improvements ✅ COMPLETED
- **Parser Conflict Resolution**: Fixed critical data loss in SBUS configurations
  - **Problem**: RawParser/MavlinkParser conflicted with SBUS parsers causing 28% data loss
  - **Solution**: Added hasSbusDevice detection to disable generic flows when SBUS is active
  - **Impact**: Reduced SBUS data loss from 28% to ~0%
  - **Affected Flows**: UDP_Input, UART2_Input, UART3_Input now properly disabled with SBUS

### Transport Method Coverage ✅ COMPLETED
- **Physical SBUS**: Direct Device2/3 SBUS_IN → SBUS_OUT connections
- **UART Transport**: SBUS over regular UART at any baudrate (115200-921600+)
- **WiFi/UDP Transport**: Wireless SBUS over local networks with real-time performance
- **Hybrid Configurations**: Multiple simultaneous transport methods with conflict detection

## v2.18.0 (SBUS Protocol Implementation & Hub Architecture) ✅ COMPLETED

### SBUS Protocol Full Implementation ✅ COMPLETED
- **SBUS Parser**: Complete SBUS frame parsing with validation and statistics
  - **Frame Validation**: Start byte (0x0F) and end byte validation (0x00, 0x04, 0x14, 0x24)
  - **Channel Extraction**: 16-channel data unpacking from 22 bytes using `unpackSbusChannels()`
  - **Flags Processing**: Frame lost and failsafe flag extraction from SBUS frames
  - **Statistics**: Valid/invalid frame counters, frame lost/failsafe tracking
  - **Implementation**: `SbusParser` class inheriting from `ProtocolParser`

### Hub Architecture Introduction ✅ COMPLETED  
- **SbusHub Implementation**: State-based packet processing replacing queue-based approach
  - **Problem Solved**: Eliminated 99% packet drops caused by queue overflow at 71 FPS
  - **State Storage**: Channel state (16 channels + flags) instead of packet queue
  - **Timer-based Output**: Fixed 14ms interval generation (71.43 FPS) independent of input
  - **Failsafe Logic**: Automatic failsafe activation after 100ms without input data
  - **Memory Efficiency**: No packet queuing, immediate state update and packet release

### Protocol Pipeline Integration ✅ COMPLETED
- **Flow Configuration**: Automatic SBUS flow setup for Device2/Device3 roles
  - **SBUS Input Flow**: `SBUS_IN` devices (D2_SBUS_IN, D3_SBUS_IN) → UART1 routing
  - **SBUS Output Flow**: UART1 → `SBUS_OUT` devices (D2_SBUS_OUT, D3_SBUS_OUT) routing
  - **Dynamic Routing**: Smart routing calculation based on device role combinations
  - **Parser Integration**: SbusParser automatically instantiated for SBUS input flows

### Auto-Protocol Detection ✅ COMPLETED
- **Startup Detection**: Automatic protocol optimization based on device roles
  - **SBUS Detection**: Auto-set `protocolOptimization = PROTOCOL_SBUS` when SBUS devices present
  - **Runtime Application**: Applied at system startup after config loading
  - **Statistics Integration**: Proper SBUS statistics display in web interface
  - **Backward Compatibility**: No manual protocol selection required

### Device Statistics Integration ✅ COMPLETED
- **TX Statistics**: Proper Device2/Device3 TX byte counting for SBUS output
  - **Hub Integration**: SbusHub updates `g_deviceStats.device3.txBytes` directly
  - **Device Awareness**: Hub knows its device index (IDX_DEVICE3, IDX_DEVICE2_UART2)
  - **Real-time Tracking**: Statistics update with each 25-byte SBUS frame sent
  - **Web Display**: Correct TX statistics in `/status` interface

### SBUS Web Interface ✅ COMPLETED
- **Statistics Display**: Dedicated SBUS statistics rendering in web interface
  - **Frame Statistics**: Valid/invalid frame counts with success rate calculation
  - **Signal Quality**: Frame lost and failsafe counters with percentage rates
  - **Parser Detection**: Improved parser search across all flows (not just first)
  - **Hub Statistics**: Frames received/generated counters from SbusHub

### Technical Improvements ✅ COMPLETED
- **Code Cleanup**: Removed obsolete `SbusSender` class and unused includes
- **Comment Standards**: All code comments converted to English per coding standards
- **Memory Management**: Efficient packet handling without memory leaks
- **Error Handling**: Robust frame validation with graceful error recovery

## v2.17.0 (MAVLink Parser Channel Fix & Performance Optimizations) ✅ COMPLETED

### Critical MAVLink Parser Channel Fix ✅ COMPLETED
- **Channel Conflict Resolution**: Fixed critical bug where all MAVLink parsers shared the same channel causing state conflicts
  - **Root Cause**: All 5 MAVLink parsers used `rxChannel=0`, causing shared `rxStatus` state corruption between flows
  - **Solution**: Assign unique channels (0-4) to each parser: Telemetry(0), USB(1), UDP(2), UART2(3), UART3(4)
  - **Result**: Eliminated MAVLink sequence gaps and fixed MAVFtp packet loss during parameter download
  - **Implementation**: Modified `MavlinkParser` constructor to accept channel parameter, updated all instantiations

### Bidirectional Pipeline Implementation ✅ COMPLETED
- **Architecture Implementation**: Partial implementation of bidirectional pipeline design (Aug 30 plan)
  - **Input Buffers**: Added 4 separate input buffers for USB/UDP/UART2/UART3 sources
  - **Anti-echo Protection**: Implemented packet source tracking to prevent loops
  - **Physical Interface Tracking**: Added interface identification for routing decisions

### Pipeline Performance Optimizations ✅ COMPLETED
- **Non-blocking Senders**: Removed blocking timeouts from USB and UDP senders (Sep 1)
  - **Issue**: Senders blocked main loop waiting for batch timeouts, reducing iterations from 1000 to 780-850/sec
  - **Fix**: Exit immediately if conditions not met, check on next iteration instead of waiting
- **Input Flow Time Limits**: Added temporal constraints for bidirectional processing (Sep 1)
  - **Issue**: `processInputFlows()` had no time limits, blocking main loop during FTP transfers
  - **Fix**: Added 5ms time limit and iteration caps to prevent main loop stalls
- **Sender Task Implementation**: Moved heavy USB operations to dedicated task (Sep 1)
  - **Architecture**: Separate task for USB bulk transfers to prevent main loop blocking
  - **Queue Management**: Ring buffer for packet passing between main loop and sender task

### UART Batch Processing Improvements ✅ COMPLETED
- **Batch Read Implementation**: Optimized UART1 data reading for higher throughput (Sep 1)
  - **Method**: Read up to 320 bytes per batch instead of single bytes
  - **Buffer Management**: Improved circular buffer write patterns for batch operations
  - **Statistics**: Enhanced throughput monitoring for batch vs single-byte performance

### Telemetry Flow Architecture Refinements ✅ COMPLETED
- **Flow Prioritization**: Separated telemetry and input flow processing (Sep 2)
  - **Telemetry Priority**: Higher priority for FC→GCS direction with exhaustive parsing
  - **Input Flow Limits**: Time-bounded processing for GCS→FC direction to prevent blocking
  - **Buffer Balance**: Optimized buffer allocation between bidirectional flows

## v2.16.0 (MAVLink Routing Implementation & Input Gateway) ✅ COMPLETED

### MAVLink Routing Architecture Implementation ✅ COMPLETED
- **Parser-Router Separation**: Moved target extraction from router to parser for clean architecture
  - **MavlinkParser**: Added `extractTargetSystem()` and `extractTargetComponent()` methods using official pymavlink getters
  - **MAVLink Getters**: Implemented proper target extraction using `mavlink_msg_*_get_target_system()` functions
  - **Routing Flag**: Added `routingEnabled` flag to enable target extraction only when routing is active
  - **Code Quality**: Replaced magic numbers (20, 21, 23...) with MAVLink constants (`MAVLINK_MSG_ID_PARAM_REQUEST_READ`, etc.)
- **Router Simplification**: Removed duplicate target extraction logic from `MavlinkRouter`
  - **Deleted Methods**: `extractTargetSystem()` and `isRoutableMessage()` - now handled by parser
  - **Broadcast Fix**: Corrected invalid broadcast check - `sysid=255` is GCS, not broadcast target
  - **ROUTABLE_MSG_IDS**: Removed unused message ID array after parser refactoring

### Input Gateway Implementation for Device2-4 to UART1 ✅ COMPLETED  
- **Device Input Routing**: Created `InputGateway` class for routing device2-4 data to UART1
  - **Temporary Passthrough**: Direct data forwarding to `uartBridgeSerial` before full bidirectional pipeline
  - **MAVLink Learning**: Extracts sysid from incoming MAVLink packets for address book population
  - **Interface Mapping**: Maps device interfaces to router address book for future routing
- **Protocol Integration**: Connected input gateway to protocol pipeline
  - **Router Learning**: `learnAddress()` method for manual address book updates from input gateway
  - **Statistics Tracking**: Counters for processed packets and learned sysids
  - **Configuration**: Enabled only when MAVLink routing is active in config

### Code Quality & Magic Numbers Elimination ✅ COMPLETED
- **MAVLink Constants**: Replaced all magic numbers with proper MAVLink constants
  - **Router Arrays**: `ALWAYS_BROADCAST_IDS` now uses `MAVLINK_MSG_ID_HEARTBEAT`, `MAVLINK_MSG_ID_SYS_STATUS`, etc.
  - **Parser Methods**: Switch cases use `MAVLINK_MSG_ID_PARAM_REQUEST_READ` instead of `20`, etc.
  - **Input Gateway**: Added TODO comments for future magic number replacement (`0xFD`, `0xFE`, offsets)
- **Diagnostic Logging**: Added comprehensive debugging for routing development
  - **Router Dumps**: Periodic address book dumps every 5 seconds with routing statistics
  - **Parser Target Logs**: Extraction logging for routable messages with throttling
  - **Pipeline Flags**: Routing enabled/disabled status logging during initialization

### Bug Fixes & Critical Corrections ✅ COMPLETED
- **Broadcast Target Fix**: Removed incorrect `targetSys == 255` broadcast check in router
  - **Root Cause**: `sysid=255` is standard GCS identifier, not broadcast indicator  
  - **Impact**: Fixed routing hits calculation - commands to/from GCS now route correctly
  - **Address Book**: GCS entries (sysid=255) now properly used for unicast routing
- **Architecture Consistency**: Fixed parser/router responsibility separation
  - **File Structure**: Created `mavlink_parser.cpp` for method implementations (was inline in header)
  - **Method Declarations**: Proper header/implementation separation to avoid compilation conflicts

## v2.15.7 (LED System Refactoring & Optimization) ✅ COMPLETED

### LED System Complete Refactoring ✅ COMPLETED
- **Code Size Reduction**: leds.cpp reduced from 600 to 462 lines (-138 lines, 23% reduction)
- **Architecture Unification**: Replaced 6 separate blinking systems with 1 universal system
  - **Old System**: Individual functions for WiFi, Ethernet, UART1-4 LED blinking
  - **New System**: Single `BlinkState` structure array with `BlinkType` enum
  - **State Management**: Centralized blinking logic in unified `updateLED()` function
- **Data Structure Optimization**: Reduced from 40+ individual variables to 5 structured elements
  - **BlinkState Structure**: `active`, `isOn`, `count`, `onTime`, `offTime`, `colorValue`, `nextTime`
  - **BlinkType Enum**: `BLINK_WIFI`, `BLINK_ETH`, `BLINK_UART1-4` for type identification
  - **State Array**: `blinkStates[6]` for all LED types with proper initialization

### Performance & Safety Improvements ✅ COMPLETED
- **Mutex Optimization**: Single mutex capture per update cycle instead of multiple captures
- **Time Handling**: Proper `millis()` overflow handling with safe time arithmetic
- **Code Cleanup**: Removed unused functions and duplicate logic
  - **Removed**: `setLED()` function (-9 lines)
  - **Eliminated**: Counter update duplication (-6 lines)
- **Volatile Warning Fix**: Replaced deprecated `volatile--` with explicit assignment

### Code Quality Enhancements ✅ COMPLETED
- **Maintainability**: Easy addition of new blink patterns through enum extension
- **Readability**: Clear separation of state management and LED control logic
- **Safety**: Proper volatile variable handling and thread-safe operations
- **Efficiency**: Optimized update loop with early returns and reduced function calls

### WiFi Manager Code Refactoring & Style Improvements ✅ COMPLETED
- **Code Size Reduction**: wifi_manager.cpp reduced from 586 to 538 lines (-48 lines, 8.2% reduction)
- **Function Naming**: Renamed all functions from snake_case to camelCase for consistency
  - **Old**: `wifi_manager_init()`, `wifi_manager_process()`, `wifi_manager_is_ready_for_data()`
  - **New**: `wifiInit()`, `wifiProcess()`, `wifiIsReady()`
  - **Updated**: 8 function declarations + implementations + 8 external call sites
- **Constants Extraction**: Replaced magic numbers with named constants
  - **WiFi Constants**: `WIFI_RECONNECT_DELAY_MS`, `WIFI_TX_POWER_LEVEL`, `WIFI_MIN_HEAP_BYTES`
  - **Buffer Sizes**: `WIFI_SSID_MAX_LEN`, `WIFI_PASSWORD_MAX_LEN`, `WIFI_CONNECT_TIMEOUT_MS`
  - **Utility**: `WIFI_MDNS_SERVICE_PORT`, `WIFI_MAC_SUFFIX_BUFFER_SIZE`

### Code Quality & Maintainability Improvements ✅ COMPLETED
- **Error Handling Unification**: Created helper functions for consistent error reporting
  - **ESP-IDF Errors**: `handleEspError()` function replaced 6 duplicate error handling blocks
  - **mDNS Errors**: `logMdnsError()` function unified mDNS error reporting
  - **Credentials Helper**: `setWifiCredentials()` function replaced 4 duplicate strncpy operations
- **Legacy Code Cleanup**: Removed unused and historical code elements
  - **Removed**: `wifiIsConnected()` legacy function (unused, 5 lines saved)
  - **Cleaned**: Historical comments about code migration and architecture changes
  - **Fixed**: Misleading constant name `WIFI_TX_POWER_DBM` → `WIFI_TX_POWER_LEVEL`
- **Variable Naming**: Improved variable names for better code readability

### Flow Control System Complete Refactoring ✅ COMPLETED
- **Architecture Simplification**: Replaced complex auto-detection system with simple ESP-IDF implementation
  - **Removed Files**: `flow_control.cpp` and `flow_control.h` (complex detection logic eliminated)
  - **Removed Globals**: `FlowControlStatus` structure and global variable from `main.cpp`
  - **Simplified Logic**: Direct ESP-IDF `UART_HW_FLOWCTRL_CTS_RTS` usage instead of custom detection
- **Object-Oriented Flow Control**: Integrated flow control status into UART interface
  - **New Method**: `getFlowControlStatus()` virtual method in `UartInterface` base class
  - **UART Implementation**: Flow control status reporting through `UartDMA` class
  - **Web Integration**: Updated `web_api.cpp` to use UART interface method instead of global function
- **Hardware Configuration**: Flow control enabled only for UART1 with proper GPIO configuration
  - **GPIO Pins**: RTS=GPIO6, CTS=GPIO7 as defined in hardware specification
  - **Threshold**: RX flow control threshold set to 100 bytes (~78% of 128-byte FIFO)
  - **User Control**: Flow control activated only when explicitly enabled via web interface
- **Build System Fixes**: Resolved compilation issues from previous refactoring
  - **Header Dependencies**: Added `#include <Arduino.h>` to `uart_interface.h` for `String` type
  - **Dead Code Removal**: Cleaned up remaining references to `globalTxBytesCounter` in protocol headers
  - **Incremental Build**: Fixed issues where header-only changes weren't properly recompiled
  - **Network Detection**: `foundNow` → `networkFoundNow`
  - **IP Address**: `ip_str` → `ipAddressStr` 
  - **WiFi Config**: `cfg` → `wifiConfig`
- **Micro-optimizations**: Small performance improvements
  - **Removed**: Duplicate extern declarations
  - **Optimized**: Cached hostname length in validation loop

### Web Interface Code Optimization & Cleanup ✅ COMPLETED
- **Code Size Reduction**: Combined optimization of web API and interface modules
  - **web_api.cpp**: Minor cleanup with constants for magic numbers and historical comment removal
  - **web_interface.cpp**: Reduced from 211 to 192 lines (-19 lines, 9% reduction)
- **HTTP Response Optimization**: Created helper function for gzipped static file serving
  - **Before**: 7 identical 3-line blocks for CSS/JS files (21 lines total)
  - **After**: Single `sendGzippedResponse()` helper + 7 one-line calls (13 lines total)
  - **Eliminated Duplication**: Response creation, gzip header, and sending logic unified
- **Magic Numbers → Named Constants**: Improved code readability
  - **HTTP_PORT (80)**: Web server port configuration
  - **UPLOAD_BUFFER_RESERVE (4096)**: File upload buffer pre-allocation size  
  - **ASCII_PRINTABLE_THRESHOLD (32)**: Character filtering threshold for uploads
- **Code Quality Improvements**: Removed historical references and unnecessary comments
  - **Cleaned**: Architecture change comments and implementation history notes
  - **Simplified**: Function documentation to focus on current behavior rather than past changes

### Task Scheduler Code Cleanup ✅ COMPLETED
- **Historical Comments Removal**: Cleaned up scheduler_tasks.cpp from legacy references
  - **Removed 10+ comments**: All "Statistics update tasks removed" and similar historical notes
  - **Focused Documentation**: Code comments now describe current functionality rather than past architecture changes
  - **Cleaner Codebase**: Eliminated references to removed statistics update tasks and deprecated functionality

## v2.15.6 (Types System Refactoring & Code Organization) ✅ COMPLETED

### Major Code Architecture Refactoring ✅ COMPLETED
- **Types.h Modularization**: Split large types.h file into logical modules
  - **File Reduction**: types.h reduced from ~400 to ~264 lines for faster compilation
  - **Logical Separation**: Device types, statistics, and core types properly separated
  - **Better Organization**: Clear modular structure by functionality
  - **Compilation Speed**: Changes to device statistics no longer recompile entire project

### New Modular Header Structure ✅ COMPLETED
- **device_stats.h**: Device statistics structure and management functions
  - **DeviceStatistics Structure**: Atomic counters for all 4 devices (Device1-4)
  - **Global Instance**: `extern DeviceStatistics g_deviceStats` declaration
  - **Management Functions**: `initDeviceStatistics()` and `resetDeviceStatistics()` 
- **device_stats.cpp**: Implementation of statistics functions and global instance
- **device_types.h**: Device roles, enums, and configuration structures
  - **Device Role Enums**: Device1Role, Device2Role, Device3Role, Device4Role
  - **Core Enums**: BridgeWiFiMode, UsbMode, LogLevel (moved from types.h)
  - **Config Structure**: Complete configuration structure with proper enum types
- **types.h Simplified**: Core system types only
  - **Essential Types**: BridgeMode, WiFiClientState, LedMode, SystemState
  - **BridgeContext**: Protocol pipeline context (maintained for compatibility)
  - **Include Integration**: Automatically includes new modular headers

### Cross-Core Communication Cleanup ✅ COMPLETED  
- **Removed Inter-Core Hacks**: Eliminated unnecessary cross-core synchronization
  - **Global Pipeline Pointer**: Removed `g_pipeline` global variable
  - **Memory Barriers**: Removed `__sync_synchronize()` calls
  - **External Declarations**: Cleaned up bridge_processing.h
- **Legacy Buffer Methods**: Removed unused cross-core communication methods
  - **writeWithSource()**: Removed from circular_buffer.h
  - **readWithSource()**: Removed from circular_buffer.h  
  - **writeInternal()**: Removed helper method
- **Architecture Simplification**: All components now run on Core 0, no sync needed

### Dead Code Elimination ✅ COMPLETED
- **Statistics Update Functions**: Removed empty update functions
  - **Diagnostics Cleanup**: Removed `updateMainStats()`, `updateDevice3Stats()`, `updateDevice4Stats()`
  - **Scheduler Tasks**: Removed unused statistics update tasks from TaskScheduler
  - **Task References**: Cleaned up all enable/disable/addTask calls for removed tasks

### Configuration Management Refactoring ✅ COMPLETED
- **Code Quality Improvements**: Enhanced maintainability and reduced duplication in config.cpp
- **Constants Extraction**: Replaced magic numbers with named constants
  - **Network**: `DEFAULT_BAUDRATE` (115200), `DEFAULT_UDP_PORT` (14560)
  - **Processing**: `JSON_PREVIEW_SIZE` (200), `IP_BUFFER_SIZE` (15)
- **Helper Functions**: Created utility functions to eliminate code duplication
  - **Migration Helper**: `finalizeMigration()` replaced 7 duplicate log+version blocks (-14 lines)
  - **Device Defaults**: `setDeviceDefaults()` replaced 2 duplicate device role assignments (-6 lines)
- **Code Cleanup**: Removed historical comments and improved consistency
  - **Removed**: "NEW" markers from UDP batching implementation
  - **Enhanced**: Protocol constant usage (`0` → `PROTOCOL_NONE` for better readability)
- **Scheduler Task Cleanup**: Removed empty if-blocks and historical comments from scheduler_tasks.cpp
  - **Eliminated**: 4 empty Device3/Device4 if-blocks (legacy after refactoring)
  - **Cleaned**: 10+ historical comments about removed statistics functions

### Crash Logging System Refactoring ✅ COMPLETED
- **Code Quality Improvements**: Enhanced maintainability and reduced duplication in crashlog.cpp
- **Helper Functions**: Created utility functions to eliminate code duplication
  - **Empty JSON Structure**: `createEmptyLog()` helper replaced 5 duplicate initialization blocks
  - **Fallback JSON**: `getFallbackJson()` helper replaced 3 duplicate error return strings
  - **File Writing**: `writeJsonToFile()` helper replaced 3 duplicate file write operations
- **Constants**: Added `CRASHLOG_MAX_FILE_SIZE` constant for better code readability
- **Code Cleanup**: Improved consistency and maintainability throughout crash logging system

### Legacy Code and Comment Cleanup ✅ COMPLETED
- **Historical Comments Removal**: Cleaned up outdated comments throughout codebase
- **Files Updated**: adaptive_buffer.h, bridge_processing.h, circular_buffer.h, uartbridge.cpp/h
- **Legacy Code Removal**: Removed unused enum DataSource from circular_buffer.h
- **Comment Consistency**: Simplified technical comments while preserving TODO markers and diagnostics

### Protocol Pipeline API Cleanup ✅ COMPLETED
- **Legacy Parameter Removal**: Eliminated unused txCounter parameter from all PacketSender constructors
- **Dead Code Elimination**: Removed updateTxCounter() helper functions and globalTxBytesCounter field
- **API Simplification**: Simplified constructor calls by removing nullptr parameters across pipeline
- **Constants Extraction**: Replaced magic numbers with named constants for buffer sizes
  - **DEFAULT_MAX_PACKETS** (20), **DEFAULT_MAX_BYTES** (8192) for UART/UDP senders
  - **USB_MAX_PACKETS** (128), **USB_MAX_BYTES** (24576) for USB sender optimized buffers
- **Code Quality**: Improved maintainability and self-documenting code through consistent naming
- **Project Structure**: Moved device_init files to src root
  - **Simplified Paths**: `devices/device_init.h` → `device_init.h` 
  - **Include Updates**: Fixed all include paths throughout codebase
  - **Directory Cleanup**: Removed empty devices/ directory

### Forward Declaration Conflicts Resolution ✅ COMPLETED
- **Enum Type Conflicts**: Fixed forward declaration vs full definition mismatches
  - **BridgeWiFiMode, UsbMode, LogLevel**: Moved complete definitions to device_types.h
  - **Removed Conflicts**: Eliminated forward declarations with base types
  - **Proper Dependencies**: Maintained correct include order through types.h
- **Include Path Optimization**: Updated all files requiring new headers
  - **main.cpp**: Added device_stats.h include, updated initialization
  - **web_api.cpp**: Added device_stats.h include for statistics access
  - **uartbridge.cpp**: Removed duplicate statistics initialization

### USB Host Code Style Refactoring ✅ COMPLETED
- **Class Structure**: Created proper usb_host.h header file with class declarations
- **Method Decomposition**: Split large `handleDeviceConnection()` into focused methods
  - **openDevice()**: Device opening and validation (8 lines)
  - **getDeviceInfo()**: Device information retrieval (17 lines)
  - **claimInterface()**: Interface claiming with error handling (12 lines)
  - **setupTransfers()**: USB transfer setup and initialization (23 lines)
- **CDC Interface Refactoring**: Improved `findCDCInterface()` method structure
  - **findBulkEndpoints()**: Separated endpoint discovery logic (44 lines)
  - **Better Error Handling**: Early returns instead of deep nesting
  - **Enhanced Logging**: Detailed debug information for USB operations
- **Naming Consistency**: Updated all methods to match project camelCase style
  - **Task Methods**: `usb_host_task` → `usbHostTask`
  - **Callbacks**: `client_event_callback` → `clientEventCallback`
  - **Variables**: `bulk_in_ep` → `bulk_in_endpoint`
- **Documentation**: Added comprehensive method comments and parameter descriptions

### Technical Benefits 🚀
- **Faster Compilation**: Modular headers reduce interdependencies
- **Cleaner Architecture**: Clear separation of concerns across modules  
- **Better Maintainability**: Smaller focused files easier to understand and modify
- **Reduced Memory Usage**: Eliminated unused tasks, variables, and functions
- **Improved Code Quality**: Consistent style and better error handling throughout
- **Lock-free Performance**: Removed unnecessary synchronization primitives

## v2.15.5 (Statistics & LED System Refactoring) ✅ COMPLETED

### Major Architecture Refactoring ✅ COMPLETED
- **Unified Device Statistics**: Complete migration to atomic-based statistics system
  - **Atomic Operations**: Replaced critical sections with `std::atomic<unsigned long>` and `memory_order_relaxed`
  - **Global Statistics**: Centralized `DeviceStatistics g_deviceStats` for all 4 devices (Device1-4)
  - **Thread Safety**: Eliminated spinlocks and critical sections for lock-free statistics
  - **Memory Reduction**: Removed task-local counters and UartStats structure
  - **Simplified Architecture**: Direct atomic increments throughout the codebase

### Device Statistics Centralization ✅ COMPLETED
- **DeviceCounter Structure**: Standardized RX/TX byte counters for all devices
  - **Device1**: UART1 primary interface statistics
  - **Device2**: USB/UART2 secondary interface statistics  
  - **Device3**: UART3 mirror/bridge interface statistics
  - **Device4**: UDP network interface statistics
- **System-wide Counters**: Added global activity tracking and system start time
- **Web Interface Integration**: All statistics accessible via atomic loads
- **Reset Functionality**: Unified statistics reset across all devices

### LED Monitor Task Implementation ✅ COMPLETED
- **Centralized LED Management**: Replaced scattered LED calls with unified monitor task
  - **TaskScheduler Integration**: 50ms interval LED monitoring task
  - **Snapshot Comparison**: Efficient LED state updates using LedSnapshot structure
  - **Activity Detection**: Smart LED updates only when device activity changes
  - **Bridge Mode Awareness**: LED task enabled/disabled based on bridge mode
- **Performance Optimization**: Eliminated redundant LED calls throughout codebase
- **Code Simplification**: Removed LED timing variables from bridge processing

### Protocol Pipeline Integration ✅ COMPLETED
- **Sender Statistics Cleanup**: Removed static counters from UartSender and UdpSender
  - **Direct Statistics**: Senders now update global g_deviceStats directly
  - **Memory Optimization**: Eliminated duplicate statistics tracking
  - **Unified Interface**: All statistics accessible through single global structure
- **Pipeline Statistics**: Protocol processing statistics integrated with global system

### Web Interface Statistics Overhaul ✅ COMPLETED
- **Atomic Statistics Access**: All web API endpoints use atomic loads
  - **Lock-free Operations**: Replaced critical sections with `load(std::memory_order_relaxed)`
  - **Real-time Updates**: Direct access to current statistics without blocking
  - **JSON Field Compatibility**: Maintained existing field names for frontend compatibility
- **Statistics Reset**: Simplified reset functionality using atomic stores
- **Diagnostic Integration**: Enhanced statistics reporting in web interface

### Code Architecture Cleanup ✅ COMPLETED
- **Legacy Statistics Removal**: Eliminated UartStats structure and related functions
  - **Types.h Cleanup**: Removed UartStats, statsMux, and old statistics functions
  - **Bridge Context**: Simplified BridgeContext initialization signature
  - **Function Signatures**: Updated all initialization functions to remove UartStats parameters
- **Critical Section Elimination**: Removed all portMUX-based statistics protection
- **Memory Efficiency**: Reduced memory footprint through unified statistics approach

### Technical Benefits 🚀
- **Lock-free Performance**: Atomic operations eliminate contention and improve throughput
- **Simplified Maintenance**: Single statistics system across entire codebase
- **Better Reliability**: No deadlocks or race conditions in statistics collection
- **Reduced Memory Usage**: Eliminated duplicate counters and synchronization primitives
- **Cleaner Architecture**: Clear separation between statistics collection and LED management

## v2.15.4 (UDP Sender Refactoring & Protocol-Aware Batching) 🚀

### Major Architecture Simplification ✅ COMPLETED
- **SPSC Queue Removal**: Eliminated complex inter-core Single Producer Single Consumer queue system
  - **Removed Components**: device4Task completely deleted, UdpTxQueue removed from UdpSender
  - **Memory Reduction**: ~6KB saved (4×1500 byte queue slots + task overhead)
  - **Latency Improvement**: Direct UDP transmission without inter-core queuing delays
  - **Code Simplification**: 300+ lines of complex synchronization code removed

### Direct UDP Integration ✅ COMPLETED  
- **Direct AsyncUDP**: UdpSender now uses AsyncUDP transport directly (no task intermediary)
- **UART→UDP Path**: processDevice1Input() → CircularBuffer → Pipeline → UdpSender → AsyncUDP (direct)
- **UDP→UART Path**: AsyncUDP onPacket → uartBridgeSerial->write() (direct bypass of Pipeline)
- **Statistics Integration**: UDP TX/RX stats moved to static UdpSender members for thread-safe access
- **Transport Creation**: UDP transport created in main.cpp for both Bridge and Logger modes

### Buffer Architecture Separation ✅ COMPLETED
- **Dual Buffer System**: Separate circular buffers for different operational modes
  - **Telemetry Buffer**: For UART→USB/UDP telemetry processing (Bridge mode)
  - **Log Buffer**: For line-based log processing (Logger mode, 1024 bytes)
- **Memory Optimization**: Only allocate needed buffer per mode (Logger saves ~1KB+ adaptive buffer)
- **Buffer Manager**: New buffer_manager.h/cpp for centralized buffer allocation/deallocation
- **Context Separation**: BridgeContext.buffers struct replaces old adaptive.circBuf reference

### UDP Logger Mode Enhancements ✅ COMPLETED
- **Buffer Initialization**: UDP log buffer properly zeroed to prevent garbage data transmission
- **WiFi Connection Checks**: Logger task clears buffers when WiFi disconnected
- **Line Parser Improvements**: Enhanced LineBasedParser supports \\r\\n, \\r, \\n line endings
- **Long Line Handling**: Graceful handling of lines without endings (skip with warning)
- **Logger-Bridge Separation**: UART telemetry ignored in Logger-only mode (prevents buffer conflicts)

### Protocol Pipeline Architecture ✅ COMPLETED
- **Mode-Specific Initialization**: Pipeline uses correct buffer based on Device4 role
  - **Logger Mode**: Uses logBuffer + LineBasedParser
  - **Bridge Mode**: Uses telemetryBuffer + RawParser/MavlinkParser
- **Buffer Validation**: Proper null-pointer checks for buffer availability
- **Error Handling**: Clear error messages for buffer allocation failures

### Compilation & Integration Fixes ✅ COMPLETED
- **Static Variable Definitions**: UDP statistics properly defined in udp_sender.cpp
- **Include Dependencies**: Added buffer_manager.h includes to uart/uartbridge.cpp
- **Context Access**: scheduler_tasks.cpp uses getBridgeContext() for buffer access
- **Bridge Processing**: Added Logger mode checks to prevent telemetry buffer usage

### Architecture Benefits 🚀
- **Simplified Design**: Removed 3 complex architectural layers (SPSC, Device4Task, injection)
- **Better Performance**: Direct UDP transmission reduces latency and CPU overhead
- **Memory Efficient**: Mode-specific buffer allocation saves memory
- **Maintainable Code**: Cleaner separation between Logger and Bridge functionality
- **Robust Logging**: UDP Logger mode with proper WiFi handling and line parsing

### Final Architecture Stabilization ✅ COMPLETED
- **Fixed Sender Indices**: Implemented stable IDX_USB=0, IDX_UART3=1, IDX_UDP=2 sender routing
  - **Eliminated Array Shifting**: Fixed sender indices prevent routing instability 
  - **PacketSource Integration**: Added SOURCE_TELEMETRY, SOURCE_LOGS, SOURCE_DEVICE4 packet tagging
  - **DataFlow Architecture**: Multi-stream processing with separate Telemetry and Logger flows
  - **Sender Mask Routing**: Precise packet routing with SENDER_USB, SENDER_UART3, SENDER_UDP masks

- **Bridge Processing Fix**: Resolved USB telemetry blocking in Logger mode
  - **Logger Mode Issue**: USB received no data when Logger was active (bridge_processing.h)
  - **Root Cause**: processDevice1Input() blocked UART→telemetryBuffer in D4_LOG_NETWORK mode
  - **Solution**: Removed blocking condition, USB always processes data through telemetryBuffer
  - **Result**: Logger mode with simultaneous USB telemetry and UDP log transmission

- **UDP Sender Universal Naming**: Renamed MAVLink-specific naming to universal atomic packet handling
  - **Renamed**: mavlink* → atomic* (atomicBatchBuffer, processAtomicPacket, etc.)
  - **Scope**: Universal batching for MAVLink telemetry, line-based logs, and future atomic packets
  - **Maintained**: All existing batching functionality and performance optimizations
  - **Prepared**: Framework for packet-type-specific batching strategies via PacketSource

- **WiFi Connectivity Universalization**: Created unified WiFi readiness checking
  - **New Function**: wifi_manager_is_ready_for_data() works for both AP and Client modes
  - **AP Mode**: Checks for connected clients before allowing data transmission
  - **Client Mode**: Checks connection status to target network
  - **Integration**: Used across UDP Logger, UDP Sender, and other network components
  - **Legacy**: Maintained wifi_manager_is_connected() for compatibility

- **UDP Batching Configuration Control**: Added configurable UDP packet batching for legacy GCS compatibility
  - **Configuration Field**: New udpBatchingEnabled setting (default: enabled) in config version 8
  - **Web Interface**: Added checkbox in Device 4 Network Configuration panel
  - **Backend Integration**: UdpSender.setBatchingEnabled() method for runtime configuration
  - **Automatic Migration**: Existing configurations upgraded to version 8 with batching enabled
  - **Performance Control**: Users can disable batching if GCS doesn't support batched UDP packets
  - **Logging**: Clear indication when UDP batching is enabled/disabled in system logs

### Architecture Evolution Within v2.15.3 📈
- **Phase 1**: Initial SPSC queue implementation for Device 4 Pipeline integration
- **Phase 2**: Real-world testing revealed complexity vs benefit imbalance  
- **Phase 3**: Architecture simplified - removed SPSC queue, implemented direct UDP + buffer separation
- **Phase 4**: Final stabilization - fixed routing, universal naming, WiFi handling improvements
- **Result**: Production-ready, maintainable, and performance-optimized architecture

## v2.15.2 (UDP Sender Refactoring & Protocol-Aware Batching) 🚀

### UDP Sender Architecture Overhaul ✅ COMPLETED
- **Protocol-Aware Batching**: Separate MAVLink and RAW packet batching strategies
  - **MAVLink Packets**: Optimized batching with keepWhole integrity (2-5 packets per batch)
  - **RAW Data**: Legacy batching for non-protocol data streams
  - **Smart Routing**: Automatic packet classification using keepWhole flag from parsers
  - **Adaptive Thresholds**: Different limits for normal (2 packets/600 bytes/5ms) vs bulk mode (5 packets/1200 bytes/20ms)

### Network Transmission Efficiency ✅ COMPLETED
- **MTU-Aware Batching**: Up to 1400 bytes per UDP datagram for optimal network utilization
- **Bulk Mode Integration**: Extended timeouts (20ms) for MAVFtp transfers, low latency (5ms) for telemetry
- **Timeout-Based Flushing**: Prevents packet stalling during low-traffic periods
- **Protocol Consistency**: Unified approach with USB sender optimizations

### Code Architecture Cleanup ✅ COMPLETED
- **TransmitHints Simplification**: Removed deprecated canBatch and urgentFlush flags
- **Parser Updates**: Cleaned up mavlink_parser.h and raw_parser.h hint assignments
- **Diagnostic Logging**: Added UDP-specific debug traces for batch operations and timeout events
- **Memory Safety**: Proper destructor with batch flushing before cleanup

## v2.15.1 (Task Priority Optimization & USB Batching Improvements) 🚀

### Task Priority Rebalancing ✅ COMPLETED
- **DMA Task Priority Increase**: Elevated DMA task priority above UartBridge task
  - **Problem Solved**: Eliminated UART FIFO overflow issues during high-throughput operations
  - **Result**: Stable data loading without packet loss
  - **Architecture**: Better real-time performance for DMA operations
  - **Impact**: More reliable MAVFtp and bulk transfer operations

### USB Batching Timeout Optimization ✅ COMPLETED
- **Adaptive Batch Timeouts**: Different timeouts for normal vs bulk modes
  - **Normal Mode**: 5ms timeout for low latency telemetry
  - **Bulk Mode**: 20ms timeout for optimal MAVFtp batching efficiency
  - **Smart Fallback**: Partial batch transmission on timeout to prevent data stalling
  - **Buffer Size Unification**: TX buffer increased to 2048 bytes across USB Device/Host modes
- **Complete Partial Write Elimination**: Full architectural cleanup
  - **Removed**: Pending buffer structure, flushPending() method
  - **Simplified**: Single packet and batch transmission without partial writes
  - **Enhanced**: Timeout-based partial batch transmission for stuck scenarios

### System Stability Improvements ✅ COMPLETED
- **USB Block Detection Timeout**: Increased from 500ms to 1000ms for better detection accuracy
- **Pool Exhausted Logging**: Rate limiting (1/second) and reduced severity (INFO vs WARNING)
- **Statistics Interface Fix**: Real MAVLink statistics instead of hardcoded zeros
  - **Backend**: getSender() methods for accessing real USB transmission counters
  - **Frontend**: Updated web interface with accurate packet counts and error statistics

## v2.15.0 (MAVLink Parser Refactoring to Byte-wise Parsing) 🚀

### Major MAVLink Parser Simplification
- **Byte-wise Parsing**: Migrated from complex state machine to standard pymavlink byte-wise parsing
  - **Removed Complexity**: Eliminated stash buffer, consume logic, prefix bytes tracking
  - **Standard Approach**: Using mavlink_parse_char() for each byte - industry standard
  - **Code Reduction**: Parser reduced from ~500 to ~300 lines
  - **No More Deadlocks**: Removed PacketAssembler and incomplete packet tracking

### BulkModeDetector Implementation
- **Smart Detection**: Decay counter mechanism for MAVFtp and parameter bulk transfers
  - **Hysteresis**: ON at 20 packets/sec, OFF at 5 packets/sec
  - **100ms Decay**: Counter decrements every 100ms for smooth transitions
  - **Targeted Detection**: Only counts FILE_TRANSFER_PROTOCOL and PARAM messages
  - **Adaptive Flushing**: Immediate flush in bulk mode, batched in normal mode

### USB Batching Architecture v2 ✅ COMPLETED
- **Complete Rewrite**: Full USB sender architecture overhaul for data integrity
  - **Pending Buffer**: Added protection against partial write data loss
  - **Helper Methods**: applyBackoff(), resetBackoff(), inBackoff(), updateTxCounter(), commitPackets(), flushPending(), sendSinglePacket()
  - **4-Step Algorithm**: Linear processing - flush pending → handle partials → single packets → bulk batching
  - **N/X/T Thresholds**: 4 packets OR 448 bytes OR 5ms timeout for batch flushing
  - **Bulk Mode Transitions**: Force flush on bulk mode exit to prevent latency
- **Memory Pool Enhancement**: Increased 128B pool from 20 to 60 blocks for MAVFtp transfers
- **Queue Architecture**: Migrated from std::queue to std::deque for direct indexing and batch planning
- **Compiler Fixes**: Resolved type deduction errors in min() calls with explicit std:: namespace

### USB Block Detection & Memory Pool Protection ✅ COMPLETED
- **Blocked USB Detection**: Smart detection when COM port closed on host side
  - **Timeout-based Logic**: 500ms timeout when availableForWrite() value unchanged
  - **Memory Pool Protection**: Auto-clear all queues to prevent pool exhaustion on startup
  - **Recovery Detection**: Keep 1 test packet for automatic unblock detection
  - **Diagnostic Logging**: `[USB-DIAG] USB blocked/unblocked` messages for debugging
- **Memory-Safe Operations**: Proper packet.free() calls in clearAllQueues()
- **Startup Protection**: No more `Pool exhausted for size X, using heap` messages when USB dead
- **Enhanced enqueue()**: Override in UsbSender to limit packets during blocked state

### MAVLink Statistics Web Interface Fix ✅ COMPLETED
- **Accurate Statistics Display**: Fixed web interface showing zeros for MAVLink stats
  - **Backend Changes**: Added getSender() methods to ProtocolPipeline for sender access
  - **Real Data Sources**: packetsSent/packetsDropped now from actual USB sender statistics
  - **Proper Error Counting**: Detection errors now counted from MAVLINK_FRAMING_BAD_CRC/BAD_SIGNATURE
  - **Field Renaming**: packetsDetected → packetsParsed for clarity
- **Frontend Updates**: Redesigned renderMavlinkStats() with relevant metrics
  - **Packet Statistics Panel**: Parsed, Sent, Dropped, Errors
  - **Packet Analysis Panel**: Average Size, Size Range, Last Activity  
  - **Removed Deprecated**: Resync Events, Total Bytes, Transmitted fields
- **Data Integrity**: All statistics now show real values instead of hardcoded zeros

### Protocol Pipeline Fix
- **Critical Logic Fix**: Fixed incorrect consume order in protocol_pipeline.h
  - **Before**: Only consumed bytes when packets found (data loss!)
  - **After**: ALWAYS consume bytes first, THEN distribute packets
  - **Universal Pattern**: Works for all parsers (MAVLink, RAW, future SBUS/CRSF)

### Cleanup and Maintenance
- **Priority System Removal**: Completely removed packet priority infrastructure
  - **Deleted**: enum PacketPriority, priority field from ParsedPacket
  - **Renamed**: dropLowestPriority() → dropOldestPacket()
  - **Simplified**: All packets treated equally in FIFO queue
- **Diagnostic Marking**: Unified format for temporary diagnostics
  - **Standard Format**: `=== DIAGNOSTIC START === (Remove after MAVLink stabilization)`
  - **Easy Removal**: All diagnostic code clearly marked for future cleanup
- **TODO Addition**: Added reminder for adaptive buffer minimum size analysis

## v2.14.1 (CircularBuffer Deadlock Fix & Priority System Rollback) ✅

### CircularBuffer Critical Fix
- **tail=511 Deadlock Resolution**: Fixed critical issue where CircularBuffer would deadlock at buffer boundaries
  - **Root Cause**: When tail near buffer end (e.g., tail=459 in 512-byte buffer), only 53 linear bytes available despite 511 total bytes
  - **Solution**: Data linearization using tempLinearBuffer[296] for transparent wrap handling
  - **Impact**: MAVLink parser now gets consistent contiguous view up to 296 bytes regardless of wrap position
- **Memory Optimization**: Removed obsolete shadow buffer system
  - **Heap Savings**: 296 bytes per buffer moved from heap to BSS (better for embedded)
  - **Cleaner Architecture**: Eliminated non-functional shadow update logic from write() method
- **Simplified Consume Logic**: Replaced complex boundary jump heuristics with clean consume pattern
  - **No More Boundary Jumps**: Parser always consumes exactly bytesProcessed amount
  - **Eliminated Stuck States**: No more aggressive consume or forced boundary jump workarounds

### Priority System Rollback
- **Architecture Simplification**: Rolled back from 4-priority queue system to single FIFO
  - **Removed Complexity**: Eliminated CRITICAL/HIGH/NORMAL/BULK priority queues
  - **Single Queue**: PacketSender now uses simple std::queue for all packets
  - **Simplified Enums**: protocol_types.h reduced to PRIORITY_NORMAL only
- **Code Cleanup**: Removed all priority-specific methods and logic
  - **Parser Simplification**: MavlinkParser.getPacketPriority() now returns PRIORITY_NORMAL for all
  - **Sender Cleanup**: Removed getDropped{Bulk,Normal,Critical} methods from UartSender/UdpSender
  - **TODO Comments**: Added documentation for potential future priority reintroduction

### Performance Improvements
- **MAVFtp Stability**: File transfer now works reliably without progress rollbacks
- **Packet Loss Reduction**: From 100+ losses per session to <1%
- **CRC Error Reduction**: From 100+ CRC errors to <10 per session
- **High Speed Support**: Stable operation at 921600 baud without packet corruption

## v2.14.0 (PyMAVLink Migration & Architecture Cleanup) ✅

### MAVLink Library Migration
- **PyMAVLink Integration**: Complete migration to PyMAVLink for better compatibility and maintainability
  - **Library Replacement**: Replaced previous MAVLink implementation with standard PyMAVLink
  - **Generated Headers**: Full pymavlink integration (common + ardupilotmega dialects)
  - **Configuration**: Updated build system with proper PyMAVLink compiler flags
  - **Improved Parsing**: Enhanced MAVLink processing efficiency and reliability

### Architecture Simplification
- **Protocol Detection Removal**: Eliminated complex protocol detection system
  - **Removed Files**: mavlink_detector.h/cpp, protocol_detector.h, protocol_factory.h/cpp
  - **Direct Parser Creation**: Protocol pipeline now creates parsers directly based on configuration
  - **Simplified Logic**: PROTOCOL_MAVLINK → MavlinkParser, PROTOCOL_NONE → RawParser
- **MAVLink Function Migration**: Moved MAVLink-specific logic from pipeline to parser
  - **MavlinkParser Enhancement**: Added non-MAVLink stream detection and warnings
  - **Smart Buffering**: Parser handles invalid data gracefully with progress warnings
  - **Error Recovery**: Automatic detection of non-MAVLink streams with user guidance

### Code Structure Cleanup
- **types.h Optimization**: Cleaned protocol structure from detection artifacts
  - **Removed Fields**: detector pointer, minBytesNeeded (detection-specific)
  - **Preserved Fields**: type, stats, enabled flag for runtime control
  - **Initialization**: Proper protocol structure initialization in initBridgeContext()
- **Include Cleanup**: Removed legacy library and detector includes across codebase
- **Factory Pattern Removal**: Eliminated initProtocolDetectionFactory() and cleanup functions

### Technical Improvements
- **Memory Efficiency**: Reduced memory footprint by removing detection overhead
- **Processing Speed**: Direct parser instantiation eliminates detection delays
- **Maintainability**: Simplified codebase with clear parser responsibilities
- **Future Ready**: Clean architecture for additional protocol support

## v2.13.2 (Protocol Statistics Web Interface) ✅

### Protocol Statistics Implementation
- **Unified Protocol Statistics**: Complete web interface implementation for all protocol types
  - **RAW Protocol Statistics**: Now shows detailed chunk processing, buffer usage, and output device metrics
  - **MAVLink Protocol Statistics**: Enhanced packet detection stats with priority-based drop analysis
  - **Extensible Architecture**: Ready for future SBUS and other protocol implementations

### Web Interface Enhancements
- **Protocol Pipeline Integration**: Centralized statistics through ProtocolPipeline.appendStatsToJson()
- **Dynamic Protocol Display**: 
  - RAW mode shows "Chunks Created", buffer utilization, timing-based processing metrics
  - MAVLink mode shows "Packets Detected", detection errors, resync events with precise packet analysis
  - Automatic protocol type detection and appropriate UI rendering
- **Sender Statistics Table**: Unified output device statistics with protocol-aware drop analysis
  - MAVLink: Priority breakdown (Bulk/Normal/Critical packet drops)
  - RAW: Percentage-based drop rate calculation
  - Queue depth monitoring and maximum queue tracking

### Backend Architecture
- **ProtocolPipeline Statistics**: Added appendStatsToJson() method for unified data export
- **JSON Structure Standardization**: Consistent protocol statistics format across all protocol types
- **Memory Efficient**: Uses existing pipeline structures without additional allocations
- **ArduinoJson Integration**: Proper JSON serialization for web interface consumption

### Code Modernization
- **Eliminated Legacy Stats**: Removed old protocol statistics code from web_api.cpp (~40 lines)
- **Modular JavaScript**: Added specialized render methods for different protocol types
- **Responsive Design**: Grid-based layout adapts to different screen sizes
- **Error Handling**: Graceful fallback for unknown or future protocol types

### Critical Fixes and Optimizations
- **Fixed Protocol Statistics Crashes**: Added comprehensive null pointer checks in appendStatsToJson()
  - **Problem**: Null pointer access causing packet losses and system instability
  - **Solution**: Added checks for !ctx, !ctx->system.config before accessing pointers
  - **Result**: Eliminated crashes during statistics export in web interface
- **Fixed RAW Parser Statistics**: RAW protocol now properly updates statistics in web interface
  - **Problem**: RAW parser showed zeros for all metrics despite active data processing
  - **Solution**: Used existing updatePacketSize() and stats->reset() methods instead of creating new fields
  - **Result**: Real-time statistics display for chunk processing, bytes transmitted, size ranges
- **Memory Pool Optimization**: RAW parser now uses standard pool sizes for efficiency
  - **Problem**: Arbitrary chunk sizes (98, 103, 115 bytes) caused "Pool exhausted" warnings
  - **Solution**: Round allocation sizes to pool standards (64, 128, 288, 512 bytes) while preserving actual data size
  - **Result**: Eliminated pool warnings, improved memory efficiency, reduced heap fragmentation
- **Statistics Reset Fix**: "Reset Statistics" button now properly clears all protocol metrics
  - **Problem**: New protocol fields not reset when user clicked reset button
  - **Solution**: Enhanced reset() methods to clear all statistics fields including protocol-specific ones
  - **Result**: Complete statistics reset functionality across all protocols

## v2.13.1 (Statistics Synchronization Unification) ✅

### Statistics Architecture Refactoring
- **Unified Statistics Management**: Centralized all device statistics functions in diagnostics.cpp
  - **Problem Solved**: Eliminated duplicate statistics functions across multiple files
  - **Core 0/Core 1 Separation**: Clean separation by CPU cores for optimal performance
  - **Thread Safety**: All device statistics properly synchronized using critical sections

### Function Consolidation
- **Moved to diagnostics.cpp**:
  - `updateMainStats()` - Updates Device 1/2 statistics (Core 0 devices)
  - `updateDevice3Stats()` - Updates Device 3 statistics (Core 1 device)  
  - `updateDevice4Stats()` - Updates Device 4 statistics (Core 1 device)
- **Removed Duplicate Code**:
  - Deleted `updateMainStats()` from uartbridge.cpp
  - Deleted `updateDevice3Stats()` from device3_task.cpp
  - Removed static pointer variables no longer needed
  - Clean extern declarations in scheduler_tasks.cpp

### Architecture Benefits
- **Proper Core Distribution**:
  - Core 0: Device 1 (UART1) + Device 2 (USB/UART2) → unified in `updateMainStats()`
  - Core 1: Device 3 (UART3) + Device 4 (UDP) → separate functions for each
- **Reduced Race Conditions**: Each device's statistics updated in single location
- **Better Maintainability**: All statistics logic centralized in diagnostics module
- **Performance**: Minimal blocking between cores, optimized update intervals

### Device 4 Statistics Integration
- **BridgeContext Integration**: Added Device 4 fields to main bridge context
- **Pipeline Statistics**: UdpSender properly updates TX byte counters
- **Scheduler Tasks**: Device 4 statistics task properly restored and configured
- **Global Variables**: Maintained existing globalDevice4* variables for task communication

## v2.13.0 (Protocol Architecture Refactoring) 🚀

### Major Architecture Overhaul: Parser + Sender Pattern
- **Complete Protocol Architecture Redesign**: Implemented separation of packet parsing and transmission
  - **New Data Flow**: UART RX → CircularBuffer → Parser → PacketQueue → Sender(s) → Device(s)  
  - **Problem Solved**: Eliminated coupling between protocol detection and transmission logic
  - **Benefits**: Clean architecture, better maintainability, protocol-agnostic design

### Memory Pool Implementation
- **Slab Allocator**: Thread-safe memory pool for packet management
  - **Implementation**: `packet_memory_pool.h` with Meyers Singleton pattern
  - **Pool Sizes**: 64B (control packets), 288B (MAVLink), 512B (RAW chunks)
  - **Fallback**: Automatic heap allocation when pools exhausted
  - **Thread Safety**: C++11+ guaranteed initialization safety

### Protocol Pipeline Components
- **Parser Framework**: Base `ProtocolParser` class with specialized implementations
  - **RawParser**: Adaptive buffering without protocol parsing (default mode)
  - **MAVLinkParser**: MAVLink packet boundary detection and parsing
  - **Future Ready**: Architecture supports SBUS, CRSF protocol additions

- **Sender Framework**: Priority-based packet transmission system
  - **UsbSender**: Exponential backoff for congestion handling
  - **UartSender**: Inter-packet gap support for timing-sensitive protocols
  - **UdpSender**: Batching capabilities for network efficiency
  - **Priority Support**: CRITICAL > NORMAL > BULK packet prioritization

### Transmission Improvements
- **Backpressure Handling**: Intelligent packet dropping based on priority levels
- **Partial Send Support**: USB/UART senders handle incomplete transmissions
- **Queue Management**: Per-sender packet queues with size limits
- **Flow Control**: Ready state checking prevents buffer overflow

### Integration & Cleanup
- **Pipeline Coordinator**: `ProtocolPipeline` class manages all parsers and senders
- **Bridge Integration**: Seamless integration with existing UART bridge task
- **Legacy Removal**: Cleaned up old protocol detection functions
- **Code Quality**: All comments converted to English, consistent style

### Performance Benefits
- **Zero Copy**: Direct buffer access for parsing without data copying
- **Memory Efficient**: Pool allocation prevents heap fragmentation
- **Multi-core Ready**: Thread-safe design for ESP32 dual-core operation
- **Scalable**: Easy addition of new protocols and output devices

## v2.12.1 (Memory Optimization & Final Code Cleanup) ✅

### Memory Optimization Phase 
- **Legacy Buffer Removal**: Complete elimination of old buffer architecture
  - **Memory Saved**: 384-2048 bytes of RAM (removed duplicate buffer allocation)
  - **Code Cleanup**: Removed unused `adaptiveBuffer` allocation in `uartbridge.cpp:95-98`
  - **Architecture**: Now uses only CircularBuffer implementation
  - **Validation**: Ensured no remaining references to legacy buffer system

### bufferIndex Removal Optimization
- **Structure Cleanup**: Removed `bufferIndex` from `AdaptiveBufferContext` structure
  - **Memory Saved**: Additional 8 bytes (pointer + int) per bridge context
  - **Code Paths**: Eliminated all bufferIndex initialization and update code
  - **Critical Fix**: Updated `isBufferCritical()` in `protocol_pipeline.h:182-188`
    - **Before**: Used legacy `bufferIndex` for buffer fullness detection
    - **After**: Direct CircularBuffer availability check: `circBuf->available() >= bufferSize - 64`
  - **Validation**: Comprehensive search confirmed no remaining bufferIndex references

### Production Code Cleanup
- **Debug Code Removal**: All temporary diagnostic logging removed for production
  - **Files Cleaned**: `circular_buffer.h`, `main.cpp`, `adaptive_buffer.h`
  - **Log Reduction**: Removed verbose buffer initialization and operation logs
  - **Performance**: Cleaner log output focused on critical events only
  - **Maintenance**: Easy identification and removal of TEMPORARY DEBUG blocks

### Final Architecture State
- **Single Buffer**: CircularBuffer as the only data buffer implementation
- **Zero Waste**: No duplicate memory allocations or unused legacy code
- **Clean Codebase**: Production-ready with minimal diagnostic output
- **Memory Efficient**: Optimized RAM usage through complete legacy removal

## v2.12.0 (Circular Buffer Implementation & Adaptive Thresholds) 🚀

### Major Architecture Changes
- **Circular Buffer Migration**: Replaced legacy linear buffer with modern circular buffer
  - **Problem**: Old buffer used inefficient memmove operations causing data corruption
  - **Solution**: Zero-copy circular buffer with scatter-gather I/O
  - **Implementation**:
    - New `CircularBuffer` class in `src/circular_buffer.h`
    - Power-of-2 sizing for bitwise masking (256-2048 bytes)
    - Shadow buffer (296 bytes) for protocol parsing continuity
    - Thread-safe with FreeRTOS portMUX locks
  - **Critical Fix**: Added missing `initAdaptiveBuffer()` call in `uartbridge.cpp`
    - BridgeContext now properly initializes CircularBuffer
    - Fixed nullptr issue preventing data transmission

### Adaptive Traffic Detection
- **Gap-based Traffic Detector**: Automatic mode switching based on data patterns
  - **Normal Mode**: Standard telemetry (2ms timeout, 10% buffer threshold)
  - **Burst Mode**: Parameter downloads (5ms timeout, 20% buffer threshold)
  - **Detection Logic**:
    - Fast data: <1ms gaps between bytes
    - Burst trigger: 100+ consecutive fast bytes
    - Burst end: >5ms pause in data stream
  - **Benefits**: Optimizes for both low-latency telemetry and bulk transfers

### USB Bottleneck Handling
- **Smart Drop Strategy**: Prevents complete system hangs
  - **Thresholds** (percentage-based, scales with buffer size):
    - Warning: 85% (small), 70% (medium), 60% (large)
    - Critical: 90% (small), 75% (medium), 65% (large)
  - **Drop Policy**:
    - Warning level: Drop 12.5% after 5 retry attempts
    - Critical level: Drop 33% immediately
  - **Improvement**: Better than old system which could hang indefinitely

### Performance Optimizations
- **Transmission Thresholds**:
  - Normal: 128 bytes max (low latency priority)
  - Burst: 200 bytes max (optimized for single MAVLink packet)
- **Buffer Sizes** (auto-calculated based on baudrate):
  - 115200: 288 bytes (384 with protocol)
  - 230400: 768 bytes
  - 460800: 1024 bytes
  - 921600: 2048 bytes

### Diagnostic Enhancements
- **Comprehensive Debug Logging** (all marked TEMPORARY for easy removal):
  - Buffer initialization and allocation
  - Write operations and availability
  - Transmission triggers with mode indication
  - USB status and bottleneck events
  - Segment information for scatter-gather
- **Traffic Statistics**:
  - Total bursts detected
  - Maximum burst size
  - Drop events and bytes

### TODO.md Updates
- Added Circular Buffer Optimizations (v2.6.x)
- Added Protocol-aware Optimizations (v2.7.0)
- Added USB Optimizations (v2.7.x)

### Testing Recommendations
1. **Low Traffic Test**: Normal telemetry should show "NORMAL mode"
2. **High Traffic Test**: Parameter downloads should trigger "BURST mode"
3. **USB Bottleneck Test**: Monitor for controlled drops vs hangs
4. **Success Metrics**:
   - Packet loss <5% (from 32+ events)
   - CRC errors <20 (from 56)
   - No transmission freezes
   - Successful MAVLink parameter downloads

## v2.11.0 (Project Restructuring & MAVLink Fix) ✅
- **Project Structure Reorganization**: Improved code organization with dedicated folders
  - **Created subfolder structure**: Organized related modules into logical directories
    - `src/web/` - Web interface modules (web_api, web_interface, web_ota)
    - `src/usb/` - USB implementation files (usb_device, usb_host, interfaces)
    - `src/devices/` - Device tasks and initialization (device3_task, device4_task, device_init)
    - `src/wifi/` - WiFi management (wifi_manager)
    - `src/uart/` - UART and flow control (uart_dma, uartbridge, flow_control)
  - **Updated all include paths**: Fixed 30+ files with new relative paths
    - Files within folders use local includes (`"file.h"`)
    - Cross-folder includes use relative paths (`"../folder/file.h"`)
    - Main files use folder paths (`"folder/file.h"`)
  - **Benefits**:
    - Better code navigation and maintainability
    - Clearer module boundaries and dependencies
    - Easier to locate related functionality
    - Reduced clutter in src/ root directory

- **MAVLink Parser Critical Fix**: Resolved packet loss and CRC errors
  - **Root Cause**: Parser state corruption from buffer operations
  - **Solution**: Independent frame buffer approach with improved isolation
  - **Implementation**: Enhanced parser with stable buffer management and simplified error handling
  - **Results**:
    - **Before**: 547 packet losses, 349 CRC errors, 13314 detection errors
    - **After**: <1% packet loss, <10 CRC errors, <100 detection errors
    - MAVFtp file transfers now work reliably
    - Eliminated configuration-related parsing errors
  - **Code Cleanup**:
    - Removed `#include <map>` and all std::map usage
    - Deleted methods: `updateDetailedStats()`, `checkSequenceLoss()`, `logPacketInfo()`
    - Simplified to basic error counters for diagnostics
    - Reduced memory footprint and complexity

## v2.10.2 (USB Backpressure Reversion) ✅
- **USB Backpressure Simplification**: Reverted complex behavioral detection to simple approach
  - **Removed Features**: Complex behavioral port state detection system
    - **Behavioral detection logic** removed from `src/usb_device.cpp`
    - **Dynamic thresholds** (ASSUME_CLOSED_THRESHOLD, FIRST_ATTEMPT_THRESHOLD) eliminated
    - **Port state tracking** and related statistics removed
    - **Automatic protocol detector reset** on port changes disabled
  - **Root Cause**: Stability issues and false positives on different systems
    - **False port closure detection** causing unnecessary data drops
    - **System-dependent behavior** making solution unreliable
    - **Over-complexity** for marginal benefit in real-world usage
  - **Current Implementation**: Simple Arduino Serial backpressure
    - **Basic `Serial.availableForWrite()`** check retained
    - **Buffer overflow prevention** still functional
    - **Improved stability** across different USB host systems
    - **Simpler debugging** without complex state machine
  - **Decision Rationale**:
    - Simple solution proved more reliable than complex behavioral detection
    - Arduino Serial backpressure sufficient for most use cases
    - Eliminated source of platform-specific issues
  - **Future Consideration**: May revisit with simpler heuristics if specific need arises

## v2.10.1 (Configuration Cleanup) ✅
- **USB Configuration Cleanup**: Removed unsupported USB Auto mode
  - **Code Cleanup**: Removed USB_MODE_AUTO enum value and all related handling code
  - **Web Interface**: Removed "Auto (not supported)" option from USB mode selection
  - **Configuration**: Simplified USB mode to only Device and Host options
  - **Implementation**: Deleted UsbAuto class and createUsbAuto factory function
  - **Benefits**: Cleaner codebase, no misleading configuration options

## v2.10.0 (USB Buffer Overflow Prevention) ✅
- **USB Buffer Overflow Prevention**: Behavioral port state detection to prevent buffer overflow
  - **Behavioral Port Detection** (`src/usb_device.cpp`): Smart detection of USB port state without relying on unavailable APIs
    - **Buffer monitoring**: Tracks write buffer availability patterns to detect port closure
    - **Adaptive thresholds**: ASSUME_CLOSED_THRESHOLD (20) and FIRST_ATTEMPT_THRESHOLD (5) for optimal response
    - **Honest API**: Returns 0 when unable to write, allowing caller to manage data freshness
    - **Automatic recovery**: Seamless transition when port opens/closes
  - **Early Data Dropping** (`src/adaptive_buffer.h`): Prevents accumulation of stale data
    - **Connection state tracking**: Monitors USB port state changes in real-time
    - **Protocol detector reset**: Automatic reset when port reopens to prevent packet fragmentation
    - **Buffer cleanup**: Immediate buffer clearing on port state transitions
    - **Diagnostic integration**: Proper statistics tracking for dropped data
  - **Improved Diagnostics**: Enhanced meaning of UART FIFO overflow messages
    - **Real issues only**: FIFO overflow now indicates actual USB performance problems when port is open
    - **Reduced log spam**: Eliminated false overflow warnings when port is closed
    - **Better troubleshooting**: Clear distinction between port closure and performance issues
  - **Real-time Performance**: Optimized for time-critical applications
    - **Fast detection**: ~20-200ms response time for port closure detection
    - **Fresh data guarantee**: Only current data transmitted after port reopens
    - **Zero stale data**: Prevents transmission of outdated information

## v2.9.5 (Critical Memory Safety & Logging Refactoring) ✅
- **Critical Memory Safety Fixes**: Eliminated heap corruption and segmentation faults
  - **Adaptive Buffer Protection** (`src/adaptive_buffer.h`): Fixed buffer underflow and bounds checking
    - **Underflow prevention**: Added validation for `totalConsumed > bufferIndex` before size_t arithmetic
    - **Bounds checking**: Comprehensive validation before all memmove operations
    - **Safe state recovery**: Automatic buffer reset on detection of invalid states
    - **Partial transmission safety**: Enhanced partial data transmission with proper boundary checks
  - **Protocol Pipeline Hardening** (`src/protocols/protocol_pipeline.h`): Fixed MAVLink detection crashes
    - **Pointer validation**: Added bounds checking before `detector->canDetect()` calls
    - **Search window protection**: Prevented buffer overruns during protocol packet search
    - **Packet size validation**: Limited packet sizes to reasonable bounds (max 512 bytes)
    - **Diagnostic logging**: Added periodic state dumps for debugging protocol issues
  - **MAVLink Detector Improvements** (`src/protocols/mavlink_detector.h/cpp`): Enhanced error recovery
    - **Increased search window**: Expanded from 64 to 300 bytes for better resync capability
    - **Improved resync logic**: More efficient recovery from garbage data streams
    - **Enhanced error handling**: Better progress through invalid data sequences
  - **Stability Impact**: Eliminated MAVFtp connection drops and system crashes
    - **Zero heap corruption**: Fixed root cause of ESP32 reboots during MAVLink processing
    - **Reliable protocol detection**: MAVLink packets now consistently detected and processed
    - **Stable network operations**: MAVFtp parameter uploads work without interruption

- **Logging System Refactoring**: Complete elimination of heap fragmentation
  - **Printf-Style Migration**: Migrated all ~306 log_msg calls across 21 files
    - Replaced String concatenation (`"Text " + String(var)`) with printf format (`"Text %d", var`)
    - Unified logging interface: `log_msg(LOG_LEVEL, "format", args)`
    - Added compile-time format string validation with `__attribute__((format(printf, 2, 3)))`
  - **Memory Optimization**: Eliminated heap allocation in logging hot paths
    - **Stack-based formatting**: All log messages use 256-byte stack buffers
    - **Zero heap fragmentation**: Removed String object creation in critical paths
    - **Hot path protection**: Adaptive buffer and protocol pipeline logging optimized
  - **Performance Impact**: Significant stability improvement for ESP32
    - **Reduced RAM usage**: Eliminated hundreds of String allocations per second
    - **Lower fragmentation**: ESP32 heap stays healthier during intensive operations
    - **Better reliability**: Reduced risk of memory-related crashes during MAVLink processing
  - **Code Cleanup**: Removed deprecated logging functions
    - Deleted old `log_msg(String, LogLevel)` and `log_msg(const char*, LogLevel)` functions
    - Single unified printf-style logging function for entire codebase
    - Improved code maintainability and consistency

## v2.9.0 (MAVLink Protocol Implementation) ✅
- **MAVLink Protocol Detector** - Phase 4.2 Complete
  - **Core MAVLink Implementation**: Full MAVLink v1/v2 packet detection
    - Created `src/protocols/mavlink_detector.h/cpp` with complete implementation
    - MAVLink packet boundary detection with header validation and payload size calculation
    - Support for MAVLink v1 (0xFE) and v2 (0xFD) protocol versions
    - Error handling with next start byte search for packet recovery
  - **Protocol Statistics System**: Comprehensive performance tracking
    - Created `src/protocols/protocol_stats.h` for real-time metrics
    - Tracks detected packets, transmitted packets, error count, resync events
    - Packet size statistics (average, min, max) and transmission rates
    - Integration with web interface for live protocol monitoring
  - **Configuration Management**: Seamless integration with existing config system
    - Added `protocolOptimization` field with version migration (v6→v7)
    - Protocol factory system with `src/protocols/protocol_factory.h/cpp`
    - Web interface dropdown for protocol selection (None/MAVLink)
    - Configuration persistence and loading across reboots
  - **Web Interface Enhancements**: User-friendly protocol management
    - Protocol Optimization dropdown in main interface (after UART Configuration)
    - Collapsible Protocol Statistics section with real-time updates
    - Statistics display: packets, errors, sizes, rates, last packet timing
    - Form integration with save/load functionality
  - **Performance Impact**: Dramatic latency reduction for MAVLink streams
    - **Eliminates UART FIFO overflows** at 115200 baud with high-speed MAVLink data
    - **Instant packet transmission** upon boundary detection (no timeout delays)
    - **Perfect packet preservation** - no data loss or fragmentation
    - Compatible with adaptive buffering fallback for non-MAVLink data
  - **HTML Minification Fix**: Improved web interface readability
    - Disabled aggressive HTML minification to preserve text spacing
    - Maintained gzip compression for optimal size (70-78% reduction)
    - Preserved JavaScript/CSS minification for performance

## v2.8.3 (Protocol Detection Framework) ✅
- **Protocol Detection Infrastructure** - Phase 4.1 Complete
  - **Framework Architecture**: Extensible protocol detection system
    - Created `src/protocols/` directory with base classes and interfaces
    - Implemented `ProtocolDetector` interface for protocol-specific implementations
    - Added comprehensive protocol pipeline with processing hooks
  - **Data Structure Extensions**: Enhanced core structures for protocol support
    - Added `ProtocolType` enum with support for MAVLink, Modbus RTU, NMEA, Text Line, SBUS
    - Extended `BridgeContext` with protocol detection state and error tracking
    - Added `protocolOptimization` configuration field with PROTOCOL_NONE default
  - **Integration Points**: Protocol-aware data processing
    - Integrated protocol hooks in `bridge_processing.h` for byte-level processing
    - Enhanced `adaptive_buffer.h` with protocol-specific transmission logic
    - Added protocol lifecycle management in `uartbridge.cpp`
  - **Stub Implementation**: Complete framework with zero functional impact
    - All protocol functions implemented as safe stubs returning default values
    - Zero performance overhead when disabled (PROTOCOL_NONE default)
    - Full backward compatibility with existing adaptive buffering
  - **Benefits**: Ready infrastructure for Phase 4.2+ protocol implementations
    - Easy addition of new protocols without architectural changes
    - Clear separation of concerns between framework and protocol logic
    - Prepared for future protocol-specific optimizations

## v2.8.2 (WiFi Manager ESP-IDF Migration) ✅
- **Complete WiFi Manager Migration** - Completed
  - **Full ESP-IDF Implementation**: Migrated from Arduino WiFi API to native ESP-IDF
    - Replaced WiFi.h with esp_wifi.h, esp_event.h, esp_netif.h
    - Event-driven architecture using ESP-IDF event handlers
    - Native WiFi configuration and state management
    - Eliminated temporary AP appearance in Client mode (main issue resolved)
  - **mDNS Service Implementation**: Device discovery by hostname in Client mode
    - Added mdns.h support with CONFIG_MDNS_ENABLED=y
    - Generated unique hostnames using device name + MAC suffix (2 bytes)
    - Safe mDNS initialization outside event handler context
    - HTTP service advertisement for web interface discovery
  - **DHCP Hostname Configuration**: Proper device identification in router
    - Implemented esp_netif_set_hostname() before WiFi start
    - Router now displays custom device name instead of "espresiff"
    - Hostname generation with MAC uniqueness for multiple devices
  - **Enhanced Error Handling**: Robust connection management and recovery
    - Bootloop protection with safe mode after 3 WiFi init failures
    - Proper memory checks before WiFi initialization
    - Connection timeout handling and automatic retry logic
    - Scan failure recovery with WiFi subsystem reset

## v2.8.1 (WiFi Client Mode Stability Fixes) ✅
- **WiFi Client Connection Logic** - Major stability improvements
  - **AP Mode Conflict Resolution**: Fixed dual AP+Client mode issue causing network conflicts
    - Added ESP-IDF level WiFi initialization with forced STA-only mode 
    - Implemented double AP disable: at client start and after successful connection
    - Eliminated unwanted AP broadcast during client connection process
  - **Intelligent Connection Management**: Enhanced retry logic with proper error distinction
    - **Initial Connection**: Scans every 15 seconds when network not found
    - **Connection Attempts**: Up to 5 attempts when target network is available  
    - **Authentication Failure**: Stops after 5 failed attempts to prevent router lockout
    - **Network Loss Recovery**: Unlimited reconnection attempts after successful initial connection
    - **Error State Handling**: Clear distinction between "wrong password" and "network not found"
  - **LED Indication Accuracy**: Corrected LED behavior for all connection states
    - Orange slow blink (2s): Searching for configured network
    - Orange solid: Successfully connected to WiFi network
    - Red fast blink (500ms): Authentication failed (wrong password) - restart required
    - Purple solid: AP mode active for direct configuration
  - **Performance Optimization**: Eliminated unnecessary scanning after successful connection
    - Fixed continuous scanning loop when already connected
    - Improved WiFi manager process state handling
- **Code Quality Improvements**
  - **ESP-IDF Integration**: Hybrid approach using ESP-IDF for initialization, Arduino for operations
- **Documentation Updates**
  - **README.md**: Comprehensive WiFi Client section with detailed connection logic
  - **Help Page**: Updated HTML help with key WiFi Client information and troubleshooting
  - **Troubleshooting**: Enhanced problem resolution guide with specific LED state meanings

## v2.8.0 (WiFi Client Mode Implementation) ✅
- **WiFi Client Mode** - Full implementation completed
  - **Dual WiFi Modes**: Support for both Access Point (AP) and Client (STA) modes
  - **WiFi Manager**: Complete state machine with scanning, connecting, and error handling
  - **Triple Click Logic**: Enhanced button logic for mode switching between Standalone/Client/AP
  - **LED Indication System**: 
    - Orange slow blink during network scanning (2s interval)
    - Solid orange when connected as WiFi client
    - Fast red blink for connection errors (500ms interval)
    - White blink for button click feedback in Client mode
  - **Device 4 Network Awareness**: Proper WiFi connection waiting using EventGroup synchronization
  - **Temporary/Permanent Modes**: Session-based temporary modes and persistent configuration
  - **Configuration**: Full web interface integration for WiFi Client credentials
  - **NVS Optimization**: Fixed preferences key length limitations (temp_net_mode)
  - **Benefits**:
    - Connect to existing WiFi networks while maintaining AP functionality
    - Seamless mode switching without configuration loss
    - Visual feedback for all WiFi states and user interactions
    - Reliable network operations with proper synchronization

## v2.7.3 (Configuration Import/Export + UI Improvements) ✅
- **Configuration Import/Export** - Completed
  - **Export Configuration**: Download current config as JSON file with unique filename
  - **Import Configuration**: Upload JSON config file with validation and auto-reboot  
  - **Web Interface**: Added "Configuration Backup" section with Export/Import buttons
  - **Async Reboot Fix**: Resolved reboot timing issues using TaskScheduler for delayed restart
  - **UI/UX Improvements**: Proper status messages, progress bars, and error handling
  - **Benefits**:
    - Quick device provisioning and configuration backup/restore
    - Share configurations between devices  
    - No need to reconfigure after firmware updates
    - Reliable status display without "Failed to fetch" errors

## v2.7.2 (Device 3/4 Code Refactoring) ✅
- **Device 3/4 Module Separation** - Completed
  - **File Structure**: Created independent device3_task.cpp/h and device4_task.cpp/h modules
  - **Code Reduction**: Reduced uartbridge.cpp from 600+ to 240 lines (360+ lines moved)
  - **Architecture Improvement**: Clear separation of device responsibilities for better maintainability
  - **Dependencies**: Updated all cross-file includes and extern declarations
  - **Linker Fixes**: Resolved device3Serial definition conflicts between modules
  - **Benefits**:
    - Easier maintenance and development
    - Reduced file complexity and improved readability
    - Clear module boundaries for future expansion
    - Preserved all existing functionality without changes

## v2.7.1 (Device 3/4 Statistics Unification) ✅
- **Statistics Thread Safety** - Completed
  - **Device 4 Protection**: Added critical sections for all TX/RX statistics updates
  - **Device 3 Migration**: Moved from local variables to global variables approach
  - **Race Condition Fixes**: Eliminated multi-threaded statistics conflicts
  - **Unified Architecture**: Consistent statistics handling across Device 3 and Device 4
  - **TaskScheduler Integration**: Implemented updateDevice3Stats() for periodic updates
  - **Benefits**:
    - Thread-safe statistics under high data load
    - Reliable counters without "jumping" values
    - Maintainable unified codebase
    - Stable operation in concurrent scenarios

## v2.7.0 (Device 4 Network Implementation) ✅
- **Device 4 Network Functionality** - Completed
  - **Network Logger Mode**: Send system logs via UDP (broadcast or unicast)
  - **Network Bridge Mode**: Bidirectional UART<->UDP communication
  - **Configuration**: Full web interface integration with IP/Port settings
  - **Statistics**: Real-time TX/RX bytes and packet counters
  - **Architecture**:
    - AsyncUDP library for network operations
    - Ring buffers for non-blocking data transfer
    - Task runs on Core 1 with web server
    - DMA-style approach for efficient data handling
  - **Integration**:
    - Device 4 configuration saved/loaded with config version 5
    - Network Log Level enabled when Device 4 is active
    - Full statistics in web interface status display
    - Works in both standalone and network modes
  - **Technical Details**:
    - 2KB buffers for logs and bridge data
    - 50ms task delay for low latency
    - Mutex protection for thread-safe buffer access
    - Forward declarations to resolve compilation order
  - **Fixed Issues**:
    - Web interface now correctly displays selected Device 4 role
    - Removed duplicate role name transmission in config JSON

## v2.6.0 (ESPAsyncWebServer Migration) ✅
- **Migrated to ESPAsyncWebServer** - Completed
  - **Libraries**: Updated to ESPAsyncWebServer v3.7.10 + AsyncTCP v3.4.5
  - **Template System**: Changed from custom `{{}}` to built-in `%PLACEHOLDER%` processor
  - **API Migration**: Converted all handlers to async (request parameter access, response sending)
  - **OTA Adaptation**: Redesigned file upload handling for async server
  - **Architecture**: Removed webServerTask - AsyncWebServer works without dedicated task
  - **Memory Benefits**: Better resource usage, no blocking operations
  - **Performance**: Non-blocking request handling, improved concurrent connections
  - **JavaScript Fixes**: Fixed Reset Statistics and Clear Crash History button handling
  - **Diagnostics**: Enhanced stack monitoring for WiFi/TCP tasks instead of web server task

## v2.5.8 (Permanent Network Mode) ✅
- **Permanent Network Mode Implementation** - Completed
  - Added `permanent_network_mode` configuration parameter in Config structure
  - Updated configuration version from 3 to 4 with automatic migration
  - Web interface: Added checkbox in WiFi Configuration section
  - Backend: Full configuration save/load support with proper validation
  - Boot logic: Automatic permanent network mode detection in `detectMode()`
  - Benefits:
    - Wi-Fi access point remains active until manually disabled
    - No timeout for permanent mode (only for triple-click temporary mode)
    - Configurable via web interface with clear user feedback
    - Backward compatible with existing configurations

- **Build System Enhancement** - Completed
  - Added `update_readme_version.py` script for automatic version synchronization
  - Script automatically updates README.md badge with version from defines.h
  - Integrated into PlatformIO build process as pre-build script
  - Prevents version mismatches between firmware and documentation
  - **Fixed**: Removed blocking `exit(0)` that prevented compilation and upload

## v2.5.7 (Device Init Refactoring) ✅
- **Refactor Device Initialization** - Completed
  - Migrated `uartbridge_init()` from `uartbridge.cpp` to `device_init.cpp`
  - Renamed `uartbridge_init()` to `initMainUART()` for consistency
  - Migrated `initDevices()` from `main.cpp` to `device_init.cpp`
  - Updated all function calls in `main.cpp` (2 occurrences)
  - Removed function declarations from original locations
  - Benefits:
    - Better code organization with all device init functions in one module
    - Consistent naming convention for initialization functions
    - Reduced file sizes: uartbridge.cpp (~55 lines), main.cpp (~30 lines)
  - Technical details:
    - Added necessary includes: `usb_interface.h`, `flow_control.h`, `freertos/semphr.h`
    - Made `g_usbInterface` global (was static) for cross-module access
    - Added `diagnostics.h` include for helper functions
    - Total lines moved: ~85 lines to device_init module

## v2.5.6 (Bridge Mode Renaming) ✅
- **Rename Device Modes to Bridge Modes** - Completed
  - Renamed `DeviceMode` enum to `BridgeMode`
  - Renamed `MODE_NORMAL` to `BRIDGE_STANDALONE`
  - Renamed `MODE_CONFIG` to `BRIDGE_NET`
  - Updated all references across 11 source files
  - Benefits:
    - Clear separation of bridge functionality vs network state
    - Future-ready for Device 4 permanent network modes
    - Better naming that accurately describes functionality
    - Flexible design supporting different network configurations

- **Update SystemState Structure** - Completed
  - Renamed `wifiAPActive` to `networkActive`
  - Renamed `wifiStartTime` to `networkStartTime`
  - Added `isTemporaryNetwork` flag for future Device 4 support
  - Allows differentiation between setup AP (temporary) and permanent network modes

- **Update All User-Visible Text** - Completed
  - Changed "normal mode" to "standalone mode" throughout
  - Changed "config mode" to "network mode" throughout
  - Changed "WiFi configuration" to "network setup" where appropriate
  - Simplified to just "Network Mode" (without "setup") for universal usage
  - Updated help documentation and web interface

- **Implementation Details** - Completed
  - Total changes: ~60 occurrences across 11 files
  - Added critical comment for `_TASK_INLINE` macro in scheduler_tasks.h
  - All mode checks updated to use new enum values
  - Function names updated (initNormalMode → initStandaloneMode, etc.)
  - TaskScheduler functions renamed (enableRuntimeTasks → enableStandaloneTasks)
  - Network timeout only active when `isTemporaryNetwork=true`

## v2.5.5 (Adaptive Buffer Optimization) ✅
- **Fix FIFO Overflow at 115200 baud** - Completed
  - Identified root cause: increased buffer size (512 bytes) causing USB bottleneck
  - Created graduated buffer sizing for smoother performance:
    - < 115200: 256 bytes (original size)
    - 115200: 288 bytes (optimal compromise)
    - 230400: 768 bytes
    - 460800: 1024 bytes
    - 921600+: 2048 bytes
  - Results at 115200 baud:
    - Reduced FIFO overflow from constant to rare (3-8 times per test)
    - Improved packet loss from 25% to 2-10%
    - Parameter download success rate: 83% (5 out of 6 attempts)
  - Determined 288 bytes as optimal for 115200 baud without flow control

- **Code Cleanup** - Completed
  - Fixed duplicated adaptive buffering code in `bridge_processing.h`
  - Now properly uses functions from `adaptive_buffer.h`
  - Removed ~55 lines of duplicated code
  - Added temporary diagnostics (marked for removal after testing)

## v2.5.4 (TaskScheduler Implementation) ✅
- **Implement TaskScheduler** - Completed
  - Added TaskScheduler library to replace manual timer checks
  - Created `scheduler_tasks.cpp/h` for centralized task management
  - Migrated all periodic tasks (diagnostics, statistics, LED updates)
  - Saved ~100 lines of timer management code
  - Benefits achieved:
    - Cleaner code structure
    - Easy to add/remove periodic tasks
    - Better timing accuracy
    - Reduced cognitive load
  - All tasks properly distributed in time to prevent simultaneous execution
  - Mode-specific task management (Runtime vs Setup modes)

## v2.5.3 (Phase 2 Code Refactoring - Hybrid Approach) ✅
- **Phase 2 Refactoring - Hybrid Approach** - Completed
  - Further refactored uartbridge.cpp from ~555 to ~250 lines
  - Created BridgeContext structure for clean parameter passing
  - Maintained performance with static inline functions
  
- **Extended types.h** - Completed
  - Added comprehensive BridgeContext structure
  - Groups all task-local variables logically
  - Added inline initBridgeContext() function
  - Forward declarations for UartInterface/UsbInterface
  
- **Enhanced Diagnostics Module** - Completed
  - Moved periodic diagnostics from uartbridge.cpp
  - Added logBridgeActivity(), logStackDiagnostics()
  - Added logDmaStatistics(), logDroppedDataStats()
  - Created updatePeriodicDiagnostics() main function
  - ~150 lines moved to diagnostics module
  
- **Created bridge_processing.h** - Completed
  - All data processing logic as inline functions
  - processDevice1Input(), processDevice2USB/UART()
  - processDevice3BridgeInput()
  - shouldYieldToWiFi() helper
  - ~200 lines of processing logic extracted
  
- **Created adaptive_buffer.h** - Completed
  - All adaptive buffering logic in one place
  - shouldTransmitBuffer(), transmitAdaptiveBuffer()
  - handleBufferTimeout()
  - calculateAdaptiveBufferSize() moved from device_init
  - ~120 lines of buffering logic extracted
  
- **Simplified Main Loop** - Completed
  - Clean, readable main loop with function calls
  - All complex logic in specialized modules
  - Performance preserved with inline functions
  - Ready for future extensions

## v2.5.2 (Phase 1 Code Refactoring) ✅
- **Refactor Large uartbridge.cpp File** - Phase 1 Complete
  - Original size: ~700 lines, reduced to ~555 lines
  - Created modular structure without breaking functionality
  
- **Flow Control Module** - Completed
  - Created `flow_control.h` and `flow_control.cpp`
  - Extracted `detectFlowControl()` and `getFlowControlStatus()` functions
  - ~50 lines of code moved to separate module
  - Clean separation of flow control logic

- **Device Initialization Module** - Completed
  - Created `device_init.h` and `device_init.cpp`
  - Extracted `initDevice2UART()` and `initDevice3()` functions
  - ~85 lines of code moved to separate module
  - Fixed linkage issues with proper extern declarations

- **Build System Verification** - Completed
  - Fixed static/extern conflicts for device2Serial
  - Added proper extern declarations in uartbridge.h
  - Project compiles successfully with new structure
  - All functionality preserved

## v2.5.1 (Web Interface Modularization) ✅
- **Split main.js into modules** - Completed
  - Created separate JS modules for better organization:
    - `utils.js` - Common utility functions
    - `device-config.js` - Device configuration UI
    - `form-utils.js` - Form handling and validation
    - `status-updates.js` - Status and statistics updates
    - `crash-log.js` - Crash log handling
    - `main.js` - Main initialization and coordination
  - Updated `web_interface.cpp` to serve all JS files
  - Updated `embed_html.py` to process all JS files
  - Added proper handlers in `web_interface.h`

- **Fix Reset Statistics button** - Completed
  - Removed confirmation dialog as requested
  - Fixed button state stuck on "Resetting..."
  - Button now properly returns to "Reset Statistics" after operation

- **OTA Update Safety** - Completed
  - Added Device 3 UART0 deinitialization before OTA update
  - Prevents conflicts during firmware update when Device 3 uses UART0
  - Made `device3Serial` accessible from `web_ota.cpp`
  - Ensures clean OTA update even with active Device 3

- **Code Cleanup** - Completed
  - Removed unused `lastWdtFeed` variable from `uartbridge.cpp`
  - Fixed legacy code remnants

## v2.5.0 (Complete ESP-IDF Migration + Performance) ✅
- **Complete ESP-IDF Migration** - Completed
  - Migrated Device 2 UART to ESP-IDF/DMA ✅
  - Migrated Device 3 UART to ESP-IDF/DMA ✅
  - Migrated UART logger to ESP-IDF/DMA ✅
  - Removed HardwareSerial dependencies completely ✅
  - Removed serial_config_decoder.h ✅

- **DMA Polling Mode Implementation** - Completed
  - Added polling mode support to UartDMA class ✅
  - Device 2/3 use polling mode (no event tasks) ✅
  - Added `pollEvents()` method for non-blocking operation ✅
  - Optimized for minimal resource usage ✅

- **Configuration Architecture Improvement** - Completed
  - Created `UartConfig` structure for UART parameters ✅
  - Separated DMA configuration from UART configuration ✅
  - Fixed scope conflicts with global config ✅
  - Clean parameter passing without global dependencies ✅

- **Dynamic Adaptive Buffer Sizing** - Completed
  - Buffer size now adjusts based on baud rate (256-2048 bytes) ✅
  - 256 bytes for ≤19200 baud
  - 512 bytes for ≤115200 baud
  - 1024 bytes for ≤460800 baud
  - 2048 bytes for >460800 baud
  - Prevents packet fragmentation at high speeds
  - Maintains low latency at all speeds

## v2.4.0 (ESP-IDF Migration) ✅
- **Remove Arduino Framework Dependencies for Device1** - Completed
  - Migrated Device 1 UART to ESP-IDF driver with DMA
  - Removed all conditional compilation (#ifdef USE_UART_DMA)
  - Deleted uart_arduino.cpp wrapper
  - Improved UART performance with hardware packet detection

- **Configuration System Update** - Completed
  - Updated Config struct to use ESP-IDF native types (uart_word_length_t, uart_parity_t, uart_stop_bits_t)
  - Increased config version to 3 with automatic migration
  - Added conversion functions between ESP-IDF enums and web interface strings

- **Code Cleanup and Optimization** - Completed
  - Created UsbBase class to eliminate code duplication between USB Host and Device
  - Simplified UartInterface with direct ESP-IDF configuration
  - Removed serial_config_decoder.h dependency for Device 1
  - Improved error handling and logging

- **Performance Improvements** - Completed
  - Hardware DMA for Device 1 UART with packet boundary detection
  - Zero-copy ring buffer implementation
  - Reduced CPU usage through event-driven architecture
  - Native ESP-IDF drivers for better performance
  - Increased DMA buffers and task priority for high-throughput scenarios

- **USB Implementation Decision** - Completed
  - Kept Arduino Serial for USB Device (stable and sufficient)
  - USB Host already uses ESP-IDF
  - Created common base class for code reuse
  - USB performance not critical compared to UART

## v2.3.3 (Performance Optimization) ✅
- **UART Bridge Performance Fix** - Completed
  - Fixed packet drops during MAVLink parameter downloads
  - Optimized device role checks with cached flags
  - Removed repeated config.deviceX.role comparisons in critical loop
  - Improved performance at 115200 baud and higher

- **Buffer Size Optimization** - Completed
  - Increased UART_BUFFER_SIZE from 256 to 1024 bytes
  - Added Serial.setTxBufferSize(1024) for USB interface
  - Removed baudrate condition - always use 1KB buffers
  - Reduced WiFi yield time from 5ms to 3ms

## v2.3.2 (Web Interface Migration) ✅
- **Web Interface Refactoring** - Completed
  - Migrated from embedded HTML strings to separate HTML/CSS/JS files
  - Created build-time processing with `embed_html.py` script
  - Reduced placeholders from 40+ to just {{CONFIG_JSON}}
  - Separated JavaScript into main.js and crash-log.js
  - HTML/CSS/JS files now editable with proper IDE support
  - C++ code reduced by ~3000 lines

## v2.3.1 ✅
- **Update Statistics System** - Completed
  - Replaced old `bytesUartToUsb`/`bytesUsbToUart` with per-device counters
  - Added `device1RxBytes`/`device1TxBytes`, `device2RxBytes`/`device2TxBytes`, `device3RxBytes`/`device3TxBytes`
  - Updated web interface to show statistics for all active devices
  - Shows device roles in statistics display

- **Code Cleanup** - Completed
  - Moved `updateSharedStats` and `resetStatistics` to diagnostics module
  - Removed legacy `if (currentMode == MODE_NORMAL || currentMode == MODE_CONFIG)` check
  - Fixed deprecated ArduinoJson `containsKey()` warning
  - Added helper functions `getDevice2RoleName()` and `getDevice3RoleName()`

- **Web Interface Updates** - Completed
  - Updated traffic statistics table to show per-device data
  - Dynamic show/hide of device rows based on active roles
  - Updated JavaScript to handle new JSON structure from `/status` endpoint
  - Fixed "Never" display for last activity

## v2.3.0 ✅
- **Remove DEBUG_MODE** - Completed
  - Removed all DEBUG_MODE checks from code
  - Bridge always active in all modes
  - Diagnostics converted to log levels
  
- **Remove CONFIG_FREERTOS_UNICORE** - Completed
  - Now only supports multi-core ESP32
  - UART task on core 0, Web task on core 1
  
- **Code Organization** - Completed
  - Extracted diagnostics to separate module (diagnostics.cpp/h)
  - Moved system utilities to system_utils.cpp/h
  - Moved RTC variables and crash update to crashlog.cpp/h
  - main.cpp reduced from ~600 to ~450 lines



## Current Status Summary

The project now uses a full ESP-IDF approach for all UART operations and modern AsyncWebServer:
- **Device 1 (Main UART)**: Full ESP-IDF with DMA and event task ✅
- **Device 2 (Secondary)**: ESP-IDF with DMA polling mode ✅
- **Device 3 (Mirror/Bridge/Log)**: ESP-IDF with DMA polling mode ✅
- **USB Device**: Arduino Serial (proven stable) ✅
- **USB Host**: ESP-IDF implementation ✅
- **UART Logger**: ESP-IDF with DMA polling mode ✅
- **Permanent Network Mode**: Fully implemented and configurable ✅
- **Web Server**: ESPAsyncWebServer for non-blocking operations ✅