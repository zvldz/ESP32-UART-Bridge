# ESP32 UART Bridge

[![PlatformIO](https://img.shields.io/badge/PlatformIO-5.0%2B-blue)](https://platformio.org/)
[![ESP32](https://img.shields.io/badge/ESP32-S3-green)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Board](https://img.shields.io/badge/Board-Waveshare_S3_Zero-blue)](https://www.waveshare.com/wiki/ESP32-S3-Zero)
[![Version](https://img.shields.io/badge/Version-2.5.0-brightgreen)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Universal UART to USB bridge with web configuration interface for any serial communication needs.

> **⚠️ BETA SOFTWARE**: This is a beta version and may not work stably in all configurations. Please report any issues you encounter.

## Features

- **Universal Protocol Support**: Works with any UART-based protocol - industrial (Modbus RTU), IoT (AT Commands), navigation (NMEA GPS), robotics (MAVLink), and more
- **DMA-Accelerated Performance**: Hardware DMA with ESP-IDF drivers for minimal CPU usage and zero packet loss
- **Smart Protocol Detection**: Adaptive buffering automatically optimizes for different data patterns
  - Binary protocols (MAVLink, Modbus RTU): Preserves packet boundaries
  - Text protocols (NMEA, AT commands): Line-based optimization
  - Stream protocols: Minimal latency mode
- **Dynamic Buffer Sizing**: Automatically adjusts buffer size based on baud rate (256-2048 bytes)
- **USB Host/Device Modes**: 
  - Device mode: Connect ESP32 to PC as USB serial device
  - Host mode: Connect USB modems or serial adapters to ESP32
  - Configurable via web interface
- **High Performance**: Hardware packet detection and event-driven architecture
- **Web Configuration**: Easy setup via WiFi access point
- **Visual Feedback**: RGB LED shows data flow direction and system status
- **Wide Speed Range**: 4800 to 1000000 baud
- **Flow Control**: Hardware RTS/CTS support
- **Crash Logging**: Automatic crash detection and logging for diagnostics
- **Flexible Logging**: Multi-level logging system with web interface
- **OTA Updates**: Firmware updates via web interface

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
  - GPIO4: UART RX (connect to device TX)
  - GPIO5: UART TX (connect to device RX)  
  - GPIO21: RGB LED (WS2812 - built-in on ESP32-S3-Zero)
  - GPIO0: BOOT button (built-in)
  - GPIO6/7: RTS/CTS (optional flow control)

## Quick Start

1. **Connect Hardware**:
   - Device TX → ESP32 GPIO4
   - Device RX → ESP32 GPIO5
   - Device GND → ESP32 GND
   - ⚠️ **Warning**: ESP32-S3 supports only 3.3V logic levels!

2. **Power On**:
   - Connect USB-C cable to computer
   - Rainbow LED effect indicates successful boot

3. **Configure** (first time only):
   - Triple-click BOOT button (LED turns solid purple)
   - Connect to WiFi network "ESP-Bridge" (password: 12345678)
   - Open web browser to 192.168.4.1
   - Set your UART parameters and WiFi credentials
   - Choose USB mode (Device/Host)
   - Click "Save & Reboot"

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

## Common Use Cases

- **Industrial Automation**: Connect Modbus RTU devices to modern systems
- **IoT Connectivity**: Bridge 4G/LTE USB modems to embedded devices
- **Vehicle Telemetry**: MAVLink communication for drones, rovers, boats
- **Marine Electronics**: NMEA GPS/AIS data bridging
- **Legacy Equipment**: Modernize RS232/RS485 equipment
- **Development**: Debug embedded systems with USB convenience

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
To enter WiFi configuration mode and change settings (including USB mode):
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

## LED Indicators

| Color | Pattern | Meaning |
|-------|---------|---------|
| Blue | Flash | Data from device (UART RX) |
| Green | Flash | Data from computer (USB RX) |
| Cyan | Flash | Bidirectional data transfer |
| Purple | Solid | WiFi configuration mode |
| Purple | Rapid blink | WiFi reset confirmation |
| White | Blinks | Button click feedback |
| Rainbow | 1 second | Boot sequence |
| Off | - | Idle, no data |

## Button Functions

- **Triple-click**: Enter WiFi configuration mode
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
- **Modbus RTU**: Requires precise inter-frame timing (automatically handled)

## Troubleshooting

| Problem | Solution |
|---------|----------|
| No data activity | Check TX/RX connections, verify baud rate matches device |
| LED solid purple | Device is in WiFi config mode - connect to "ESP-Bridge" network |
| Forgot WiFi password | Hold BOOT button 5+ seconds to reset to defaults |
| Connection drops | Enable flow control if supported by device |
| USB device not recognized | In Host mode, only CDC devices are supported |
| Application can't connect | Check USB cable, try different COM port settings |
| Connection resets repeatedly | Application toggling DTR/RTS - disable hardware flow control |
| Partial or corrupted data | Protocol timing requirements - try different buffer modes |

## Performance Notes

The bridge features DMA-accelerated UART communication with intelligent adaptive buffering that automatically detects and optimizes for your protocol:

**Binary/Packet Protocols:**
- MAVLink: Hardware packet boundary detection for efficient telemetry and parameter transfer
- Modbus RTU: Maintains critical 3.5 character silence periods
- Custom binary: Learns packet patterns automatically

**Text/Line Protocols:**
- NMEA sentences: Buffers complete lines ($GPGGA...*checksum)
- AT commands: Optimizes for command/response patterns
- Debug output: Preserves line endings

**Technical Details:**
- ESP-IDF native drivers with hardware DMA
- Dynamic buffer allocation (256-2048 bytes based on baud rate)
- Zero-copy ring buffers (16KB for main UART)
- Hardware packet timeout detection
- Event-driven architecture for minimal latency

Current implementation achieves:
- Zero packet loss under normal conditions
- Sub-millisecond latency for small packets
- Reliable operation up to 1000000 baud
- Minimal CPU usage thanks to DMA

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
- **UART Output**: Optional logging to Device 3 UART TX pin
- **Network Logging**: Future support for UDP log streaming

### Adaptive Buffering Technology

The bridge continuously monitors data patterns and adjusts its behavior:
- Detects packet boundaries in binary protocols
- Recognizes line endings in text protocols  
- Measures inter-byte gaps for timeout-sensitive protocols
- Switches between latency and throughput optimization modes

This means optimal performance whether you're transferring MAVLink parameters, streaming GPS coordinates, or controlling industrial equipment.

## Acknowledgments

This project was developed with assistance from various AI systems, including Claude (Anthropic), ChatGPT (OpenAI), and other AI assistants. Their contributions helped with code optimization, documentation, and architectural decisions.

## License

MIT License - see LICENSE file for details