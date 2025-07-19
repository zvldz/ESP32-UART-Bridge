// ESP32 UART Bridge - Main JavaScript

// Parse configuration from embedded JSON
const config = JSON.parse(document.getElementById('config').textContent);

// Safe fetch with error handling
function safeFetch(url, callback, errorCallback) {
    fetch(url)
        .then(r => {
            if (!r.ok) throw new Error('Network response was not ok');
            return r.json();
        })
        .then(callback)
        .catch(err => {
            console.error('Fetch error:', err);
            if (errorCallback) errorCallback(err);
        });
}

// Initialize UI with config data
function initializeUI() {
    // Build system info table
    document.getElementById('systemInfo').innerHTML = buildSystemInfo(config);
    
    // Build device stats table
    document.getElementById('deviceStats').innerHTML = buildDeviceStats(config);
    
    // Build device configuration table
    document.getElementById('deviceConfig').innerHTML = buildDeviceConfig();
    
    // Populate form selects
    populateFormSelects();
    
    // Set form values from config
    setFormValues();
    
    // Update device visibility
    updateDeviceVisibility();
    
    // Update Device 2 pins display
    updateDevice2Pins();
    
    // Setup event listener for device2 role
    document.getElementById('device2_role').addEventListener('change', updateDevice2Pins);
    
    // Update crash count on page load
    updateCrashCount();
    
    // Start dynamic updates
    setInterval(updateStatus, 5000);
    setInterval(updateLogs, 5000);
    
    // Load logs immediately
    updateLogs();
}

// Build system info HTML
function buildSystemInfo(cfg) {
    return `
        <table>
            <tr><td><strong>Device:</strong></td><td>${cfg.deviceName} v${cfg.version}</td></tr>
            <tr><td><strong>Free RAM:</strong></td><td id="freeRam">${cfg.freeRam} bytes</td></tr>
            <tr><td><strong>Uptime:</strong></td><td id="uptime">${cfg.uptime} seconds</td></tr>
            <tr><td><strong>WiFi SSID:</strong></td><td>${cfg.ssid}</td></tr>
            <tr><td><strong>Current UART:</strong></td><td>${cfg.uartConfig}</td></tr>
            <tr><td><strong>Flow Control:</strong></td><td>${cfg.flowControl}</td></tr>
        </table>
    `;
}

// Build device statistics HTML
function buildDeviceStats(cfg) {
    return `
        <table>
            <tr><td colspan="2" style="text-align: center; font-weight: bold; background-color: #f0f0f0;">Device Statistics</td></tr>
            <tr id="device1Stats">
                <td><strong>Device 1 (UART1):</strong></td>
                <td>RX: <span id="device1Rx">${cfg.device1Rx}</span> bytes, TX: <span id="device1Tx">${cfg.device1Tx}</span> bytes</td>
            </tr>
            <tr id="device2Stats" style="display: none;">
                <td><strong>Device 2 (<span id="device2Role">${cfg.device2RoleName}</span>):</strong></td>
                <td>RX: <span id="device2Rx">${cfg.device2Rx}</span> bytes, TX: <span id="device2Tx">${cfg.device2Tx}</span> bytes</td>
            </tr>
            <tr id="device3Stats" style="display: none;">
                <td><strong>Device 3 (<span id="device3Role">${cfg.device3RoleName}</span>):</strong></td>
                <td>RX: <span id="device3Rx">${cfg.device3Rx}</span> bytes, TX: <span id="device3Tx">${cfg.device3Tx}</span> bytes</td>
            </tr>
            <tr><td colspan="2" style="border-top: 1px solid #ddd; padding-top: 5px;"></td></tr>
            <tr><td><strong>Total Traffic:</strong></td><td id="totalTraffic">${cfg.totalTraffic} bytes</td></tr>
            <tr><td><strong>Last Activity:</strong></td><td id="lastActivity">${cfg.lastActivity}</td></tr>
        </table>
        <div style="text-align: center; margin: 15px 0;">
            <button style="background-color: #ff9800;" onclick="if(confirm('Reset all statistics and logs?')) location.href='/reset_stats'">Reset Statistics</button>
        </div>
    `;
}

