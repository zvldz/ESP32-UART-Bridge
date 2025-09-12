// ESP32 UART Bridge - Device Configuration Module

const DeviceConfig = {
    config: null,
    
    init(config) {
        this.config = config;
        this.render();
        this.attachListeners();
        this.updateVisibility();
    },
    
    render() {
        const container = document.getElementById('deviceConfig');
        if (container) {
            container.innerHTML = this.buildHTML();
            this.setFormValues();
        }
    },
    
    buildHTML() {
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
                            <option value="3">SBUS Input</option>
                            <option value="4">SBUS Output</option>
                        </select>
                    </td>
                </tr>
                <tr>
                    <td><strong>Device 3</strong></td>
                    <td>GPIO 11/12</td>
                    <td>
                        <select name="device3_role" id="device3_role" style="width: 100%;">
                            <option value="0">Disabled</option>
                            <option value="1">UART3 Mirror</option>
                            <option value="2">UART3 Bridge</option>
                            <option value="3">UART3 Logger</option>
                            <option value="4">SBUS Input</option>
                            <option value="5">SBUS Output</option>
                        </select>
                    </td>
                </tr>
                <tr>
                    <td><strong>Device 4</strong></td>
                    <td>Network</td>
                    <td>
                        <select name="device4_role" id="device4_role" style="width: 100%;">
                            <option value="0">Disabled</option>
                            <option value="1">Network Bridge</option>
                            <option value="2">Network Logger</option>
                        </select>
                    </td>
                </tr>
            </table>
            
            <div id="device4Config" style="display: none; margin-top: 15px; padding: 10px; border: 1px solid #ddd; border-radius: 4px;">
                <h4 style="margin-top: 0;">Device 4 Network Configuration</h4>
                <div style="display: flex; gap: 20px; align-items: flex-end; flex-wrap: wrap;">
                    <div>
                        <label for="device4_target_ip">Target IP:</label>
                        <input type="text" name="device4_target_ip" id="device4_target_ip" 
                               style="width: 150px;" pattern="^(?:[0-9]{1,3}\\.){3}[0-9]{1,3}$">
                    </div>
                    <div>
                        <label for="device4_port">Port:</label>
                        <input type="number" name="device4_port" id="device4_port" 
                               style="width: 80px;" min="1" max="65535">
                    </div>
                    <div style="display: flex; align-items: center;">
                        <input type="checkbox" name="udp_batching" id="udp_batching" checked>
                        <label for="udp_batching" style="margin-left: 5px; cursor: pointer;">Batching</label>
                    </div>
                </div>
                <small style="display: block; margin-top: 10px; color: #666;">
                    ℹ️ Use x.x.x.255 for broadcast<br>
                    📦 Disable UDP Batching only if your GCS doesn't support it (reduces performance)
                </small>
            </div>
            
            <h4 style="margin-top: 20px;">Logging Configuration</h4>
            <div style="display: flex; gap: 20px; flex-wrap: wrap;">
                <div>
                    <label>Web Logs:</label>
                    <select name="log_level_web" id="log_level_web" style="width: 120px;">
                        <option value="-1">OFF</option>
                        <option value="0">ERROR</option>
                        <option value="1">WARNING</option>
                        <option value="2">INFO</option>
                        <option value="3">DEBUG</option>
                    </select>
                </div>
                <div>
                    <label>UART Logs:</label>
                    <select name="log_level_uart" id="log_level_uart" style="width: 120px;">
                        <option value="-1">OFF</option>
                        <option value="0">ERROR</option>
                        <option value="1">WARNING</option>
                        <option value="2">INFO</option>
                        <option value="3">DEBUG</option>
                    </select>
                </div>
                <div>
                    <label>Network Logs:</label>
                    <select name="log_level_network" id="log_level_network" disabled style="width: 120px;">
                        <option value="-1">OFF</option>
                        <option value="0">ERROR</option>
                        <option value="1">WARNING</option>
                        <option value="2">INFO</option>
                        <option value="3">DEBUG</option>
                    </select>
                </div>
            </div>
        `;
    },
    
    setFormValues() {
        // Set device roles
        const device2Role = document.getElementById('device2_role');
        const device3Role = document.getElementById('device3_role');
        const device4Role = document.getElementById('device4_role');
        
        if (device2Role) device2Role.value = this.config.device2Role;
        if (device3Role) device3Role.value = this.config.device3Role;
        if (device4Role) {
            device4Role.value = this.config.device4Role;
        }
        
        // Set log levels
        const logLevelWeb = document.getElementById('log_level_web');
        const logLevelUart = document.getElementById('log_level_uart');
        const logLevelNetwork = document.getElementById('log_level_network');
        
        if (logLevelWeb) logLevelWeb.value = this.config.logLevelWeb;
        if (logLevelUart) logLevelUart.value = this.config.logLevelUart;
        if (logLevelNetwork) logLevelNetwork.value = this.config.logLevelNetwork;
        
        // Set Device 4 configuration
        const device4TargetIp = document.getElementById('device4_target_ip');
        const device4Port = document.getElementById('device4_port');
        if (device4TargetIp) device4TargetIp.value = this.config.device4TargetIp || '';
        if (device4Port) device4Port.value = this.config.device4Port || '';
        
        // Set UDP batching checkbox
        const udpBatching = document.getElementById('udp_batching');
        if (udpBatching) {
            udpBatching.checked = this.config.udpBatchingEnabled !== false;  // Default true
        }
        
        // Update device displays
        this.updateDevice2Pins();
        this.updateDevice4Config();
        this.updateNetworkLogLevel();
    },
    
    attachListeners() {
        const device2Role = document.getElementById('device2_role');
        if (device2Role) {
            device2Role.addEventListener('change', () => {
                this.updateDevice2Pins();
                this.updateProtocolFieldState();  // Update protocol field when device2 role changes
            });
        }
        
        const device3Role = document.getElementById('device3_role');
        if (device3Role) {
            device3Role.addEventListener('change', () => {
                this.updateProtocolFieldState();  // Update protocol field when device3 role changes
            });
        }

        const device4Role = document.getElementById('device4_role');
        if (device4Role) {
            device4Role.addEventListener('change', () => this.updateDevice4Config());
        }
    },
    
    updateDevice2Pins() {
        const device2Role = document.getElementById('device2_role');
        const pinsCell = document.getElementById('device2_pins');
        
        if (!device2Role || !pinsCell) return;
        
        const role = device2Role.value;
        
        if (role === '1') { // UART2
            pinsCell.textContent = 'GPIO 8/9';
        } else if (role === '2') { // USB
            pinsCell.textContent = 'USB';
        } else if (role === '3' || role === '4') { // SBUS IN/OUT
            pinsCell.textContent = 'GPIO 8/9 (INV)';
        } else {
            pinsCell.textContent = 'N/A';
        }
    },
    
    updateVisibility() {
        // Update device stats visibility based on roles
        const device2Stats = document.getElementById('device2Stats');
        const device3Stats = document.getElementById('device3Stats');
        
        if (device2Stats) {
            device2Stats.style.display = (this.config.device2Role !== '0') ? 'table-row' : 'none';
        }
        
        if (device3Stats) {
            device3Stats.style.display = (this.config.device3Role !== '0') ? 'table-row' : 'none';
        }
        
        // Update protocol field state based on SBUS roles
        this.updateProtocolFieldState();
    },
    
    updateProtocolFieldState() {
        // Check if any device has SBUS role active
        const isSbusActive = (this.config.device2Role === '3' || this.config.device2Role === '4' ||
                             this.config.device3Role === '4' || this.config.device3Role === '5');
        
        // Protocol optimization field should be blocked when SBUS is active
        const protocolField = document.getElementById('protocol_optimization');
        if (protocolField) {
            protocolField.disabled = isSbusActive;
            // Add visual indication
            protocolField.style.opacity = isSbusActive ? '0.5' : '1';
            protocolField.style.cursor = isSbusActive ? 'not-allowed' : 'default';
            
            // Set to SBUS protocol when SBUS is active
            if (isSbusActive && protocolField.value !== '2') {
                // Add SBUS option if it doesn't exist
                let sbusOption = protocolField.querySelector('option[value="2"]');
                if (!sbusOption) {
                    sbusOption = document.createElement('option');
                    sbusOption.value = '2';
                    sbusOption.textContent = 'SBUS';
                    protocolField.appendChild(sbusOption);
                }
                protocolField.value = '2';  // Set to SBUS protocol
            }
        }
        
        // MAVLink routing checkbox should also be blocked when SBUS is active
        const mavlinkRouting = document.getElementById('mavlink-routing');
        if (mavlinkRouting) {
            mavlinkRouting.disabled = isSbusActive;
            // Add visual indication to the checkbox and its label
            mavlinkRouting.style.opacity = isSbusActive ? '0.5' : '1';
            mavlinkRouting.style.cursor = isSbusActive ? 'not-allowed' : 'default';
            
            // Also disable the parent label
            const label = mavlinkRouting.closest('label');
            if (label) {
                label.style.opacity = isSbusActive ? '0.5' : '1';
                label.style.cursor = isSbusActive ? 'not-allowed' : 'default';
            }
        }
        
        // Show/hide warning message
        this.updateSbusWarning(isSbusActive);
    },
    
    updateSbusWarning(isSbusActive) {
        const existingWarning = document.getElementById('sbus-uart-warning');
        
        if (isSbusActive && !existingWarning) {
            // Create warning message
            const warning = document.createElement('div');
            warning.id = 'sbus-uart-warning';
            warning.style.cssText = `
                background: #fff3cd; 
                color: #856404; 
                padding: 10px; 
                border: 1px solid #ffeeba; 
                border-radius: 4px; 
                margin: 10px 0;
                font-size: 13px;
            `;
            warning.innerHTML = '⚠️ Protocol optimization is automatically set to SBUS when SBUS device roles are active';
            
            // Insert after Protocol Optimization section
            const protocolSection = document.getElementById('protocol_optimization')?.parentElement;
            if (protocolSection) {
                protocolSection.appendChild(warning);
            }
        } else if (!isSbusActive && existingWarning) {
            // Remove warning message
            existingWarning.remove();
        }
    },
    
    // Get role name for display
    getRoleName(device, role) {
        const roleNames = {
            device2: {
                '0': 'Disabled',
                '1': 'UART2',
                '2': 'USB',
                '3': 'SBUS In',
                '4': 'SBUS Out'
            },
            device3: {
                '0': 'Disabled',
                '1': 'Mirror',
                '2': 'Bridge',
                '3': 'Logger',
                '4': 'SBUS In',
                '5': 'SBUS Out'
            }
        };
        
        return roleNames[device]?.[role] || 'Unknown';
    },

    updateDevice4Config() {
        const device4Role = document.getElementById('device4_role');
        const device4Config = document.getElementById('device4Config');
        const targetIpInput = document.getElementById('device4_target_ip');
        const portInput = document.getElementById('device4_port');
        const networkLogLevel = document.getElementById('log_level_network');
        
        if (!device4Role || !device4Config) return;
        
        const role = device4Role.value;
        
        if (role !== '0') {
            device4Config.style.display = 'block';
            
            // Enable Network Log Level when Device 4 is active
            if (networkLogLevel) {
                networkLogLevel.disabled = false;
            }
            
            // Always update port based on role
            if (role === '1') {  // Network Bridge
                portInput.value = '14550';
            } else if (role === '2') {  // Network Logger
                portInput.value = '14560';
            }
            
            // Set IP defaults only if field is empty
            if (!targetIpInput.value || targetIpInput.value === '') {
                // Try to get client IP first, fallback to broadcast
                fetch('/client-ip')
                    .then(response => response.text())
                    .then(clientIP => {
                        targetIpInput.value = clientIP;
                    })
                    .catch(() => {
                        targetIpInput.value = '192.168.4.255'; // Fallback to broadcast
                    });
            }
        } else {
            device4Config.style.display = 'none';
            
            // Disable Network Log Level when Device 4 is disabled
            if (networkLogLevel) {
                networkLogLevel.disabled = true;
                networkLogLevel.value = '-1'; // Set to OFF
            }
        }
    },

    updateNetworkLogLevel() {
        const device4Role = document.getElementById('device4_role');
        const networkLogLevel = document.getElementById('log_level_network');
        
        if (!device4Role || !networkLogLevel) return;
        
        // Enable/disable Network Log Level based on Device 4 role
        if (device4Role.value !== '0') {
            networkLogLevel.disabled = false;
        } else {
            networkLogLevel.disabled = true;
        }
    }
};