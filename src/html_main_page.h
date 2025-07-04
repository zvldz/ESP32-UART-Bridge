#ifndef HTML_MAIN_PAGE_H
#define HTML_MAIN_PAGE_H

#include <Arduino.h>

// Quick connection guide with integrated USB warning
const char HTML_QUICK_GUIDE[] PROGMEM = R"rawliteral(
<div class="section" style="padding: 10px 15px;">
<h3 style="margin: 10px 0 5px 0;">📡 Quick Connection Guide</h3>
<p style="margin: 5px 0;"><strong>UART Device → ESP32-C3:</strong> TX→GPIO4, RX→GPIO5, GND→GND</p>
<p style="margin: 5px 0;"><strong>USB Host:</strong> USB cable to ESP32-C3 (for any USB application)</p>
<div class="warning" style="font-size: 13px; margin: 10px 0 5px 0;">
<strong>⚠️ USB Serial Connection:</strong><br>
Closing serial terminal software or disconnecting from COM port will restart the ESP32-C3.<br>
This is hardware behavior of ESP32-C3/S2/S3 chips with native USB.<br>
To avoid restart: keep serial connection active during WiFi configuration.
</div>
<p style="margin: 5px 0;"><a href='/help' target='_blank'>📋 Detailed wiring diagram & troubleshooting</a></p>
</div>
)rawliteral";

// System status section start
const char HTML_SYSTEM_STATUS_START[] PROGMEM = R"rawliteral(
<div class="section"><h3>System Status</h3>
)rawliteral";

// System info table
const char HTML_SYSTEM_INFO_TABLE[] PROGMEM = R"rawliteral(
<table>
<tr><td><strong>Device:</strong></td><td>%DEVICE_NAME% v%VERSION%</td></tr>
<tr><td><strong>Free RAM:</strong></td><td id="freeRam">%FREE_RAM% bytes</td></tr>
<tr><td><strong>Uptime:</strong></td><td id="uptime">%UPTIME% seconds</td></tr>
<tr><td><strong>WiFi SSID:</strong></td><td>%WIFI_SSID%</td></tr>
<tr><td><strong>Current UART:</strong></td><td>%UART_CONFIG%</td></tr>
<tr><td><strong>Flow Control:</strong></td><td>%FLOW_CONTROL%</td></tr>
<tr><td><strong>Debug Mode:</strong></td><td>%DEBUG_MODE%</td></tr>
</table>
)rawliteral";

// Traffic stats table
const char HTML_TRAFFIC_STATS[] PROGMEM = R"rawliteral(
<table>
<tr><td><strong>UART→USB:</strong></td><td id="uartToUsb">%UART_TO_USB% bytes</td></tr>
<tr><td><strong>USB→UART:</strong></td><td id="usbToUart">%USB_TO_UART% bytes</td></tr>
<tr><td><strong>Total Traffic:</strong></td><td id="totalTraffic">%TOTAL_TRAFFIC% bytes</td></tr>
<tr><td><strong>Last Activity:</strong></td><td id="lastActivity">%LAST_ACTIVITY% seconds ago</td></tr>
</table>
<div style="text-align: center; margin: 15px 0;">
<button style="background-color: #ff9800;" onclick="if(confirm('Reset all statistics and logs?')) location.href='/reset_stats'">Reset Statistics</button>
</div>
</div>
)rawliteral";

// System logs section with icon button
const char HTML_SYSTEM_LOGS[] PROGMEM = R"rawliteral(
<div class="section"><h3>System Logs</h3>
<div style="position: relative;">
<button onclick="copyLogs()" style="position: absolute; top: 5px; right: 5px; padding: 5px 8px; font-size: 16px; background-color: #2196F3;" title="Copy logs to clipboard">📋</button>
<div class="log-container" id="logContainer">
<div id="logEntries">Loading logs...</div>
</div>
</div>
<p><small>Showing last %LOG_DISPLAY_COUNT% entries. Auto-updates every 5 seconds.</small></p>
</div>
)rawliteral";

