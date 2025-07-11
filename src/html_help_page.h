#ifndef HTML_HELP_PAGE_H
#define HTML_HELP_PAGE_H

#include <Arduino.h>

// Help page specific title
const char HTML_HELP_TITLE[] PROGMEM = R"rawliteral(
<title>ESP32 UART Bridge - Connection Help</title>
)rawliteral";

// Help page heading
const char HTML_HELP_HEADING[] PROGMEM = R"rawliteral(
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
<tr><th>ESP32-S3 Pin</th><th>Function</th><th>Connect To</th><th>Required</th></tr>
<tr><td>GPIO4</td><td>UART RX</td><td>Device TX (UART/TELEM)</td><td>✅ Yes</td></tr>
<tr><td>GPIO5</td><td>UART TX</td><td>Device RX (UART/TELEM)</td><td>✅ Yes</td></tr>
<tr><td>GND</td><td>Ground</td><td>Device GND</td><td>✅ Yes</td></tr>
<tr><td>GPIO0</td><td>BOOT Button</td><td>Built-in</td><td>✅ Yes</td></tr>
<tr><td>GPIO21</td><td>RGB LED</td><td>Built-in (WS2812)</td><td>✅ Yes</td></tr>
<tr><td>GPIO6</td><td>RTS</td><td>Flow Control</td><td>⚪ Optional</td></tr>
<tr><td>GPIO7</td><td>CTS</td><td>Flow Control</td><td>⚪ Optional</td></tr>
</table>
</div>
)rawliteral";

const char HTML_HELP_LED_BEHAVIOR[] PROGMEM = R"rawliteral(
<div class="section">
<h3>💡 LED Status Indicators</h3>
<div class="success">
<strong>The RGB LED (GPIO21) provides visual feedback:</strong><br>
- <strong>Blue flashes</strong> - Data from device (UART RX)<br>
- <strong>Green flashes</strong> - Data from computer (USB RX)<br>
- <strong>Cyan flashes</strong> - Bidirectional data transfer<br>
- <strong>Solid purple</strong> - WiFi configuration mode<br>
- <strong>White blinks</strong> - Button click feedback (1-2 clicks)<br>
- <strong>Purple rapid blinking</strong> - WiFi reset confirmation (5s hold)<br>
- <strong>Rainbow effect</strong> - Boot sequence (1 second)<br>
- <strong>Off</strong> - Idle, no data transfer
</div>
<table>
<tr><th>Mode</th><th>LED State</th><th>Description</th></tr>
<tr><td>Normal Mode</td><td>Blue/Green/Cyan flashes</td><td>50ms flash per data activity</td></tr>
<tr><td>WiFi Config Mode</td><td>Solid purple</td><td>Constantly ON while in configuration mode</td></tr>
<tr><td>Boot Sequence</td><td>Rainbow effect</td><td>Colorful startup animation for 1 second</td></tr>
<tr><td>Button clicks</td><td>White blinks</td><td>Shows click count (100ms per blink)</td></tr>
</table>
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
<strong>⚡ Voltage Warning:</strong> ESP32-S3 GPIO pins support only 3.3V logic levels!<br>
Connecting 5V devices directly WILL damage the ESP32-S3.<br>
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
<tr><td>LED not flashing</td><td>No data activity - check connections and device settings</td></tr>
<tr><td>Unstable connection</td><td>Enable Flow Control, check wire quality</td></tr>
<tr><td>No WiFi config</td><td>Triple-click BOOT button, wait for solid purple LED</td></tr>
<tr><td>LED solid purple</td><td>Device is in WiFi config mode - normal behavior</td></tr>
<tr><td>Forgot WiFi password</td><td>Hold BOOT button for 5+ seconds to reset WiFi to defaults</td></tr>
<tr><td>Frequent crashes</td><td>Check Crash History on main page for patterns</td></tr>
</table>
</div>

<div class="section">
<h3>🔘 Button Functions</h3>
<div class="success">
<strong>BOOT Button (GPIO0) Functions:</strong><br>
- <strong>Triple-click (3 clicks within 3 seconds):</strong> Enter WiFi configuration mode<br>
- <strong>Hold 5+ seconds:</strong> Reset WiFi settings to defaults (SSID: ESP-Bridge, Password: 12345678)<br>
- <strong>Hold during power-on:</strong> Enter bootloader mode for firmware update<br>
<br>
<strong>WiFi Reset Procedure:</strong><br>
1. Press and hold BOOT button for at least 5 seconds<br>
2. LED will flash purple rapidly 10 times to confirm<br>
3. Device will restart with default WiFi settings<br>
4. Connect to "ESP-Bridge" network with password "12345678"
</div>
</div>
)rawliteral";

const char HTML_HELP_BUTTONS[] PROGMEM = R"rawliteral(
<div class="section">
<h3>🔍 Crash Diagnostics</h3>
<div class="success">
<strong>The device automatically logs system crashes to help diagnose issues:</strong><br>
<br>
<strong>What is logged:</strong><br>
- Reset reason (PANIC, TASK_WDT, etc.)<br>
- Uptime before crash<br>
- Free heap memory<br>
- Minimum heap during session<br>
<br>
<strong>Viewing crash history:</strong><br>
1. Go to the main configuration page<br>
2. Look for "Crash History" section below System Logs<br>
3. Click to expand and view last 10 crashes<br>
4. Red badge shows total crash count<br>
<br>
<strong>Common crash patterns:</strong><br>
- Crashes at similar uptime → Possible memory leak<br>
- Low heap values → Out of memory<br>
- TASK_WDT → Task blocked (often USB disconnect)<br>
- PANIC → Software exception<br>
</div>
</div>

<div class="section">
<button onclick="window.close()">Close Help</button>
<button onclick="location.href='/'">Back to Settings</button>
</div>
)rawliteral";

#endif // HTML_HELP_PAGE_H