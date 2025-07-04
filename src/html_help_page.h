#ifndef HTML_HELP_PAGE_H
#define HTML_HELP_PAGE_H

#include <Arduino.h>

// Help page specific header (reuses icon from common)
const char HTML_HELP_HEADER[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<title>ESP32 UART Bridge - Connection Help</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta charset="UTF-8">
<link rel="icon" href="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><text x='50%' y='50%' font-size='12' text-anchor='middle' dominant-baseline='middle'>🔗</text></svg>">
)rawliteral";

const char HTML_HELP_BODY[] PROGMEM = R"rawliteral(
</head><body>
<div class="container">
<h1>🔗 ESP32 UART Bridge - Connection Help</h1>
)rawliteral";

const char HTML_HELP_PROTOCOL_INFO[] PROGMEM = R"rawliteral(
<div class="section">
<h3>📡 Protocol Optimization</h3>
<div class="success">
<strong>Universal UART Bridge:</strong> This bridge works with any UART protocol. It uses adaptive buffering that automatically optimizes for different data patterns. The system is particularly well-tuned for MAVLink protocol used by drone autopilots (ArduPilot, PX4), but works efficiently with any serial communication protocol.
</div>
<p><strong>Supported protocols:</strong></p>
<ul style="margin-left: 20px;">
<li><strong>MAVLink</strong> - Optimized for telemetry and parameter transfer</li>
<li><strong>NMEA GPS</strong> - Clean message boundaries detection</li>
<li><strong>Modbus RTU</strong> - Preserves inter-frame gaps</li>
<li><strong>AT Commands</strong> - Fast response for modem control</li>
<li><strong>Firmata</strong> - Low latency for microcontroller communication</li>
<li><strong>and other UART-based protocols</strong></li>
</ul>
<p style="margin-top: 10px;"><small>The adaptive buffering (200μs to 15ms) ensures optimal performance across different protocols and data rates.</small></p>
<p style="margin-top: 5px;"><small><strong>Note:</strong> The bridge works efficiently at all supported speeds. For MAVLink specifically, protocol-aware parsing could provide up to 20% improvement at very high speeds (≥460800 baud), though the current implementation already delivers excellent performance across the entire range.</small></p>
</div>
)rawliteral";

const char HTML_HELP_PIN_TABLE[] PROGMEM = R"rawliteral(
<div class="section">
<h3>Pin Connections</h3>
<table>
<tr><th>ESP32-C3 Pin</th><th>Function</th><th>Connect To</th><th>Required</th></tr>
<tr><td>GPIO4</td><td>UART RX</td><td>Device TX (UART/TELEM)</td><td>✅ Yes</td></tr>
<tr><td>GPIO5</td><td>UART TX</td><td>Device RX (UART/TELEM)</td><td>✅ Yes</td></tr>
<tr><td>GND</td><td>Ground</td><td>Device GND</td><td>✅ Yes</td></tr>
<tr><td>GPIO9</td><td>BOOT Button</td><td>Built-in</td><td>✅ Yes</td></tr>
<tr><td>GPIO8</td><td>Blue LED (controllable)</td><td>Built-in</td><td>✅ Yes</td></tr>
<tr><td>GPIO2</td><td>Red LED (power)</td><td>Built-in (always on)</td><td>✅ Yes</td></tr>
<tr><td>GPIO20</td><td>RTS</td><td>Flow Control</td><td>⚪ Optional</td></tr>
<tr><td>GPIO21</td><td>CTS</td><td>Flow Control</td><td>⚪ Optional</td></tr>
</table>
</div>
)rawliteral";

const char HTML_HELP_LED_BEHAVIOR[] PROGMEM = R"rawliteral(
<div class="section">
<h3>💡 LED Behavior Guide</h3>
<table>
<tr><th>LED</th><th>Normal Mode</th><th>WiFi Config Mode</th><th>Notes</th></tr>
<tr><td>Red LED (GPIO2)</td><td>Always ON</td><td>Always ON</td><td>Power indicator - not controllable</td></tr>
<tr><td>Blue LED (GPIO8)</td><td>Flashes on data</td><td>Constantly ON</td><td>Controllable - shows activity/status</td></tr>
</table>
<div class="success">
<strong>Normal Mode:</strong> Blue LED flashes briefly (50ms) when data is transferred in either direction<br>
<strong>WiFi Config Mode:</strong> Blue LED stays constantly ON while in configuration mode<br>
<strong>Triple-click detection:</strong> Blue LED blinks rapidly to show click count
</div>
</div>
)rawliteral";

const char HTML_HELP_CONNECTION_GUIDE[] PROGMEM = R"rawliteral(
<div class="section">
<h3>Connection Guide</h3>
<div style="background: #f8f9fa; padding: 20px; border-radius: 10px; text-align: center;">
<h4>Step 1: Connect UART wires</h4>
<p style="font-size: 18px; background: #e9ecef; padding: 10px; border-radius: 5px;">
Device TX → ESP32 GPIO4<br>
Device RX → ESP32 GPIO5<br>
Device GND → ESP32 GND
</p>
<h4>Step 2: Connect USB</h4>
<p style="font-size: 18px; background: #d1ecf1; padding: 10px; border-radius: 5px;">
ESP32 USB-C → Computer USB
</p>
<p><strong>Result:</strong> Transparent UART ↔ USB bridge ready!</p>
</div>
</div>
)rawliteral";

const char HTML_HELP_SETUP_INSTRUCTIONS[] PROGMEM = R"rawliteral(
<div class="section">
<h3>Setup Instructions</h3>
<div class="warning"><strong>⚠️ Important:</strong> Only connect signal wires and GND. Do not connect power between devices!</div>
<div class="warning" style="background-color: #ffebee; border-left-color: #f44336;">
<strong>⚡ Voltage Warning:</strong> ESP32-C3 GPIO pins support only 3.3V logic levels!<br>
Connecting 5V devices directly WILL damage the ESP32-C3.<br>
For 5V devices, you MUST use a level shifter (e.g., TXS0108E).
</div>
<ol>
<li><strong>Physical Connection:</strong> Connect TX→GPIO4, RX→GPIO5, GND→GND</li>
<li><strong>Device Settings:</strong> Configure UART protocol and baud rate on your device</li>
<li><strong>Bridge Settings:</strong> Use this web interface to configure UART speed</li>
<li><strong>USB Connection:</strong> Connect USB cable and select COM port in your application</li>
</ol>
</div>
)rawliteral";

const char HTML_HELP_TROUBLESHOOTING[] PROGMEM = R"rawliteral(
<div class="section">
<h3>Troubleshooting</h3>
<table>
<tr><th>Problem</th><th>Solution</th></tr>
<tr><td>No UART activity</td><td>Check TX/RX wiring, verify device UART settings</td></tr>
<tr><td>Application can't connect</td><td>Check USB connection, try different baud rate</td></tr>
<tr><td>Blue LED not flashing</td><td>No data activity - check connections and device settings</td></tr>
<tr><td>Unstable connection</td><td>Enable Flow Control, check wire quality</td></tr>
<tr><td>No WiFi config</td><td>Triple-click BOOT button, wait 30 seconds</td></tr>
<tr><td>Blue LED always on</td><td>Device is in WiFi config mode - normal behavior</td></tr>
</table>
</div>
)rawliteral";

const char HTML_HELP_BUTTONS[] PROGMEM = R"rawliteral(
<div class="section">
<button onclick="window.close()">Close Help</button>
<button onclick="location.href='/'">Back to Settings</button>
</div>
)rawliteral";

#endif // HTML_HELP_PAGE_H