// Build device configuration HTML
function buildDeviceConfig() {
    return `
        <h3>Device Configuration</h3>
        <table style="width: 100%;">
            <tr>
                <th>Device</th>
                <th>Pins</th>
                <th>Role</th>
            </tr>
            <tr>
                <td><strong>Device 1</strong></td>
                <td>GPIO 4/5</td>
                <td>
                    <select name="device1_role" disabled style="width: 100%;">
                        <option value="0">UART1</option>
                    </select>
                </td>
            </tr>
            <tr>
                <td><strong>Device 2</strong></td>
                <td id="device2_pins">Variable</td>
                <td>
                    <select name="device2_role" id="device2_role" style="width: 100%;">
                        <option value="0">Disabled</option>
                        <option value="1">UART2</option>
                        <option value="2">USB</option>
                    </select>
                </td>
            </tr>
            <tr>
                <td><strong>Device 3</strong></td>
                <td>GPIO 11/12</td>
                <td>
                    <select name="device3_role" style="width: 100%;">
                        <option value="0">Disabled</option>
                        <option value="1">UART3 Mirror</option>
                        <option value="2">UART3 Bridge</option>
                        <option value="3">UART3 Logger</option>
                    </select>
                </td>
            </tr>
            <tr>
                <td><strong>Device 4</strong></td>
                <td>Network</td>
                <td>
                    <select name="device4_role" disabled style="width: 100%;">
                        <option value="0">Disabled</option>
                        <option value="1">Network Bridge (Coming Soon)</option>
                        <option value="2">Network Logger (Coming Soon)</option>
                    </select>
                </td>
            </tr>
        </table>
        
        <h4 style="margin-top: 20px;">Logging Configuration</h4>
        <div style="display: flex; gap: 20px; flex-wrap: wrap;">
            <div>
                <label>Web Logs:</label>
                <select name="log_level_web" style="width: 100px;">
                    <option value="-1">OFF</option>
                    <option value="0">ERROR</option>
                    <option value="1">WARNING</option>
                    <option value="2">INFO</option>
                    <option value="3">DEBUG</option>
                </select>
            </div>
            <div>
                <label>UART Logs:</label>
                <select name="log_level_uart" style="width: 100px;">
                    <option value="-1">OFF</option>
                    <option value="0">ERROR</option>
                    <option value="1">WARNING</option>
                    <option value="2">INFO</option>
                    <option value="3">DEBUG</option>
                </select>
            </div>
            <div>
                <label>Network Logs:</label>
                <select name="log_level_network" disabled style="width: 100px;">
                    <option value="-1">OFF</option>
                    <option value="0">ERROR</option>
                    <option value="1">WARNING</option>
                    <option value="2">INFO</option>
                    <option value="3">DEBUG</option>
                </select>
            </div>
        </div>
    `;
}

// Populate form select options
function populateFormSelects() {
    // Baud rates
    const baudRates = [4800, 9600, 19200, 38400, 57600, 74880, 115200, 230400, 250000, 460800, 500000, 921600, 1000000];
    const baudSelect = document.getElementById('baudrate');
    baudRates.forEach(rate => {
        const option = document.createElement('option');
        option.value = rate;
        option.textContent = rate;
        baudSelect.appendChild(option);
    });
    
    // Data bits
    const dataBitsSelect = document.getElementById('databits');
    [7, 8].forEach(bits => {
        const option = document.createElement('option');
        option.value = bits;
        option.textContent = bits;
        dataBitsSelect.appendChild(option);
    });
    
    // Parity
    const paritySelect = document.getElementById('parity');
    const parityOptions = {none: 'None', even: 'Even', odd: 'Odd'};
    Object.entries(parityOptions).forEach(([value, text]) => {
        const option = document.createElement('option');
        option.value = value;
        option.textContent = text;
        paritySelect.appendChild(option);
    });
    
    // Stop bits
    const stopBitsSelect = document.getElementById('stopbits');
    [1, 2].forEach(bits => {
        const option = document.createElement('option');
        option.value = bits;
        option.textContent = bits;
        stopBitsSelect.appendChild(option);
    });
}

// Set form values from config
function setFormValues() {
    document.getElementById('baudrate').value = config.baudrate;
    document.getElementById('databits').value = config.databits;
    document.getElementById('parity').value = config.parity;
    document.getElementById('stopbits').value = config.stopbits;
    document.getElementById('flowcontrol').checked = config.flowcontrol;
    document.getElementById('usbmode').value = config.usbMode;
    
    // Device roles
    document.getElementById('device2_role').value = config.device2Role;
    document.querySelector('select[name="device3_role"]').value = config.device3Role;
    document.querySelector('select[name="device4_role"]').value = config.device4Role;
    
    // Log levels
    document.querySelector('select[name="log_level_web"]').value = config.logLevelWeb;
    document.querySelector('select[name="log_level_uart"]').value = config.logLevelUart;
    document.querySelector('select[name="log_level_network"]').value = config.logLevelNetwork;
    
    // WiFi settings
    document.getElementById('ssid').value = config.ssid;
    document.getElementById('password').value = config.password;
    
    // Log count
    document.getElementById('logCount').textContent = config.logDisplayCount;
}

// Update device visibility based on roles
function updateDeviceVisibility() {
    if (config.device2Role !== '0') {
        document.getElementById('device2Stats').style.display = 'table-row';
    }
    if (config.device3Role !== '0') {
        document.getElementById('device3Stats').style.display = 'table-row';
    }
}

