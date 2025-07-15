#ifndef HTML_MAIN_PAGE_H
#define HTML_MAIN_PAGE_H

#include <Arduino.h>

// Main page specific title
const char HTML_MAIN_TITLE[] PROGMEM = R"rawliteral(
<title>ESP32 UART Bridge</title>
)rawliteral";

// Main page heading
const char HTML_MAIN_HEADING[] PROGMEM = R"rawliteral(
<h1>🔗 ESP32 UART Bridge</h1>
)rawliteral";

// Quick connection guide with updated USB note
const char HTML_QUICK_GUIDE[] PROGMEM = R"rawliteral(
<div class="section" style="padding: 10px 15px;">
<h3 style="margin: 10px 0 5px 0;">📡 Quick Connection Guide</h3>
<p style="margin: 5px 0;"><strong>UART Device → ESP32-S3:</strong> TX→GPIO4, RX→GPIO5, GND→GND</p>
<p style="margin: 5px 0;"><strong>USB Host:</strong> USB cable to ESP32-S3 (for any USB application)</p>
<div class="status" style="font-size: 13px; margin: 10px 0 5px 0;">
<strong>📌 Mission Planner Users:</strong><br>
Enable "Disable RTS resets on ESP32 SerialUSB" in connection options to prevent reset loops.
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

// System logs section with minimal SVG icon button
const char HTML_SYSTEM_LOGS[] PROGMEM = R"rawliteral(
<div class="section"><h3>System Logs</h3>
<div style="position: relative;">
<span id="copyBtn" onclick="copyLogs()" style="position: absolute; top: 5px; right: 15px; cursor: pointer; user-select: none; display: flex; align-items: center; padding: 5px;" title="Copy logs to clipboard">
<svg width="20" height="20" viewBox="0 0 20 20" fill="none" stroke="#666" stroke-width="1.5">
<rect x="6" y="6" width="10" height="12" rx="1"></rect>
<path d="M14 4V3a1 1 0 00-1-1H5a1 1 0 00-1 1v12a1 1 0 001 1h1"></path>
</svg>
</span>
<div class="log-container" id="logContainer">
<div id="logEntries">Loading logs...</div>
</div>
</div>
<p><small>Showing last %LOG_DISPLAY_COUNT% entries. Auto-updates every 5 seconds.</small></p>
</div>
)rawliteral";

// Crash history section
const char HTML_CRASH_HISTORY[] PROGMEM = R"rawliteral(
<div class="section" style="padding: 10px 15px;">
<h3 style="display: flex; justify-content: space-between; align-items: center; cursor: pointer; margin: 10px 0;" onclick="toggleCrashLog()">
  <span>💥 Crash History</span>
  <span style="display: flex; align-items: center; gap: 10px;">
    <span id="crashBadge" class="badge" style="background: #4CAF50; color: white; padding: 2px 8px; border-radius: 12px; font-size: 14px; font-weight: bold; min-width: 20px; text-align: center;">0</span>
    <span id="crashClear" onclick="event.stopPropagation(); clearCrashHistory();" title="Clear all crash history" style="cursor: pointer; display: none; opacity: 0.6; padding: 4px; transition: all 0.2s;">
      <svg width="16" height="16" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5">
        <path d="M3 3h10l-1 10H4L3 3z"></path>
        <path d="M6 3V2a1 1 0 011-1h2a1 1 0 011 1v1"></path>
        <path d="M2 3h12"></path>
        <path d="M6 6v4M10 6v4"></path>
      </svg>
    </span>
    <span id="crashArrow" style="user-select: none; font-size: 18px; transition: transform 0.3s;">▼</span>
  </span>
</h3>
<div id="crashContent" style="display: none; overflow: hidden; transition: max-height 0.3s ease-out; margin-top: 10px;">
  <div class="log-container" style="height: 150px;">
    <table style="width: 100%; font-size: 12px;">
      <thead>
        <tr><th style="width: 10%;">#</th><th style="width: 25%;">Type</th><th style="width: 25%;">Uptime</th><th style="width: 20%;">Heap</th><th style="width: 20%;">Min</th></tr>
      </thead>
      <tbody id="crashTableBody">
        <tr><td colspan="5" style="text-align: center;">Loading...</td></tr>
      </tbody>
    </table>
  </div>
