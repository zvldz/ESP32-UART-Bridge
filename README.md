# ESP32 UART Bridge

[![PlatformIO](https://img.shields.io/badge/PlatformIO-5.0%2B-blue)](https://platformio.org/)
[![ESP32](https://img.shields.io/badge/ESP32-S3-green)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Board](https://img.shields.io/badge/Board-Waveshare_S3_Zero-blue)](https://www.waveshare.com/wiki/ESP32-S3-Zero)
[![Version](https://img.shields.io/badge/Version-2.18.15-brightgreen)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Universal UART to USB bridge with web configuration interface for any serial communication needs.

<a href="https://www.buymeacoffee.com/8tSDjWbdc" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" height="36" ></a>

> **⚠️ BETA SOFTWARE**: This is a beta version and may not work stably in all configurations. Please report any issues you encounter.

## Quick Flash

<a href="https://zvldz.github.io/ESP32-UART-Bridge/" target="_blank"><img src="https://img.shields.io/badge/Web_Flasher-Flash_from_Browser-blue?style=for-the-badge" alt="Web Flasher" height="36"></a>

Flash firmware directly from your browser — no tools required (Chrome/Edge only).

## Features

- **Universal Protocol Support**: Works with any UART-based protocol - industrial (Modbus RTU), IoT (AT Commands), navigation (NMEA GPS), robotics (MAVLink), RC (SBUS), and more
- **Protocol Optimization**:
  - **SBUS**: Multi-source failover, WiFi transport with UDP batching
  - **CRSF/ELRS**: RC channel + telemetry parsing, text/binary output via USB, UART, UDP, BLE
  - **MAVLink**: Zero-latency packet forwarding, multi-GCS routing, priority-based transmission
  - **Raw Mode**: Adaptive buffering for unknown protocols
- **High Performance**: DMA-accelerated UART with adaptive buffering (256-2048 bytes based on baud rate)
- **USB Host/Device Modes**:
  - Device mode: ESP32 as USB-to-Serial converter
  - Host mode: Connect USB modems/devices to ESP32
- **Dual WiFi Modes**:
  - **Access Point**: Create hotspot for direct configuration
  - **Client Mode**: Connect to existing WiFi networks
  - **WiFi "At Boot" Options**: Disabled, Temporary (auto-disable), Always On
  - **Triple-click Activation**: Manual WiFi start from standalone mode
- **Bluetooth Connectivity**:
  - **Classic SPP** (MiniKit): Wireless serial bridge for Android GCS apps
  - **BLE NUS** (all boards): Low-power wireless telemetry with passkey pairing
- **Visual Feedback**: RGB LED shows data flow and system status
- **Wide Speed Range**: 4800 to 1000000 baud
- **Flow Control**: Hardware RTS/CTS support
- **Web Interface**: Configuration, monitoring, crash logs, OTA updates
- **RC Channel Monitor**: Real-time 16-channel bar visualization in web UI (SBUS and CRSF)
- **Flexible Logging**: Multi-level logs via web, UART, or UDP network stream

## Current Limitations

- **Device 4 Network**: UDP only
- **Additional Protocols**: Modbus planned for future releases

## Hardware

- **Recommended Boards**:
  - **Waveshare ESP32-S3-Zero**: Compact (25x24mm), WS2812 RGB LED, USB-C, 4MB Flash, USB Host support ✅ Fully tested
  - **ESP32-S3 Super Mini**: Ultra-compact, WS2815 RGB LED (GPIO48), USB-C, 4MB Flash, no USB Host ✅ Fully tested
  - **XIAO ESP32-S3**: Ultra-compact (21x17.5mm), single-color LED, USB-C, 8MB Flash/8MB PSRAM, USB Host support, external antenna connector ⚠️ Partially tested (basic mode: UART Bridge + USB)
  - **ESP32 MiniKit with CP2104**: Classic WROOM-32 board, single-color LED (GPIO2), USB via CP2104 chip, 4MB Flash, no PSRAM ⚠️ Partially tested (CH340/CH9102 not supported)
- **Requirements**:
  - ESP32 chip with dual-core (single-core not supported)
  - USB-C or micro-USB with data lines connected
- **Connections (ESP32-S3-Zero / Super Mini)**:
  - **Device 1 (Main UART - Always Active)**:
    - GPIO4: UART RX (connect to device TX)
    - GPIO5: UART TX (connect to device RX)
    - GPIO6: RTS (optional flow control)
    - GPIO7: CTS (optional flow control)
  - **Device 2 (Secondary UART - When UART2 Role Selected)**:
    - GPIO8: UART RX
    - GPIO9: UART TX
  - **Device 3 (Mirror/Bridge/Logger UART)**:
    - GPIO11: UART RX (used only in Bridge mode)
    - GPIO12: UART TX (used in all Device 3 modes)
  - **Device 4 (Network Channel)**:
    - No physical pins - uses Wi-Fi network
    - Network Logger: UDP port for log streaming (default: 14560)
    - Network Bridge: Bidirectional UDP for UART data (default: 14550)
  - **System**:
    - GPIO0: BOOT button (built-in) - triple-click for network mode
    - GPIO21: RGB LED (WS2812 - Zero) or GPIO48 (WS2815 - Super Mini)
- **Connections (XIAO ESP32-S3)** - Uses D-pin labels on board:
  - **Device 1 (Main UART - Always Active)**:
    - D3 (GPIO4): UART RX (connect to device TX)
    - D4 (GPIO5): UART TX (connect to device RX)
    - D0 (GPIO1): RTS (optional flow control)
    - D1 (GPIO2): CTS (optional flow control)
  - **Device 2 (Secondary UART - When UART2 Role Selected)**:
    - D8 (GPIO8): UART RX
    - D9 (GPIO9): UART TX
  - **Device 3 (Mirror/Bridge/Logger UART)**:
    - D7 (GPIO44): UART RX (used only in Bridge mode)
    - D6 (GPIO43): UART TX (used in all Device 3 modes)
  - **Device 4 (Network Channel)**:
    - No physical pins - uses Wi-Fi network
    - Network Logger: UDP port for log streaming (default: 14560)
    - Network Bridge: Bidirectional UDP for UART data (default: 14550)
  - **System**:
    - GPIO0: BOOT button (built-in) - triple-click for network mode
    - GPIO21: Single-color LED (blink-only, no RGB)
  - **⚠️ Warning**: ESP32-S3 supports only 3.3V logic levels!
- **Connections (ESP32 MiniKit with CP2104)**:
  - **Device 1 (Main UART - Always Active)**:
    - GPIO4: UART RX (connect to device TX)
    - GPIO5: UART TX (connect to device RX)
    - GPIO18: RTS (optional flow control)
    - GPIO19: CTS (optional flow control)
  - **Device 2 (USB Device Only)**:
    - USB via external CP2104 chip (no UART2 option)
    - UART2 NOT available on MiniKit (GPIO8/9 used by SPI flash)
  - **Device 3 (Mirror/Bridge/Logger UART)**:
    - GPIO16: UART RX (used only in Bridge mode)
    - GPIO17: UART TX (used in all Device 3 modes)
  - **Device 4 (Network Channel)**:
    - No physical pins - uses Wi-Fi network
    - Network Logger: UDP port for log streaming (default: 14560)
    - Network Bridge: Bidirectional UDP for UART data (default: 14550)
  - **System**:
    - RESET button: Triple-press always activates temporary AP mode (no BOOT button)
    - GPIO2: Single-color LED (built-in, active-high)
  - **⚠️ Limitations**: No USB Host mode, no PSRAM, no UART2 on Device 2

## Quick Start

1. **Connect Hardware**:
   - **Zero/SuperMini**: Device TX → GPIO4, Device RX → GPIO5, GND → GND
   - **XIAO**: Device TX → D3, Device RX → D4, GND → GND
   - **MiniKit**: Device TX → GPIO4, Device RX → GPIO5, GND → GND
   - ⚠️ **Warning**: ESP32-S3 supports only 3.3V logic levels! MiniKit (WROOM-32) supports 3.3V only.

2. **Power On**:
   - Connect USB cable to computer (USB-C for S3 boards, micro-USB for MiniKit)
   - **Zero/SuperMini**: Rainbow LED effect indicates successful boot
   - **XIAO**: Three quick blinks indicate successful boot (single-color LED)
   - **MiniKit**: Three quick blinks indicate successful boot (single-color LED on GPIO2)

3. **Configure**:

   **Option A: Temporary Setup (traditional method)**
   - **Zero/SuperMini/XIAO**: Triple-click BOOT button (LED turns solid blue)
   - **MiniKit**: Triple-press RESET button quickly (always activates AP mode, LED turns solid)
   - Connect to WiFi network "ESP-Bridge" (password: 12345678)
   - Open web browser to 192.168.4.1
   - Configure settings and click "Save & Reboot"
   - Device returns to standalone mode after timeout

   **Option B: Always-On Network Mode**
   - Use temporary setup to access web interface
   - Set "At Boot" to "Always On" in WiFi Configuration
   - Set your UART parameters and WiFi credentials
   - Click "Save & Reboot"
   - Device will maintain Wi-Fi connection permanently for remote access

4. **Use**:
   - **Device Mode**: ESP32 appears as COM port on computer
     - Terminal software (PuTTY, CoolTerm, etc.)
     - Industrial HMI software
     - GPS utilities
     - Ground control stations
     - Custom applications
   - **Host Mode**: Connect USB devices to ESP32's USB port
   - LED flashes indicate data flow:
     - Blue: Device → Computer
     - Green: Computer → Device
     - Cyan: Bidirectional

## WiFi Operation Modes

The device supports two WiFi connection modes with configurable "At Boot" behavior:

### WiFi Access Point (AP) Mode
- **Default Mode**: Creates WiFi hotspot "ESP-Bridge-xxxx" with unique MAC-based suffix (password: 12345678)
- **Direct Configuration**: Connect devices directly to ESP32 for setup
- **No Internet Required**: Works independently without existing WiFi infrastructure
- **Access**: Web interface at 192.168.4.1 or via mDNS (e.g., `esp-bridge-xxxx.local`)

### WiFi Client Mode
- **Network Integration**: Connects ESP32 to existing WiFi network
- **Multi-Network Support**: Configure up to 5 WiFi networks with priority order
  - Automatic fallback to next network when connection fails
  - Priority-based scanning (tries networks in configured order)
- **Remote Access**: Access web interface using assigned IP address or mDNS hostname
- **Internet Connectivity**: Enables cloud features and remote monitoring
- **Configuration**: Set networks via web interface (primary + 4 additional in collapsible section)
- **Intelligent Connection Logic**:
  - Scans for configured networks every 15 seconds when not found
  - Up to 5 password retries, then tries next SSID (if configured)
  - Distinguishes between authentication failures and network unavailability
  - Unlimited reconnection attempts after successful initial connection
  - Automatic recovery from temporary network outages

### WiFi "At Boot" Options

**Disabled**
- WiFi does not start automatically at boot
- Activate via triple-click BOOT button when needed
- Returns to standalone mode after timeout or manual disable

**Temporary (Default)**
- WiFi starts at boot automatically
- Auto-disables after 5 minutes if no activity (AP: no clients, Client: no web access)
- Ideal for quick configuration without draining battery

**Always On**
- WiFi remains active indefinitely
- Maintains connection across reboots
- **Benefits**: Always-on remote access, continuous network logging

### Triple-Click Behavior
- **From Standalone**: Activates saved WiFi mode (AP or Client)
- **From WiFi Client**: Forces temporary AP mode for reconfiguration
- Useful when can't access current network or need to change settings

### Mode Switching
- **Standalone → Network**: Triple-click BOOT button (uses saved WiFi mode)
- **WiFi Client → AP**: Triple-click BOOT button (temporary AP for reconfiguration)
- **Change "At Boot"**: Select Disabled/Temporary/Always On in web interface
- **Disable Network**: Set "At Boot" to Disabled or let Temporary mode auto-disable

### mDNS (Local Network Discovery)
- **Easy Access**: Connect to device using `hostname.local` instead of IP address
- **Auto-Generated**: Unique hostname created on first boot (e.g., `esp-bridge-11fc.local`)
- **Customizable**: Set custom hostname via web interface
- **Both Modes**: Works in AP and Client modes

### WiFi Client Connection Logic

The device implements intelligent connection management with different behaviors for initial connection vs. reconnection:

#### Initial Connection Process
1. **Network Scanning**: Scans for configured SSID every 15 seconds
   - **LED**: Orange slow blink (2 second intervals)
   - Continues until network is found or mode is changed
2. **Connection Attempts**: When network found, up to 5 password retries
   - **LED**: Orange slow blink continues during attempts
   - On failure, tries next configured SSID (if available)
3. **Success**: Connection established and IP address obtained
   - **LED**: Green solid (connected)
   - Device ready for remote access
4. **Authentication Failure**: Wrong password on all configured networks
   - **LED**: Red fast blink (500ms intervals)
   - **Recovery**: Requires device restart or mode change
   - Only occurs after trying all SSIDs (5 retries each)

#### Network Loss Recovery
1. **Connection Monitoring**: Detects when established connection is lost
2. **Immediate Reconnection**: Starts scanning and connecting immediately
   - **LED**: Orange slow blink (searching for network)
   - No retry limit - assumes temporary network outage
3. **Unlimited Attempts**: Continues reconnection indefinitely
   - Device has previously proven credentials work
   - Handles router restarts, temporary outages, WiFi roaming

#### Error States and Recovery
- **Network Not Found**: Orange slow blink, continues scanning indefinitely
- **Wrong Password**: Red fast blink, requires restart to retry
- **Connection Lost**: Returns to search mode, unlimited reconnection attempts
- **AP Conflict**: Device operates only in Client mode when connected (no dual AP+Client mode)

## Configuration Management

### Configuration Backup & Restore
- **Export Configuration**: Download your current device settings as a JSON file
  - Access via web interface → "Configuration Backup" section → "Export Config"
  - Creates unique filename using mDNS hostname (e.g., `esp-bridge-11fc-config.json`)
  - Includes all UART, WiFi, device roles, and logging settings
- **Import Configuration**: Upload and apply saved configuration
  - Select JSON file via "Import Config" button
  - Validates configuration structure and version compatibility
  - Automatically reboots device with new settings
- **Use Cases**:
  - Quick provisioning of multiple devices
  - Backup before firmware updates
  - Share working configurations between team members
  - Restore after device reset or configuration errors
- **Factory Reset**: Reset all settings to defaults via web interface button

## Common Use Cases

- **RC Systems**: SBUS bridging, WiFi transport, multi-source failover for RC receivers and flight controllers
- **Vehicle Telemetry**: MAVLink communication for drones, rovers, boats with native protocol optimization
- **Industrial Automation**: Connect Modbus RTU devices to modern systems
- **IoT Connectivity**: Bridge 4G/LTE USB modems to embedded devices
- **Marine Electronics**: NMEA GPS/AIS data bridging
- **Legacy Equipment**: Modernize RS232/RS485 equipment
- **Development**: Debug embedded systems with USB convenience
- **Remote Monitoring**: Permanent Wi-Fi access for remote diagnostics
- **Network Bridging**: UART-to-UDP conversion for wireless serial communication
- **Remote Logging**: Stream device logs over network for centralized monitoring

## USB Modes

### Device Mode (Default)
Traditional operation - ESP32 acts as USB-to-Serial converter:
- Connect ESP32 to PC via USB
- ESP32 appears as COM port
- Use with any serial terminal or GCS software

### Host Mode
ESP32 acts as USB host for serial devices:
- Connect USB modems, GPS receivers, or other USB-serial devices to ESP32
- Data is bridged between USB device and UART pins
- Useful for adding USB connectivity to devices with only UART
- **⚠️ Power Requirements**: In Host mode, you MUST provide external 5V power to the ESP32 board pins (VIN or 5V pin). The USB-C connector cannot power both ESP32 and connected USB devices.

### Mode Selection
- Configure via web interface
- Setting is saved and persists across reboots
- Requires restart when changing modes

## Power Requirements

### Device Mode
- Power via USB-C from computer - no external power needed
- ESP32 and connected UART device are powered from USB

### Host Mode  
- **MUST** provide external 5V power via board pins (VIN/5V pin)
- USB-C cannot provide enough power for both ESP32 and connected USB devices
- External power supplies 5V VBUS to connected USB devices

### ⚠️ Important Safety Warning
**Never connect both USB-C and external power simultaneously!** This can damage the board.
(Some boards have protection diodes that allow dual power, but verify your board's schematic first. Use at your own risk!)

### Changing Settings
To enter network mode and change settings (including USB mode):
- Power ONLY via USB-C (disconnect external power)
- OR power ONLY via external pins (disconnect USB-C)
- The USB mode setting doesn't matter for configuration
- Just ensure only ONE power source is connected

## Configuration Parameters

The web interface allows configuration of:
- **Baud Rate**: 4800 to 1000000 baud
- **Data Bits**: 7 or 8
- **Stop Bits**: 1 or 2
- **Parity**: None, Even, Odd
- **Flow Control**: None or RTS/CTS
- **WiFi Settings**: Choose AP or Client mode, configure up to 5 networks for Client mode
- **WiFi TX Power**: Adjustable transmit power (2-20 dBm) for range/power optimization
- **USB Mode**: Device (default) or Host mode
- **WiFi "At Boot"**: Disabled, Temporary (auto-disable), or Always On
- **Device Roles**: Configure Device 1, 2, 3, and 4 functionality (including SBUS modes)
- **Protocol Optimization**: Choose RAW (timing-based), MAVLink (packet-aware), or SBUS (frame-based) optimization

## Device Roles

The bridge supports multiple configurable devices:

### Device 1 - Main UART (Always Active)
Primary UART interface on GPIO 4/5. Can be configured as:
- **Standard UART**: Full range of baud rates and configurations
- **SBUS Input**: Read SBUS frames from RC receiver (100000 baud, 8E2, inverted signal)

### Device 2 - Secondary Channel
Can be configured as:
- **Disabled**: No secondary channel
- **UART2**: Additional UART on GPIO 8/9 (not available on MiniKit)
- **USB**: USB device or host mode
- **SBUS Input**: Second SBUS source for failover
- **SBUS Output**: Send SBUS to flight controller (binary format)
- **USB SBUS Text Output**: Text format `RC 1500,...\r\n` via USB (MiniKit)
- **USB Logger**: Stream system logs via USB (same format as UART Logger)

### Device 3 - Auxiliary UART
Can be configured as:
- **Disabled**: Not used
- **Mirror**: One-way copy of Device 1 data (TX only)
- **Bridge**: Full bidirectional UART bridge with Device 1 and protocol-aware processing
- **Logger**: Dedicated UART log output at 115200 baud
- **SBUS Input**: Third SBUS source for failover
- **SBUS Output**: Send SBUS to flight controller (binary format)
- **SBUS Text Output**: Text format `RC 1500,...\r\n` via UART

### Device 4 - Network Channel
Requires active Wi-Fi connection. Can be configured as:
- **Disabled**: Not used
- **Network Logger**: Stream system logs via UDP (port 14560)
- **Network Bridge**: Bidirectional UART-to-UDP conversion (port 14550)
- **SBUS Output**: Send SBUS frames over WiFi (binary format)
- **SBUS Text Output**: Send SBUS as text format `RC 1500,...\r\n` over UDP
- **SBUS Input**: Receive SBUS from WiFi with failover support

### Device 5 - Bluetooth
Wireless serial bridge via Bluetooth. Requires BLE or BT firmware variant (see [Build Environments](#build-environments)).
- **All boards**: BLE with NUS (Nordic UART Service) — hardcoded passkey 1234
- **MiniKit only**: Also supports Bluetooth Classic SPP — SSP "Just Works" pairing (no PIN)
  - ⚠️ Classic BT disables WiFi (mutual exclusion on WROOM-32). Use BLE build for BT + WiFi together.

⚠️ **BLE re-pairing**: Firmware updates via USB (PlatformIO) may erase NVS bond storage, requiring re-pairing on the client. OTA updates via web interface preserve bonds.

Device name matches mDNS hostname. Modes:
- **Disabled**: Bluetooth off
- **Bridge**: MAVLink/Raw telemetry to Android apps (QGC, Tower, Mission Planner)
- **SBUS Text Output**: Text format `RC 1500,...\r\n` for Mission Planner RC Override plugin

### Network Configuration for Device 4
When Device 4 is enabled, additional settings appear:
- **Target IP**: Destination for UDP packets (supports up to 4 comma-separated IPs)
  - Use x.x.x.255 for broadcast (reaches all devices on network)
  - Use specific IP for unicast (single destination)
  - Multiple targets: `192.168.1.10,192.168.1.20` sends to both
- **Port**: UDP port number (1-65535)
- **Auto Broadcast**: Automatically discover UDP targets on network (Client mode + UDP TX only)
- **Network Log Level**: Only visible when Device 4 is active

## SBUS Protocol Support

SBUS is a digital RC protocol used by FrSky, Futaba, and compatible receivers. Standard configuration: 100000 baud, 8E2, inverted signal.

### Hardware Requirements
- **No external inverter needed**: ESP32 UART supports software signal inversion
- **Direct connection**: Connect SBUS signal directly to ESP32 GPIO pins

### SBUS Features
- **Multi-source Failover**: Automatic switching between up to 4 SBUS sources (Device1, Device2, Device3, UDP)
- **Priority System**: Device1 > Device2 > Device3 > UDP (configurable to Auto or Manual mode)
- **WiFi Transport**: Send/receive SBUS over UDP with 3-frame batching (~85% packet reduction)
- **Timing Keeper**: Repeats last frame every 20ms when WiFi source lags (configurable)
- **Web Monitoring**: Real-time source quality, failsafe state, and switching statistics

### Common Configurations

**Direct Connection** (RC receiver → Flight controller):
- Device 1: SBUS Input (from RC receiver)
- Device 2: SBUS Output (to flight controller)
- Single ESP32 bridge between receiver and FC

**WiFi Link** (Wireless SBUS between ESP32 units):
- ESP1: Device 1 SBUS Input, Device 4 SBUS UDP TX
- ESP2: Device 4 SBUS UDP RX, Device 2/3 SBUS Output
- SBUS transmitted wirelessly with automatic batching

**Multi-source Failover** (Redundant SBUS inputs):
- Device 1: SBUS Input (primary source)
- Device 2: SBUS Input (secondary source)
- Device 3: SBUS Input (tertiary source)
- Device 4: SBUS UDP RX (WiFi backup)
- Automatic failover based on signal quality and priority

### CRSF/ELRS Features
- **Device 1 CRSF Input**: 420000 baud, 8N1, single RX wire from ELRS receiver
- **RC Channels + Telemetry**: Parses RC (16ch), Link Quality, Battery, GPS, Attitude, Flight Mode, Altitude
- **Text Output**: Human-readable CRSF data via USB, UART3, UDP, or BLE
- **Binary Bridge**: Raw CRSF frame forwarding via USB or UART3 (for external FC tools)
- **Bidirectional**: Reverse channel from USB/UART3 back to ELRS RX (telemetry injection)
- **Web Monitoring**: Real-time CRSF statistics (valid/invalid frames, CRC errors, last activity)

### Common CRSF Configurations

**ELRS Telemetry via WiFi** (ELRS RX → ESP32 → GCS over UDP):
- Device 1: CRSF Input (from ELRS receiver)
- Device 4: CRSF Text Output (UDP to GCS)
- RC Override plugin reads channel + telemetry data

**ELRS via USB** (ELRS RX → ESP32 → PC):
- Device 1: CRSF Input
- Device 2: USB CRSF Text Output (human-readable) or USB CRSF Bridge (binary)

**ELRS Bidirectional Bridge** (FC ↔ ESP32 ↔ ELRS RX):
- Device 1: CRSF Input (bidirectional with RX)
- Device 3: CRSF Bridge (bidirectional UART3 to FC)
- Full CRSF passthrough between FC and ELRS receiver

## LED Indicators

**Note**: RGB indicators apply to Zero and SuperMini boards. XIAO and MiniKit have single-color LEDs that show blink patterns only.

### Data Activity (Standalone Mode)
| Color | Pattern | Meaning |
|-------|---------|---------|
| Blue | Flash | Data from device (UART RX) |
| Green | Flash | Data from computer (USB RX) |
| Cyan | Flash | Bidirectional data transfer |
| Off | - | Idle, no data |

*XIAO/MiniKit: Any data activity shows as LED blink*

### WiFi Network Status
| Color | Pattern | Meaning | Details |
|-------|---------|---------|---------|
| Blue | Solid | WiFi AP mode active | Hotspot "ESP-Bridge" ready for connections |
| Orange | Slow blink (2s) | WiFi Client searching | Scanning for configured network every 15s |
| Green | Solid | WiFi Client connected | Successfully connected to WiFi network |
| Red | Fast blink (500ms) | WiFi Client error | Wrong password after 5 attempts - restart required |

*XIAO/MiniKit: Solid ON for AP or Client connected, slow blink for searching, fast blink for error*

### Bluetooth Status (BLE firmware only)
| Color | Pattern | Meaning | Details |
|-------|---------|---------|---------|
| Purple | Solid | BLE only | Bluetooth active, WiFi disabled |
| Blue↔Purple | Fade | WiFi AP + BLE | Both connections available |
| Green↔Purple | Fade | WiFi Client + BLE | Both connections available |

*XIAO/MiniKit: Fast blink (150ms) for BLE only, solid ON for WiFi + BLE*

### System Status
| Color | Pattern | Meaning |
|-------|---------|---------|
| White | Quick blinks | Button click feedback |
| Purple | Rapid blink | WiFi reset confirmation |
| Rainbow | 1 second | Boot sequence |

## Button Functions

**Note**: Zero/SuperMini/XIAO use BOOT button (GPIO0). MiniKit uses RESET button (triple-press detection).

### Triple-Click/Press (Mode Switching)
- **From Standalone Mode**: Activates saved WiFi mode
  - If WiFi Client configured: Connects to saved network
  - If no Client configured: Creates AP hotspot
  - LED feedback: White blinks indicate click count (RGB boards) or quick blinks (single-color)
- **From WiFi Client Mode**: Forces temporary AP mode
  - Allows reconfiguration when can't access current network
  - Useful when WiFi password changed or moved to different location
  - Bypasses wrong password error state

### Long Press (5+ seconds) - Zero/SuperMini/XIAO only
- **WiFi Reset**: Resets WiFi settings to defaults (preserves UART and device roles)
  - SSID: "ESP-Bridge-xxxx" (unique, auto-generated)
  - Password: "12345678"
  - LED feedback: Purple rapid blink confirms reset
  - Device automatically restarts
- **MiniKit**: Long press not available (use Web UI Settings → Factory Reset)

### Power-On Hold - Zero/SuperMini/XIAO only
- **Bootloader Mode**: Hold BOOT during power-on for firmware flashing
  - Used for manual firmware updates via USB
- **MiniKit**: Use BOOT button on board (if available) or automatic bootloader detection

## Building from Source

### Requirements
- VSCode with PlatformIO extension
- ESP32 platform support (automatically installed)

### Build Instructions
1. Clone repository
2. Open in VSCode with PlatformIO
3. Select environment:
   - `production` - For normal use (no debug output)
   - `debug` - With crash diagnostics and logging
4. Build and upload

### Build Environments

Each board has multiple build variants:

| Board | Environments | Bluetooth |
|-------|-------------|-----------|
| **Zero** | `zero_production`, `zero_debug` | WiFi only |
| **Zero BLE** | `zero_ble_production`, `zero_ble_debug` | WiFi + BLE |
| **SuperMini** | `supermini_production`, `supermini_debug` | WiFi only |
| **SuperMini BLE** | `supermini_ble_production`, `supermini_ble_debug` | WiFi + BLE |
| **XIAO** | `xiao_production`, `xiao_debug` | WiFi only |
| **XIAO BLE** | `xiao_ble_production`, `xiao_ble_debug` | WiFi + BLE |
| **MiniKit** | `minikit_production`, `minikit_debug` | WiFi only |
| **MiniKit BT** | `minikit_bt_production`, `minikit_bt_debug` | WiFi + BT Classic SPP |
| **MiniKit BLE** | `minikit_ble_production`, `minikit_ble_debug` | WiFi + BLE |

- **production**: Standard firmware with maximum performance
- **debug**: Includes crash diagnostics and debug logging for troubleshooting
- **BLE/BT variants**: Include Bluetooth stack (Device 5), slightly larger firmware size

## Application Compatibility Notes

### USB Reset Behavior
The ESP32-S3's native USB implementation outputs bootloader messages when DTR/RTS lines are toggled. This can cause issues with some applications that interpret these messages as data.

**Common symptoms:**
- Connection resets repeatedly
- Application fails to establish stable connection
- Unexpected data appears in terminal

**Known affected applications and solutions:**
- **Mission Planner**: Enable "Disable RTS resets on ESP32 SerialUSB" in connection settings
- **QGroundControl**: Usually works without modification
- **Arduino IDE**: May require manual reset timing during upload
- **PuTTY/Terminal programs**: Disable "DTR/RTS flow control" in serial settings
- **ModbusPoll and similar**: Turn off hardware handshaking

**General fix:** Look for options to disable DTR/RTS control, hardware flow control, or "serial handshaking" in your application's connection settings.

### Protocol-Specific Considerations
- **SBUS devices**: Enable Protocol Optimization → SBUS for native frame parsing
  - 100000 baud, 8E2, inverted signal (handled by ESP32 UART)
  - Multi-source failover with automatic quality-based switching
  - WiFi transport with 3-frame UDP batching for efficiency
- **MAVLink devices**: Enable Protocol Optimization → MAVLink for zero-latency packet forwarding
  - Supports v1/v2 protocols at any baud rate with perfect packet boundaries
  - Memory pool optimization prevents heap fragmentation during intensive communication
  - Priority-based transmission ensures critical packets are delivered first
- **GPS/NMEA**: Most GPS modules use 9600 or 115200 baud (use RAW mode for timing-based optimization)
- **AT command modems**: May require specific line endings (CR, LF, or CR+LF) (use RAW mode)
- **Modbus RTU**: Uses strict timing requirements - RAW mode provides timing-based buffering but is not Modbus-optimized

### Device 4 Network Usage Examples

**Network Logger Mode:**
```bash
# Listen for logs on Linux/Mac
nc -u -l 14560

# Listen for logs on Windows
# Use a UDP listener tool or netcat for Windows
```

**Network Bridge Mode:**
```bash
# Connect to UART device over network
# Configure your application to use UDP instead of serial port
# Example: MAVLink ground station configured for UDP connection
# IP: ESP32's IP address, Port: 14550
```

## Troubleshooting

| Problem | Solution |
|---------|----------|
| No data activity | Check TX/RX connections, verify baud rate matches device |
| LED solid blue | WiFi AP mode active - connect to "ESP-Bridge" network |
| LED orange slow blink | WiFi Client searching - check SSID spelling and network availability |
| LED solid green | WiFi Client connected successfully |
| LED solid purple | BLE only mode - Bluetooth active, no WiFi |
| LED fade animation | WiFi + BLE active - both connections available |
| LED red fast blink | Wrong WiFi password - restart device or triple-click to change mode |
| Forgot WiFi password | Hold BOOT button 5+ seconds to reset to defaults (MiniKit: triple RESET for AP, then Factory Reset in web UI) |
| Can't connect to WiFi | Check network name, password, and 2.4GHz band (5GHz not supported) |
| WiFi connects then disconnects | Check router settings, signal strength, or power saving features |
| Connection drops | Enable flow control if supported by device |
| USB device not recognized | In Host mode, only CDC devices are supported |
| Application can't connect | Check USB cable, try different COM port settings |
| Connection resets repeatedly | Application toggling DTR/RTS - disable hardware flow control |
| Partial or corrupted data | Check baud rate settings, verify wire quality and grounding |
| "UART FIFO overflow" messages | This indicates USB cannot keep up with UART data rate. Only appears when COM port is open. Check adaptive buffer size matches your baud rate, consider enabling flow control, or reduce baud rate. |
| "USB: Dropping data - port not connected" | Normal behavior when COM port is not opened. Data is discarded to prevent buffer overflow. Open a serial terminal to receive data. |
| Can't access WiFi network | Triple-click BOOT to enter temporary AP mode, then reconfigure |
| Network takes long to connect | Normal - device scans every 15s and attempts 5 connections when found |

## Performance Notes

- **DMA-Accelerated UART**: Hardware DMA reduces CPU usage and packet loss
- **Adaptive Buffering**: Buffer size auto-adjusts based on baud rate (256-2048 bytes)
- **Protocol Modes**:
  - **RAW**: Timing-based packet detection for unknown protocols
  - **MAVLink/SBUS**: Protocol-aware parsing with zero-latency forwarding
- **Reliable Operation**: Tested up to 1000000 baud with minimal packet loss

## Advanced Features

### Protocol Statistics

The bridge provides real-time protocol statistics through the web interface:
- **RAW Protocol Statistics**: Shows chunk processing, buffer utilization, and timing-based metrics for unknown protocols
- **MAVLink Protocol Statistics**: Displays packet detection, errors, resync events, and priority-based transmission analysis
- **Output Device Monitoring**: Real-time statistics for all active senders (USB, UART, UDP) with queue depths and drop analysis
- **Performance Metrics**: Buffer usage, transmission rates, and last activity timing

Access protocol statistics via the web interface Status tab to monitor data flow and diagnose communication issues.

### Crash Diagnostics
The device automatically logs system crashes including:
- Reset reason (panic, watchdog, etc.)
- Uptime before crash
- Free heap memory
- Minimum heap during session

View crash history in web interface under "Crash History" section.

### Logging System
The bridge includes a flexible logging system with multiple levels:
- **ERROR**: Critical errors only
- **WARNING**: Important warnings
- **INFO**: General information
- **DEBUG**: Detailed diagnostic information

Logs are viewable through the web interface. The system supports multiple output channels:
- **Web Interface**: View logs in real-time through the browser
- **UART Output**: Optional logging to Device 3 UART TX pin (115200 baud)
- **Network Logging**: UDP log streaming via Device 4 (when configured as Network Logger)


## Development

- **[CHANGELOG.md](CHANGELOG.md)** - Detailed version history and technical implementation details
- **[TODO.md](TODO.md)** - Future roadmap and current architecture documentation

## Acknowledgments

This project was developed with assistance from various AI systems, including Claude (Anthropic), ChatGPT (OpenAI), and other AI assistants. Their contributions helped with code optimization, documentation, and architectural decisions.

## License

MIT License - see LICENSE file for details