// Update Device 2 pins display
function updateDevice2Pins() {
    const device2Role = document.getElementById('device2_role').value;
    const pinsCell = document.getElementById('device2_pins');
    
    if (device2Role === '1') { // UART2
        pinsCell.textContent = 'GPIO 8/9';
    } else if (device2Role === '2') { // USB
        pinsCell.textContent = 'USB';
    } else {
        pinsCell.textContent = 'N/A';
    }
}

// Toggle WiFi configuration
function toggleWifiConfig() {
    const content = document.getElementById('wifiConfigContent');
    const arrow = document.getElementById('wifiArrow');
    
    if (content.style.display === 'none') {
        content.style.display = 'block';
        arrow.textContent = '▼';
    } else {
        content.style.display = 'none';
        arrow.textContent = '▶';
    }
}

// Toggle firmware update
function toggleFirmwareUpdate() {
    const content = document.getElementById('firmwareUpdateContent');
    const arrow = document.getElementById('firmwareArrow');
    
    if (content.style.display === 'none') {
        content.style.display = 'block';
        arrow.textContent = '▼';
    } else {
        content.style.display = 'none';
        arrow.textContent = '▶';
    }
}

// Toggle password visibility
function togglePassword() {
    const passwordInput = document.getElementById("password");
    const icon = document.getElementById("toggleIcon");
    
    if (passwordInput.type === "password") {
        passwordInput.type = "text";
        // Eye with slash (hidden)
        icon.innerHTML = '<svg width="20" height="20" viewBox="0 0 20 20" fill="none" stroke="#666" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M13.875 13.875A8.5 8.5 0 016.125 6.125m-3.27.73A9.98 9.98 0 001 10s3 6 9 6a9.98 9.98 0 003.145-.855M8.29 4.21A4.95 4.95 0 0110 4c6 0 9 6 9 6a17.46 17.46 0 01-2.145 2.855M1 1l18 18"></path></svg>';
    } else {
        passwordInput.type = "password";
        // Open eye
        icon.innerHTML = '<svg width="20" height="20" viewBox="0 0 20 20" fill="none" stroke="#666" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"><path d="M1 10s3-6 9-6 9 6 9 6-3 6-9 6-9-6-9-6z"></path><circle cx="10" cy="10" r="3"></circle></svg>';
    }
}

// Copy logs to clipboard
function copyLogs() {
    const logEntriesElement = document.getElementById('logEntries');
    if (!logEntriesElement) return;
    
    const logs = logEntriesElement.innerText || logEntriesElement.textContent || '';
    if (!logs || logs === 'Loading logs...') return;
    
    const textArea = document.createElement("textarea");
    textArea.value = logs;
    textArea.style.position = "fixed";
    textArea.style.left = "-999999px";
    document.body.appendChild(textArea);
    textArea.select();
    
    try {
        document.execCommand('copy');
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

// Dynamic status update
function updateStatus() {
    safeFetch('/status', function(data) {
        document.getElementById('freeRam').textContent = data.freeRam + ' bytes';
        document.getElementById('uptime').textContent = data.uptime + ' seconds';
        
        // Update Device 1 stats (always visible)
        document.getElementById('device1Rx').textContent = data.device1Rx + '';
        document.getElementById('device1Tx').textContent = data.device1Tx + '';
        
        // Update Device 2 stats if present
        if (data.device2Role) {
            document.getElementById('device2Stats').style.display = 'table-row';
            document.getElementById('device2Role').textContent = data.device2Role;
            document.getElementById('device2Rx').textContent = data.device2Rx + '';
            document.getElementById('device2Tx').textContent = data.device2Tx + '';
        } else {
            document.getElementById('device2Stats').style.display = 'none';
        }
        
        // Update Device 3 stats if present
        if (data.device3Role) {
            document.getElementById('device3Stats').style.display = 'table-row';
            document.getElementById('device3Role').textContent = data.device3Role;
            document.getElementById('device3Rx').textContent = data.device3Rx + '';
            document.getElementById('device3Tx').textContent = data.device3Tx + '';
        } else {
            document.getElementById('device3Stats').style.display = 'none';
        }
        
        // Update totals
        document.getElementById('totalTraffic').textContent = data.totalTraffic + ' bytes';
        
        // Handle both "Never" string and numeric seconds
        if (data.lastActivity === "Never") {
            document.getElementById('lastActivity').textContent = "Never";
        } else {
            document.getElementById('lastActivity').textContent = data.lastActivity + ' seconds ago';
        }
    });
}

// Update logs
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

// Initialize on page load
document.addEventListener('DOMContentLoaded', initializeUI);