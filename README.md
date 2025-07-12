# ESP32 UART Bridge

Universal UART to USB bridge with web configuration interface. Optimized for drone autopilots (ArduPilot, PX4) but works with any UART protocol.

## Features

- **Universal Protocol Support**: Works with MAVLink, NMEA GPS, Modbus RTU, AT Commands, and any UART-based protocol
- **High Performance**: Adaptive buffering (200μs to 15ms) for optimal throughput
- **Web Configuration**: Easy setup via WiFi access point
- **Visual Feedback**: RGB LED shows data flow direction and system status
- **Wide Speed Range**: 4800 to 1000000 baud
- **Flow Control**: Hardware RTS/CTS support
- **Crash Logging**: Automatic crash detection and logging for diagnostics
- **OTA Updates**: Firmware updates via web interface

## Hardware

- **Board**: ESP32-S3-Zero or compatible ESP32-S3 development board
- **Connections**:
  - GPIO4: UART RX (connect to device TX)
  - GPIO5: UART TX (connect to device RX)
  - GPIO21: RGB LED (WS2812 - built-in)
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
   - Click "Save & Reboot"

4. **Use**:
   - Device appears as COM port on computer
   - LED flashes indicate data flow:
     - Blue: Device → Computer
     - Green: Computer → Device
     - Cyan: Bidirectional

## Connection Issues with Ground Control Software

Some ground control applications may experience connection problems due to ESP32-S3's native USB bootloader messages. The bootloader outputs diagnostic information that can be misinterpreted as data, causing reset loops.

**Mission Planner:** Enable "Disable RTS resets on ESP32 SerialUSB" option in the connection settings to prevent reset loops.

**QGroundControl:** Generally handles ESP32 connections well without special configuration.

**Other applications:** If you experience connection issues, look for options to disable DTR/RTS control or add connection delays.

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
- ESP32 platform support

### Build Instructions
1. Clone repository
2. Open in VSCode with PlatformIO
3. Select environment:
   - `production` - For normal use with Mission Planner
   - `production_debug` - With crash diagnostics
   - `full_debug` - Complete debug output
4. Build and upload

## Troubleshooting

| Problem | Solution |
|---------|----------|
| No data activity | Check TX/RX connections, verify baud rate matches device |
| LED solid purple | Device is in WiFi config mode - connect to "ESP-Bridge" network |
| Forgot WiFi password | Hold BOOT button 5+ seconds to reset to defaults |
| Connection drops | Enable flow control if supported by device |
| Application can't connect | Check USB cable, try different COM port settings |

## Performance Notes

The bridge uses adaptive buffering that automatically optimizes for different protocols:
- **MAVLink**: Optimized for telemetry and parameter transfer
- **NMEA GPS**: Clean message boundary detection
- **Modbus RTU**: Preserves inter-frame gaps
- **AT Commands**: Low latency for modem control

## License

MIT License - see LICENSE file for details