// Combined settings form
const char HTML_DEVICE_SETTINGS[] PROGMEM = R"rawliteral(
<div class="section"><h3>Device Settings</h3>
<form action="/save" method="POST">
<h4>UART Configuration</h4>
<div style="display: flex; flex-wrap: wrap; gap: 10px; align-items: end;">
<div><label for="baudrate">Baud Rate:</label>
<select name="baudrate" id="baudrate" style="width: 100px;">
<option value="4800">4800</option>
<option value="9600">9600</option>
<option value="19200">19200</option>
<option value="38400">38400</option>
<option value="57600">57600</option>
<option value="74880">74880</option>
<option value="115200">115200</option>
<option value="230400">230400</option>
<option value="250000">250000</option>
<option value="460800">460800</option>
<option value="500000">500000</option>
<option value="921600">921600</option>
<option value="1000000">1000000</option>
</select></div>
<div><label for="databits">Data:</label>
<select name="databits" id="databits" style="width: 60px;">
<option value="7">7</option>
<option value="8">8</option>
</select></div>
<div><label for="parity">Parity:</label>
<select name="parity" id="parity" style="width: 80px;">
<option value="none">None</option>
<option value="even">Even</option>
<option value="odd">Odd</option>
</select></div>
<div><label for="stopbits">Stop:</label>
<select name="stopbits" id="stopbits" style="width: 60px;">
<option value="1">1</option>
<option value="2">2</option>
</select></div>
<div><label><input type="checkbox" name="flowcontrol" value="1"> Flow Control</label></div>
</div>
<h4 style="margin-top: 20px;">WiFi Configuration</h4>
<label for="ssid">WiFi SSID:</label>
<input type="text" name="ssid" id="ssid" value="%WIFI_SSID%" maxlength="32">
<label for="password">WiFi Password:</label>
<div style="position: relative;">
<input type="password" name="password" id="password" value="%WIFI_PASSWORD%" maxlength="64" style="padding-right: 40px;">
<span onclick="togglePassword()" style="position: absolute; right: 10px; top: 50%; transform: translateY(-50%); cursor: pointer; user-select: none;">👁️</span>
</div>
<div style="text-align: center; margin-top: 20px;">
<button type="submit" style="background-color: #4CAF50; font-size: 16px; padding: 12px 30px;">Save & Reboot</button>
</div>
</form></div>
)rawliteral";

// Firmware update section
const char HTML_FIRMWARE_UPDATE[] PROGMEM = R"rawliteral(
<div class="section"><h3>Firmware Update</h3>
<form method='POST' action='/update' enctype='multipart/form-data'>
<input type='file' name='update' accept='.bin'><br><br>
<input type='submit' value='Update Firmware' onclick='return confirm("Are you sure you want to update firmware?");'>
</form>
<p><small>Select .bin firmware file. Device will restart automatically after update.</small></p>
</div>
)rawliteral";

// JavaScript with fixed Last Activity handling
const char HTML_JAVASCRIPT[] PROGMEM = R"rawliteral(
<script>
document.getElementById('baudrate').value = '%BAUDRATE%';
document.getElementById('databits').value = '%DATABITS%';
document.getElementById('parity').value = '%PARITY%';
document.getElementById('stopbits').value = '%STOPBITS%';
document.querySelector('input[name="flowcontrol"]').checked = %FLOWCONTROL%;

function togglePassword() {
  var x = document.getElementById("password");
  if (x.type === "password") {
    x.type = "text";
  } else {
    x.type = "password";
  }
}

function copyLogs() {
  var logs = document.getElementById('logEntries').innerText;
  navigator.clipboard.writeText(logs).then(function() {
    alert('Logs copied to clipboard!');
  }, function(err) {
    console.error('Could not copy logs: ', err);
  });
}

function updateStatus() {
  fetch('/status').then(r => r.json()).then(data => {
    document.getElementById('freeRam').textContent = data.freeRam + ' bytes';
    document.getElementById('uptime').textContent = data.uptime + ' seconds';
    document.getElementById('uartToUsb').textContent = data.uartToUsb + ' bytes';
    document.getElementById('usbToUart').textContent = data.usbToUart + ' bytes';
    document.getElementById('totalTraffic').textContent = data.totalTraffic + ' bytes';
    
    // Handle both "Never" string and numeric seconds
    if (data.lastActivity === "Never") {
      document.getElementById('lastActivity').textContent = "Never";
    } else {
      document.getElementById('lastActivity').textContent = data.lastActivity + ' seconds ago';
    }
  });
}

function updateLogs() {
  fetch('/logs').then(r => r.json()).then(data => {
    const logContainer = document.getElementById('logEntries');
    logContainer.innerHTML = '';
    data.logs.forEach(log => {
      const entry = document.createElement('div');
      entry.className = 'log-entry';
      entry.textContent = log;
      logContainer.appendChild(entry);
    });
    const container = document.getElementById('logContainer');
    container.scrollTop = container.scrollHeight;
  });
}

updateLogs();
setInterval(updateStatus, 5000);
setInterval(updateLogs, 5000);
</script>
)rawliteral";

#endif // HTML_MAIN_PAGE_H