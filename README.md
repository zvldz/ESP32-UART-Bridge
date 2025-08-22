# ESP32 UART Bridge

[![PlatformIO](https://img.shields.io/badge/PlatformIO-5.0%2B-blue)](https://platformio.org/)
[![ESP32](https://img.shields.io/badge/ESP32-S3-green)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Board](https://img.shields.io/badge/Board-Waveshare_S3_Zero-blue)](https://www.waveshare.com/wiki/ESP32-S3-Zero)
[![Version](https://img.shields.io/badge/Version-2.15.3-brightgreen)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Universal UART to USB bridge with web configuration interface for any serial communication needs.

<a href="https://www.buymeacoffee.com/8tSDjWbdc" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" height="36" ></a>

> **⚠️ BETA SOFTWARE**: This is a beta version and may not work stably in all configurations. Please report any issues you encounter.

## Features

- **Universal Protocol Support**: Works with any UART-based protocol - industrial (Modbus RTU), IoT (AT Commands), navigation (NMEA GPS), robotics (MAVLink), and more
- **Advanced Protocol Pipeline**: Modern Parser + Sender architecture with memory pool management
  - **MAVLink Protocol Optimization**: Native MAVLink v1/v2 packet detection with zero latency
  - **Raw Protocol Mode**: Adaptive buffering for unknown protocols with timing-based optimization
  - **Memory Pool**: Efficient packet memory management to prevent heap fragmentation
  - **Priority-based Transmission**: CRITICAL > NORMAL > BULK packet prioritization with intelligent dropping
- **DMA-Accelerated Performance**: Hardware DMA with ESP-IDF drivers for minimal CPU usage and minimal packet loss
- **Circular Buffer Architecture**: Zero-copy circular buffer with scatter-gather I/O for optimal performance
  - Prevents data corruption from buffer operations
  - Adaptive traffic detection (normal vs burst modes)
  - Smart USB bottleneck handling with percentage-based thresholds
- **Dynamic Buffer Sizing**: Automatically adjusts buffer size based on baud rate (256-2048 bytes)
- **USB Host/Device Modes**: 
  - Device mode: Connect ESP32 to PC as USB serial device
  - Host mode: Connect USB modems or serial adapters to ESP32
  - Configurable via web interface
- **High Performance**: Hardware packet detection (UART idle timeout) and event-driven architecture (Device 1)
- **Dual WiFi Modes**: Flexible network connectivity options
  - **WiFi Access Point (AP) Mode**: Create hotspot for direct configuration 
  - **WiFi Client Mode**: Connect to existing WiFi networks for remote access
  - **Permanent Network Mode**: Wi-Fi remains active permanently for always-on access
  - **Temporary Setup Mode**: Wi-Fi timeout for quick configuration (triple-click activation)
- **Visual Feedback**: RGB LED shows data flow direction and system status
- **Wide Speed Range**: 4800 to 1000000 baud
- **Flow Control**: Hardware RTS/CTS support
- **Crash Logging**: Automatic crash detection and logging for diagnostics
- **Flexible Logging**: Multi-level logging system with web interface
- **OTA Updates**: Firmware updates via web interface

## Current Limitations

- **Device 3 Adaptive Buffering**: Not yet integrated with new protocol pipeline (uses legacy approach)
- **Protocol Pipeline**: Currently supports MAVLink and Raw modes (SBUS, CRSF planned for future)
- **Transmission Modes**: USB/UART senders support partial send recovery, UDP sender does not
- **Device 2/3**: Use polling mode instead of event-driven architecture
- **Device 4**: UDP only (TCP support planned for future versions)
- **Multi-device Pipeline**: Protocol pipeline optimized for single input → multiple outputs

## Hardware

- **Recommended Board**: [Waveshare ESP32-S3-Zero](https://www.waveshare.com/wiki/ESP32-S3-Zero)
  - Compact size (25x24mm)
  - Built-in WS2812 RGB LED
  - USB-C connector with native USB support
  - 4MB Flash
  - Dual-core processor (required)
- **Alternative**: Compatible ESP32-S3 boards
  - Must have ESP32-S3 chip with native USB support (for USB Host mode)
  - **Must be dual-core variant** (single-core ESP32 not supported)
  - USB-C or micro-USB with data lines connected to ESP32-S3
  - Similar pinout and features to ESP32-S3-Zero
  - Note: May require code modifications for different LED pins or missing components
- **Connections**:
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
    - GPIO21: RGB LED (WS2812 - built-in on ESP32-S3-Zero)
  - **⚠️ Warning**: ESP32-S3 supports only 3.3V logic levels!

## Quick Start

1. **Connect Hardware**:
   - Device TX → ESP32 GPIO4
   - Device RX → ESP32 GPIO5
   - Device GND → ESP32 GND
   - ⚠️ **Warning**: ESP32-S3 supports only 3.3V logic levels!

2. **Power On**:
   - Connect USB-C cable to computer
   - Rainbow LED effect indicates successful boot

3. **Configure**:
   
   **Option A: Temporary Setup (traditional method)**
   - Triple-click BOOT button (LED turns solid purple)
   - Connect to WiFi network "ESP-Bridge" (password: 12345678)
   - Open web browser to 192.168.4.1
   - Configure settings and click "Save & Reboot"
   - Device returns to standalone mode after timeout
   
   **Option B: Permanent Network Mode**
   - Use temporary setup to access web interface
   - Enable "Permanent Network Mode" checkbox in WiFi Configuration
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

The device supports two WiFi connection modes that can operate in temporary or permanent configurations:

### WiFi Access Point (AP) Mode
- **Default Mode**: Creates WiFi hotspot "ESP-Bridge" (password: 12345678)
- **Direct Configuration**: Connect devices directly to ESP32 for setup
- **No Internet Required**: Works independently without existing WiFi infrastructure
- **Access**: Web interface at 192.168.4.1

### WiFi Client Mode  
- **Network Integration**: Connects ESP32 to existing WiFi network
- **Remote Access**: Access web interface using assigned IP address
- **Internet Connectivity**: Enables cloud features and remote monitoring
- **Configuration**: Set SSID and password via web interface
- **Intelligent Connection Logic**: 
  - Scans for configured network every 15 seconds when not found
  - Attempts connection up to 5 times when network is available
  - Distinguishes between authentication failures and network unavailability
  - Unlimited reconnection attempts after successful initial connection
  - Automatic recovery from temporary network outages

### Network Mode Duration

**Temporary Mode (Triple-Click Activation)**
- Activated by triple-clicking BOOT button from any mode
- Wi-Fi active for limited time with automatic timeout
- Returns to standalone mode automatically after timeout
- **Mode Selection**: 
  - From Standalone: Activates saved WiFi mode (AP or Client)
  - From WiFi Client: Forces temporary AP mode for reconfiguration

**Permanent Network Mode**
- Configured via web interface "Permanent Network Mode" checkbox
- Wi-Fi remains active indefinitely until manually disabled  
- No automatic timeout - maintains connection across reboots
- **Benefits**: Always-on remote access, continuous network logging

### Mode Switching
- **Standalone → Network**: Triple-click BOOT button (uses saved WiFi mode)
- **WiFi Client → AP**: Triple-click BOOT button (temporary AP for reconfiguration)
- **Enable Permanent**: Check box in web interface WiFi configuration
- **Disable Network**: Uncheck permanent mode via web interface

### WiFi Client Connection Logic

The device implements intelligent connection management with different behaviors for initial connection vs. reconnection:

#### Initial Connection Process
1. **Network Scanning**: Scans for configured SSID every 15 seconds
   - **LED**: Orange slow blink (2 second intervals)
   - Continues until network is found or mode is changed
2. **Connection Attempts**: When network found, attempts connection up to 5 times
   - **LED**: Orange solid during connection attempts
   - Brief pause between attempts for WiFi stack recovery
3. **Success**: Connection established and IP address obtained
   - **LED**: Orange solid (connected)
   - Device ready for remote access
4. **Authentication Failure**: Wrong password detected after 5 attempts
   - **LED**: Red fast blink (500ms intervals) 
   - **Recovery**: Requires device restart or mode change
   - Device stops attempting connections to prevent router lockout

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
  - Creates unique filename: `esp32-bridge-config-[ID].json`
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

## Common Use Cases

- **Industrial Automation**: Connect Modbus RTU devices to modern systems
- **IoT Connectivity**: Bridge 4G/LTE USB modems to embedded devices
- **Vehicle Telemetry**: MAVLink communication for drones, rovers, boats with native protocol optimization
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
- **WiFi Settings**: Choose AP or Client mode, configure credentials
- **USB Mode**: Device (default) or Host mode
- **Network Mode**: Temporary setup or permanent Wi-Fi operation
- **Device Roles**: Configure Device 2, 3, and 4 functionality
- **Protocol Optimization**: Choose RAW (timing-based) or MAVLink (packet-aware) optimization

## Device Roles

The bridge supports multiple configurable devices:

### Device 1 - Main UART (Always Active)
- Primary UART interface on GPIO 4/5
- Cannot be disabled - this is the main data channel
- Supports full range of baud rates and configurations

### Device 2 - Secondary Channel
Can be configured as:
- **Disabled**: No secondary channel
- **UART2**: Additional UART on GPIO 8/9
- **USB**: USB device or host mode

### Device 3 - Auxiliary UART
Can be configured as:
- **Disabled**: Not used
- **Mirror**: One-way copy of Device 1 data (TX only)
- **Bridge**: Full bidirectional UART bridge with Device 1
- **Logger**: Dedicated UART log output at 115200 baud

### Device 4 - Network Channel
Requires active Wi-Fi connection. Can be configured as:
- **Disabled**: Not used
- **Network Logger**: Stream system logs via UDP
  - Default port: 14560
  - Supports broadcast (x.x.x.255) or unicast
  - Configurable log levels (ERROR, WARNING, INFO, DEBUG)
- **Network Bridge**: Bidirectional UART-to-UDP conversion
  - Default port: 14550
  - Full duplex communication
  - Ideal for wireless serial connections
  - Use with ground control stations, telemetry systems, etc.

### Network Configuration for Device 4
When Device 4 is enabled, additional settings appear:
- **Target IP**: Destination for UDP packets
  - Use x.x.x.255 for broadcast (reaches all devices on network)
  - Use specific IP for unicast (single destination)
- **Port**: UDP port number (1-65535)
- **Network Log Level**: Only visible when Device 4 is active

## LED Indicators

### Data Activity (Standalone Mode)
| Color | Pattern | Meaning |
|-------|---------|---------|
| Blue | Flash | Data from device (UART RX) |
| Green | Flash | Data from computer (USB RX) |
| Cyan | Flash | Bidirectional data transfer |
| Off | - | Idle, no data |

### WiFi Network Status  
| Color | Pattern | Meaning | Details |
|-------|---------|---------|---------|
| Purple | Solid | WiFi AP mode active | Hotspot "ESP-Bridge" ready for connections |
| Orange | Slow blink (2s) | WiFi Client searching | Scanning for configured network every 15s |
| Orange | Solid | WiFi Client connected | Successfully connected to WiFi network |
| Red | Fast blink (500ms) | WiFi Client error | Wrong password after 5 attempts - restart required |

### System Status
| Color | Pattern | Meaning |
|-------|---------|---------|
| White | Quick blinks | Button click feedback |
| Purple | Rapid blink | WiFi reset confirmation |
| Rainbow | 1 second | Boot sequence |

## Button Functions

### Triple-Click (Mode Switching)
- **From Standalone Mode**: Activates saved WiFi mode
  - If WiFi Client configured: Connects to saved network  
  - If no Client configured: Creates AP hotspot
  - LED feedback: White blinks indicate click count
- **From WiFi Client Mode**: Forces temporary AP mode
  - Allows reconfiguration when can't access current network
  - Useful when WiFi password changed or moved to different location
  - Bypasses wrong password error state

### Long Press (5+ seconds)
- **WiFi Reset**: Resets WiFi credentials to factory defaults
  - SSID: "ESP-Bridge" 
  - Password: "12345678"
  - LED feedback: Purple rapid blink confirms reset
  - Device automatically restarts

### Power-On Hold
- **Bootloader Mode**: Hold during power-on for firmware flashing
  - Used for manual firmware updates via USB

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

- **production**: Standard firmware with maximum performance
- **debug**: Includes crash diagnostics and debug logging for troubleshooting

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
| LED solid purple | WiFi AP mode active - connect to "ESP-Bridge" network |
| LED orange slow blink | WiFi Client searching - check SSID spelling and network availability |
| LED solid orange | WiFi Client connected successfully |
| LED red fast blink | Wrong WiFi password - restart device or triple-click to change mode |
| Forgot WiFi password | Hold BOOT button 5+ seconds to reset to defaults |
| Can't connect to WiFi | Check network name, password, and 2.4GHz band (5GHz not supported) |
| WiFi connects then disconnects | Check router settings, signal strength, or power saving features |
| Connection drops | Enable flow control if supported by device |
| USB device not recognized | In Host mode, only CDC devices are supported |
| Application can't connect | Check USB cable, try different COM port settings |
| Connection resets repeatedly | Application toggling DTR/RTS - disable hardware flow control |
| Partial or corrupted data | Check baud rate settings, verify wire quality and grounding |
| "UART FIFO overflow" messages | This indicates USB cannot keep up with UART data rate. Only appears when COM port is open. Check adaptive buffer size matches your baud rate, consider enabling flow control, or reduce baud rate. |
| "USB: Dropping data - port not connected" | Normal behavior when COM port is not opened. Data is discarded to prevent buffer overflow. Open a serial terminal to receive data. |
| Can't access permanent network | Triple-click BOOT to enter temporary mode, then reconfigure |
| Network takes long to connect | Normal - device scans every 15s and attempts 5 connections when found |

## Performance Notes

The bridge features DMA-accelerated UART communication with adaptive buffering based on baud rate:

**What it actually does:**
- Uses hardware DMA for efficient data transfer without CPU involvement
- Dynamically adjusts buffer size based on baud rate (256-2048 bytes)
- Implements time-based packet detection using inter-byte gaps
- Works with any UART protocol without protocol-specific knowledge

**Buffer sizing:**
- 115200 baud: 288 bytes (384 with protocol optimization)
- 230400 baud: 768 bytes
- 460800 baud: 1024 bytes
- ≥921600 baud: 2048 bytes

**Technical implementation:**
- ESP-IDF native drivers with hardware DMA
- Hardware idle detection (10 character periods timeout)
- Event-driven architecture for Device 1 (main UART)
- Polling mode for Device 2/3 (secondary channels)
- Circular buffer with shadow buffer for protocol parsing
- Gap-based traffic detection with adaptive transmission thresholds

The hardware idle detection works in conjunction with software buffering - the UART controller detects inter-packet gaps and triggers DMA transfers, while the software layer groups these transfers based on timing for optimal USB transmission.

Current implementation achieves:
- Minimal packet loss under normal conditions
- Low latency for small packets
- Reliable operation up to 1000000 baud
- Reduced CPU usage thanks to DMA

**Protocol Pipeline Architecture:**
The bridge now uses a modern Parser + Sender architecture:
- **RAW mode**: Uses timing-based heuristics to group bytes (works with most protocols)
- **MAVLink mode**: Protocol-aware parsing with perfect packet boundaries
- **Memory Pool**: Efficient packet allocation prevents heap fragmentation
- **Multi-Sender**: Single parsed stream distributed to multiple output devices (USB, UART, UDP)
- **Priority System**: Critical packets bypass normal queuing for guaranteed delivery

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

### Circular Buffer Technology

The bridge uses advanced circular buffer architecture for optimal data handling:
- **Zero-copy operations**: Eliminates expensive memory copy operations
- **Scatter-gather I/O**: Efficient transmission of fragmented data
- **Traffic-aware buffering**: Automatic detection of normal vs burst traffic patterns
- **Smart dropping**: Prevents system hangs with intelligent data management
- **Protocol-aware**: Optimized packet boundary preservation for detected protocols

This implementation provides superior performance and reliability compared to traditional linear buffering, especially under high data loads and varying traffic patterns.

## Development

- **[CHANGELOG.md](CHANGELOG.md)** - Detailed version history and technical implementation details
- **[TODO.md](TODO.md)** - Future roadmap and current architecture documentation

## Technical Notes

- **Web Interface**: HTML files use gzip compression (70-78% size reduction) with preserved text readability. JavaScript/CSS remain minified for optimal performance.

## Acknowledgments

This project was developed with assistance from various AI systems, including Claude (Anthropic), ChatGPT (OpenAI), and other AI assistants. Their contributions helped with code optimization, documentation, and architectural decisions.

## License

MIT License - see LICENSE file for details