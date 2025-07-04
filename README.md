# ESP32 UART Bridge

A high-performance, universal UART to USB bridge that works with any serial protocol or device. Features wireless configuration and intelligent data handling.

Transform any UART connection into a modern USB interface - no drivers, no complexity, just plug and play.

[![Version](https://img.shields.io/badge/version-2.1.0-blue)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-ESP32--C3-green)]()

---

## 🚀 Key Features

- **True Transparent Bridge** - Zero protocol interference
- **Multiple Operating Modes**:
  - **Normal Mode** - High-performance UART bridge (default)
  - **Config Mode** - WiFi AP for web-based configuration
  - **Debug Mode** - UART monitoring only, USB used for debug logs
- **Optimized for Low Latency** - UART task runs at highest priority
- **Adaptive Buffer Management** - Intelligent packet boundary detection
- **Configuration Without Drivers** - Web-based setup via WiFi
- **Wide Compatibility** - Works with any UART-based device
- **Visual Status Indicators** - Blue LED shows data activity and mode

---

## 📋 Quick Start

1. **Connect** your UART device:
   - Device TX → ESP32 GPIO4
   - Device RX → ESP32 GPIO5  
   - Device GND → ESP32 GND
   
2. **Connect** ESP32-C3 to USB port

3. **Configure** (optional):
   - Triple-click BOOT button for WiFi configuration
   - Connect to "ESP-Bridge" WiFi (password: 12345678)
   - Configuration page opens automatically (captive portal)
   - If not redirected, open http://192.168.4.1

4. **Use** like any USB-UART adapter!

---

## 🔧 Configuration & Web Interface

### Accessing Configuration

1. **Triple-click** the BOOT button (3 clicks within 3 seconds)
2. Blue LED stays **ON** indicating WiFi Config Mode
3. Connect to WiFi network "ESP-Bridge" (password: 12345678)
4. Web browser opens automatically via captive portal
5. Configure settings and save

### Configurable Parameters

- **UART Settings**
  - Baud Rate: 4800 to 1000000
  - Data Bits: 7 or 8
  - Parity: None, Even, Odd
  - Stop Bits: 1 or 2
  - Flow Control: RTS/CTS (optional)

- **WiFi Settings**
  - SSID (network name)
  - Password (8+ characters)

- **System Features**
  - View real-time statistics
  - Monitor data traffic
  - System logs with timestamps
  - Firmware update via web interface

### Default Configuration

- **UART**: 115200 baud, 8N1, no flow control
- **WiFi AP**: SSID "ESP-Bridge", Password "12345678"
- **Config Timeout**: 20 minutes
- **Buffer Size**: 256 bytes (adaptive)

---

## 📐 Hardware Connection

### Pin Mapping (ESP32-C3 SuperMini)

<div align="center">
<table>
<tr>
<td align="center" width="200"><strong>ESP32-C3</strong></td>
<td align="center" width="100">↔</td>
<td align="center" width="200"><strong>UART Device</strong></td>
</tr>
<tr>
<td align="right">GPIO4 (RX) <strong>←</strong></td>
<td align="center">⚠️ 3.3V</td>
<td align="left"><strong>→</strong> TX</td>
</tr>
<tr>
<td align="right">GPIO5 (TX) <strong>→</strong></td>
<td align="center">⚠️ 3.3V</td>
<td align="left"><strong>←</strong> RX</td>
</tr>
<tr>
<td align="right">GND <strong>—</strong></td>
<td align="center">required</td>
<td align="left"><strong>—</strong> GND</td>
</tr>
<tr>
<td colspan="3" align="center"><em>Optional Flow Control</em></td>
</tr>
<tr>
<td align="right">GPIO20 (RTS) <strong>→</strong></td>
<td align="center"></td>
<td align="left"><strong>←</strong> CTS</td>
</tr>
<tr>
<td align="right">GPIO21 (CTS) <strong>←</strong></td>
<td align="center"></td>
<td align="left"><strong>→</strong> RTS</td>
</tr>
</table>
</div>

<div align="center">
<strong>Built-in:</strong> GPIO8 (LED) • GPIO9 (BOOT Button)
</div>

#### Pin Details

<table>
<thead>
<tr>
<th>Pin</th>
<th>Function</th>
<th>Type</th>
<th>Connect To</th>
<th>Notes</th>
</tr>
</thead>
<tbody>
<tr>
<td><strong>GPIO4</strong></td>
<td>UART RX</td>
<td>Input</td>
<td>Device TX</td>
<td>⚠️ <strong>3.3V ONLY!</strong></td>
</tr>
<tr>
<td><strong>GPIO5</strong></td>
<td>UART TX</td>
<td>Output</td>
<td>Device RX</td>
<td>⚠️ <strong>3.3V ONLY!</strong></td>
</tr>
<tr>
<td><strong>GND</strong></td>
<td>Ground</td>
<td>-</td>
<td>Device GND</td>
<td><strong>Required</strong></td>
</tr>
<tr>
<td><strong>GPIO20</strong></td>
<td>RTS</td>
<td>Output</td>
<td>Device CTS</td>
<td><em>Optional flow control</em></td>
</tr>
<tr>
<td><strong>GPIO21</strong></td>
<td>CTS</td>
<td>Input</td>
<td>Device RTS</td>
<td><em>Optional flow control</em></td>
</tr>
<tr>
<td><strong>GPIO8</strong></td>
<td>Blue LED</td>
<td>Output</td>
<td><em>Built-in</em></td>
<td>Status indicator</td>
</tr>
<tr>
<td><strong>GPIO9</strong></td>
<td>BOOT Button</td>
<td>Input</td>
<td><em>Built-in</em></td>
<td>Mode control</td>
</tr>
</tbody>
</table>

### ⚡ Voltage Level Warning

**ESP32-C3 GPIO pins support only 3.3V logic levels!**

- ✅ **3.3V devices**: Direct connection
- ❌ **5V devices**: REQUIRES level shifter (e.g., TXS0108E)
- ⚠️ **Direct 5V connection WILL damage the ESP32!**

### ⚠️ USB Connection Warning

**Disconnecting serial terminal or USB cable WILL restart the ESP32-C3!**
- This is hardware behavior of ESP32-C3/S2/S3 with native USB
- Cannot be fixed in software
- Keep connection active during configuration

---

## 🔍 How It Works

### Performance-Oriented Architecture

The bridge design prioritizes UART data transfer:

- **UART Task** - Highest priority
  - Currently optimized for ESP32-C3 (single core)
  - Future versions will support Core 0 pinning on multi-core ESP32
  - Minimal latency data forwarding
  - Local variables to avoid mutex overhead
  
- **Web/WiFi Task** - Secondary priority
  - Only active in Config Mode (triple-click activation)
  - Runs with lower priority than UART task
  - Auto-shutdown after 20 minutes

- **Smart Resource Management**
  - Adaptive buffering optimizes throughput and latency
  - Thread-safe statistics without blocking UART
  - LED indication without performance impact

### Operating Modes

#### Normal Mode (Default)
- Full-speed UART↔USB bridge
- WiFi disabled for power efficiency
- Blue LED flashes on data activity
- Maximum performance, minimum latency

#### Config Mode
- Activated by triple-clicking BOOT button
- Creates WiFi AP "ESP-Bridge" 
- Captive portal auto-redirects to configuration
- Manual access: http://192.168.4.1 if needed
- UART bridge continues working at current settings
- Auto-exits after 20 minutes to save power

#### Debug Mode (Compile-time)
- Development tool only
- UART data is read but NOT forwarded to USB
- USB serial used exclusively for debug output
- Set DEBUG_MODE=1 in source code

### Adaptive Buffering System

The bridge uses intelligent buffering (200μs to 15ms timeouts) that automatically adapts to different data patterns:

- **Small packets** (≤12 bytes): 200μs timeout - minimal latency for commands
- **Medium packets** (≤64 bytes): 1ms timeout - balanced performance
- **Large transfers**: 5ms timeout or buffer full (256 bytes)
- **Safety timeout**: 15ms maximum to prevent excessive delays

This system works efficiently with:
- **Packet-based protocols** (MAVLink, Modbus RTU)
- **Stream protocols** (NMEA GPS, debug consoles)
- **Command-response protocols** (AT commands, Firmata)
- **High-frequency telemetry** (sensor data, real-time systems)

---

## 🛠️ Building the Project

### Using PlatformIO with VSCode

1. Install [VSCode](https://code.visualstudio.com/)
2. Install [PlatformIO Extension](https://platformio.org/install/ide?install=vscode)
3. Clone this repository
4. Open project folder in VSCode
5. PlatformIO will auto-install required packages
6. Click "Build" (✓) in PlatformIO toolbar
7. Click "Upload" (→) to flash to ESP32

### Required Configuration

The project is configured for ESP32-C3 SuperMini in `platformio.ini`. No changes needed for standard setup.

### Build Output

Firmware binary will be in `.pio/build/esp32-c3-supermini/firmware.bin`

---

## 📡 Common Use Cases

- **Device Configuration** - Setup embedded systems without proprietary cables
- **Telemetry Bridge** - Real-time data transfer between devices
- **Development & Debugging** - Universal serial console access
- **Protocol Analysis** - Monitor UART communication
- **System Integration** - Connect legacy UART devices to modern systems

---

## 📝 Development

See [TODO.md](TODO.md) for roadmap and planned features.

---

## 📦 Hardware Support

### Current Version
- **Target Board**: ESP32-C3 SuperMini
- **Fixed Pins**: TX=GPIO5, RX=GPIO4, RTS=GPIO20, CTS=GPIO21
- **USB**: Native USB CDC (no external chip needed)

### Future Support
- Other ESP32 variants (requires configurable pins)
- See [TODO.md](TODO.md) for details

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

---

## 📞 Support

- **Issues**: Use GitHub Issues for bug reports and feature requests
- **Discussions**: Use GitHub Discussions for questions and ideas

---

*Built with ❤️ for the maker community*