</div>
</div>

<style>
#crashClear:hover {
  opacity: 1 !important;
  transform: scale(1.1);
}
#crashClear:hover svg {
  stroke: #f44336;
}
.badge.warning { background: #ff9800 !important; }
.badge.danger { background: #f44336 !important; }
.crash-panic { color: #d32f2f; font-weight: bold; }
.crash-wdt { color: #f57c00; font-weight: bold; }
.heap-critical { background: #fff3cd; }
</style>
)rawliteral";

// Combined settings form with SVG password toggle - FIXED align-items
const char HTML_DEVICE_SETTINGS[] PROGMEM = R"rawliteral(
<div class="section"><h3>Device Settings</h3>
<form action="/save" method="POST">
<h4>UART Configuration</h4>
<div style="display: flex; flex-wrap: wrap; gap: 10px; align-items: flex-end;">
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
<h4 style="margin-top: 20px;">USB Mode</h4>
<div style="margin: 10px 0;">
<select name="usbmode" id="usbmode" style="width: 200px;">
  <option value="device">Device (Connect to PC)</option>
  <option value="host">Host (Connect USB Modem)</option>
  <option value="auto">Auto-detect</option>
</select>
</div>
<h4 style="margin-top: 20px;">WiFi Configuration</h4>
<label for="ssid">WiFi SSID:</label>
<input type="text" name="ssid" id="ssid" value="%WIFI_SSID%" maxlength="32">
<label for="password">WiFi Password:</label>
<div style="position: relative;">
<input type="password" name="password" id="password" value="%WIFI_PASSWORD%" maxlength="64" style="padding-right: 40px;">
<span id="toggleIcon" onclick="togglePassword()" style="position: absolute; right: 10px; top: 50%; transform: translateY(-50%); cursor: pointer; user-select: none; display: flex; align-items: center;">
<svg width="20" height="20" viewBox="0 0 20 20" fill="none" stroke="#666" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
<path d="M1 10s3-6 9-6 9 6 9 6-3 6-9 6-9-6-9-6z"></path>
<circle cx="10" cy="10" r="3"></circle>
</svg>
</span>
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

// JavaScript with improved copy function and password toggle - using safeFetch
const char HTML_JAVASCRIPT[] PROGMEM = R"rawliteral(
<script>
document.getElementById('baudrate').value = '%BAUDRATE%';
document.getElementById('databits').value = '%DATABITS%';
document.getElementById('parity').value = '%PARITY%';
document.getElementById('stopbits').value = '%STOPBITS%';
document.querySelector('input[name="flowcontrol"]').checked = %FLOWCONTROL%;

// Set USB mode in dropdown
document.getElementById('usbmode').value = '%USB_MODE%';

function togglePassword() {
  var x = document.getElementById("password");
  var icon = document.getElementById("toggleIcon");

  if (x.type === "password") {
    x.type = "text";
    // Eye with slash (hidden)
    icon.innerHTML = '<svg width="20" height="20" viewBox="0 0 20 20" fill="none" stroke="#666" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M13.875 13.875A8.5 8.5 0 016.125 6.125m-3.27.73A9.98 9.98 0 001 10s3 6 9 6a9.98 9.98 0 003.145-.855M8.29 4.21A4.95 4.95 0 0110 4c6 0 9 6 9 6a17.46 17.46 0 01-2.145 2.855M1 1l18 18"></path></svg>';
  } else {
    x.type = "password";
    // Open eye
    icon.innerHTML = '<svg width="20" height="20" viewBox="0 0 20 20" fill="none" stroke="#666" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M1 10s3-6 9-6 9 6 9 6-3 6-9 6-9-6-9-6z"></path><circle cx="10" cy="10" r="3"></circle></svg>';
  }
}

function copyLogs() {
  const logEntriesElement = document.getElementById('logEntries');
  if (!logEntriesElement) {
    return;
  }

  const logs = logEntriesElement.innerText || logEntriesElement.textContent || '';
  if (!logs || logs === 'Loading logs...') {
    return;
  }

  // Create textarea for copy operation
  const textArea = document.createElement("textarea");
  textArea.value = logs;
  textArea.style.position = "fixed";
  textArea.style.left = "-999999px";
  document.body.appendChild(textArea);
  textArea.select();

  try {
    document.execCommand('copy');
    // Visual feedback
    const icon = document.getElementById('copyBtn').querySelector('svg');
    const originalStroke = icon.getAttribute('stroke');
    const originalContent = icon.innerHTML;

    icon.setAttribute('stroke', '#4CAF50');
    icon.innerHTML = '<polyline points="5 10 8 13 15 6"></polyline>';

    setTimeout(function() {
      icon.setAttribute('stroke', originalStroke);
      icon.innerHTML = originalContent;
    }, 1500);
  } catch (err) {
    console.error('Copy failed:', err);
  }

  document.body.removeChild(textArea);
}

// Crash log functions
function toggleCrashLog() {
  var content = document.getElementById('crashContent');
  var arrow = document.getElementById('crashArrow');

  if (content.style.display === 'none') {
    content.style.display = 'block';
    arrow.style.transform = 'rotate(180deg)';
    loadCrashLog();
  } else {
    content.style.display = 'none';
    arrow.style.transform = 'rotate(0deg)';
  }
}

function loadCrashLog() {
  safeFetch('/crashlog_json', function(data) {
    var tbody = document.getElementById('crashTableBody');
    var badge = document.getElementById('crashBadge');
    var clearBtn = document.getElementById('crashClear');

    // Update badge
    var count = data.total || 0;
    badge.textContent = count;

    // Update badge color
    if (count === 0) {
      badge.className = 'badge';
      clearBtn.style.display = 'none';
    } else if (count <= 5) {
      badge.className = 'badge warning';
      clearBtn.style.display = 'inline-flex';
    } else {
      badge.className = 'badge danger';
      clearBtn.style.display = 'inline-flex';
    }

    // Update table
    if (data.entries && data.entries.length > 0) {
      tbody.innerHTML = '';
      data.entries.forEach(function(entry) {
        var heapClass = entry.heap < 15000 ? ' class="heap-critical"' : '';
        var typeClass = entry.reason === 'PANIC' ? 'crash-panic' : 'crash-wdt';

        var row = '<tr' + heapClass + '>' +
          '<td>' + entry.num + '</td>' +
          '<td class="' + typeClass + '">' + entry.reason + '</td>' +
          '<td>' + formatUptime(entry.uptime) + '</td>' +
          '<td>' + formatBytes(entry.heap) + '</td>' +
          '<td>' + formatBytes(entry.min_heap) + '</td>' +
          '</tr>';
        tbody.innerHTML += row;
      });
    } else {
      tbody.innerHTML = '<tr><td colspan="5" style="text-align: center;">No crashes recorded</td></tr>';
    }
  });
}

function formatUptime(seconds) {
  if (!seconds || seconds === 0) return '0s';
  if (seconds < 60) return seconds + 's';
  if (seconds < 3600) return Math.floor(seconds/60) + 'm';
  return Math.floor(seconds/3600) + 'h ' + Math.floor((seconds%3600)/60) + 'm';
}

function formatBytes(bytes) {
  if (!bytes || bytes === 0) return '0';
  return Math.floor(bytes/1024) + 'KB';
}

function clearCrashHistory() {
  if (confirm('Clear all crash history?\nThis action cannot be undone.')) {
    safeFetch('/clear_crashlog', function() {
      // Update UI
      document.getElementById('crashBadge').textContent = '0';
      document.getElementById('crashBadge').className = 'badge';
      document.getElementById('crashClear').style.display = 'none';

      // Reload table if open
      if (document.getElementById('crashContent').style.display !== 'none') {
        loadCrashLog();
      }
    });
  }
}

// Update crash count on page load
safeFetch('/crashlog_json', function(data) {
  var count = data.total || 0;
  var badge = document.getElementById('crashBadge');
  var clearBtn = document.getElementById('crashClear');

  badge.textContent = count;
  if (count === 0) {
    badge.className = 'badge';
    clearBtn.style.display = 'none';
  } else if (count <= 5) {
    badge.className = 'badge warning';
    clearBtn.style.display = 'inline-flex';
  } else {
    badge.className = 'badge danger';
    clearBtn.style.display = 'inline-flex';
  }
});

function updateStatus() {
  safeFetch('/status', function(data) {
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
  safeFetch('/logs', function(data) {
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