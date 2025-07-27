# ESP32 UART Bridge

[![PlatformIO](https://img.shields.io/badge/PlatformIO-5.0%2B-blue)](https://platformio.org/)
[![ESP32](https://img.shields.io/badge/ESP32-S3-green)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Board](https://img.shields.io/badge/Board-Waveshare_S3_Zero-blue)](https://www.waveshare.com/wiki/ESP32-S3-Zero)
[![Version](https://img.shields.io/badge/Version-2.7.2-brightgreen)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Universal UART to USB bridge with web configuration interface for any serial communication needs.

> **⚠️ BETA SOFTWARE**: This is a beta version and may not work stably in all configurations. Please report any issues you encounter.

## Features

- **Universal Protocol Support**: Works with any UART-based protocol - industrial (Modbus RTU), IoT (AT Commands), navigation (NMEA GPS), robotics (MAVLink), and more
- **DMA-Accelerated Performance**: Hardware DMA with ESP-IDF drivers for minimal CPU usage and minimal packet loss
- **Adaptive Buffering**: Automatically adjusts based on baud rate and inter-byte timing
  - Uses configurable timeout thresholds (200μs/1ms/5ms/15ms)
  - Groups bytes into packets based on inter-byte gaps
- **Dynamic Buffer Sizing**: Automatically adjusts buffer size based on baud rate (256-2048 bytes)
- **USB Host/Device Modes**: 
  - Device mode: Connect ESP32 to PC as USB serial device
  - Host mode: Connect USB modems or serial adapters to ESP32
  - Configurable via web interface
- **High Performance**: Hardware packet detection (UART idle timeout) and event-driven architecture (Device 1)
- **Web Configuration**: Easy setup via WiFi access point
  - **Permanent Network Mode**: Wi-Fi remains active permanently for always-on access
  - **Temporary Setup Mode**: Wi-Fi timeout for quick configuration (triple-click activation)
- **Visual Feedback**: RGB LED shows data flow direction and system status
- **Wide Speed Range**: 4800 to 1000000 baud
- **Flow Control**: Hardware RTS/CTS support
- **Crash Logging**: Automatic crash detection and logging for diagnostics
- **Flexible Logging**: Multi-level logging system with web interface
- **OTA Updates**: Firmware updates via web interface

## Current Limitations

- **Device 3 Adaptive Buffering**: Not yet implemented (uses simple 64-byte blocks)
- **Packet Boundaries**: Currently preserved only for Device 1 → Device 2 path
- **Protocol Detection**: No automatic protocol detection (uses timing-based optimization)
- **USB Buffer**: Can overflow at very high sustained data rates
- **Device 2/3**: Use polling mode instead of event-driven architecture
- **Device 4**: UDP only (TCP support planned for future versions)

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

## Network Operation Modes

### Temporary Setup Mode (Triple-Click)
- Activated by triple-clicking BOOT button
- Wi-Fi AP active for limited time (default timeout)
- Designed for quick configuration changes
- Returns to standalone mode automatically
- LED: Solid purple during network mode

### Permanent Network Mode
- Configured via web interface checkbox
- Wi-Fi remains active indefinitely
- No timeout - stays connected until manually disabled
- Connects to your existing Wi-Fi network
- LED: Solid purple when network active

### Switching Between Modes
- **To Temporary**: Triple-click BOOT button anytime
- **To Permanent**: Enable via web interface in WiFi Configuration
- **To Standalone**: Disable permanent mode via web interface

## Common Use Cases

- **Industrial Automation**: Connect Modbus RTU devices to modern systems
- **IoT Connectivity**: Bridge 4G/LTE USB modems to embedded devices
- **Vehicle Telemetry**: MAVLink communication for drones, rovers, boats
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
- **WiFi Settings**: SSID and password for your network
- **USB Mode**: Device (default) or Host mode
- **Network Mode**: Temporary setup or permanent Wi-Fi operation
- **Device Roles**: Configure Device 2, 3, and 4 functionality

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

| Color | Pattern | Meaning |
|-------|---------|---------|
| Blue | Flash | Data from device (UART RX) |
| Green | Flash | Data from computer (USB RX) |
| Cyan | Flash | Bidirectional data transfer |
| Purple | Solid | Network mode (temporary or permanent) |
| Purple | Rapid blink | WiFi reset confirmation |
| White | Blinks | Button click feedback |
| Rainbow | 1 second | Boot sequence |
| Off | - | Idle, no data |

## Button Functions

- **Triple-click**: Enter temporary network mode
- **Hold 5+ seconds**: Reset WiFi to defaults (SSID: ESP-Bridge, Password: 12345678)
- **Hold during power-on**: Enter bootloader mode for firmware flashing

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
- **MAVLink devices**: Ensure baud rate matches autopilot settings (typically 57600 or 115200)
- **GPS/NMEA**: Most GPS modules use 9600 or 115200 baud
- **AT command modems**: May require specific line endings (CR, LF, or CR+LF)
- **Modbus RTU**: Uses strict timing requirements - the bridge's timeout-based buffering may work at some baud rates but is not specifically optimized for Modbus

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
| LED solid purple | Device is in network mode - connect to "ESP-Bridge" network or your configured WiFi |
| Forgot WiFi password | Hold BOOT button 5+ seconds to reset to defaults |
| Connection drops | Enable flow control if supported by device |
| USB device not recognized | In Host mode, only CDC devices are supported |
| Application can't connect | Check USB cable, try different COM port settings |
| Connection resets repeatedly | Application toggling DTR/RTS - disable hardware flow control |
| Partial or corrupted data | Check baud rate settings, verify wire quality and grounding |
| Can't access permanent network | Triple-click BOOT to enter temporary mode, then reconfigure |

## Performance Notes

The bridge features DMA-accelerated UART communication with adaptive buffering based on baud rate:

**What it actually does:**
- Uses hardware DMA for efficient data transfer without CPU involvement
- Dynamically adjusts buffer size based on baud rate (256-2048 bytes)
- Implements time-based packet detection using inter-byte gaps
- Works with any UART protocol without protocol-specific knowledge

**Buffer sizing:**
- ≤19200 baud: 256 bytes
- 115200 baud: 288 bytes (optimized to reduce USB bottlenecks)
- 230400 baud: 768 bytes
- 460800 baud: 1024 bytes
- ≥921600 baud: 2048 bytes

**Technical implementation:**
- ESP-IDF native drivers with hardware DMA
- Hardware idle detection (10 character periods timeout)
- Event-driven architecture for Device 1 (main UART)
- Polling mode for Device 2/3 (secondary channels)
- Zero-copy ring buffers (16KB)
- Software timeout thresholds (200μs/1ms/5ms/15ms) for adaptive buffering

The hardware idle detection works in conjunction with software buffering - the UART controller detects inter-packet gaps and triggers DMA transfers, while the software layer groups these transfers based on timing for optimal USB transmission.

Current implementation achieves:
- Minimal packet loss under normal conditions
- Low latency for small packets
- Reliable operation up to 1000000 baud
- Reduced CPU usage thanks to DMA

**Note:** The bridge does NOT parse or understand specific protocols. It uses timing-based heuristics to group bytes into packets, which works well for most protocols but is not protocol-aware.

## Advanced Features

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

### Adaptive Buffering Technology

The bridge uses time-based buffering that groups bytes into packets based on inter-byte gaps:
- Monitors timing between received bytes
- Groups bytes that arrive close together (within timeout thresholds)
- Sends grouped data when gaps exceed the threshold
- Buffer size automatically adjusts based on configured baud rate

This approach works well for many protocols without requiring protocol-specific knowledge. The timing thresholds (200μs/1ms/5ms/15ms) help preserve packet boundaries for protocols that use inter-frame gaps.

## Development

- **[CHANGELOG.md](CHANGELOG.md)** - Detailed version history and technical implementation details
- **[TODO.md](TODO.md)** - Future roadmap and current architecture documentation

## Acknowledgments

This project was developed with assistance from various AI systems, including Claude (Anthropic), ChatGPT (OpenAI), and other AI assistants. Their contributions helped with code optimization, documentation, and architectural decisions.

## License

MIT License - see LICENSE file for details