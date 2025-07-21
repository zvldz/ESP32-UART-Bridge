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
                        </select>
                    </td>
                </tr>
                <tr>
                    <td><strong>Device 4</strong></td>
                    <td>Network</td>
                    <td>
                        <select name="device4_role" id="device4_role" disabled style="width: 100%;">
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
                    <select name="log_level_web" id="log_level_web" style="width: 100px;">
                        <option value="-1">OFF</option>
                        <option value="0">ERROR</option>
                        <option value="1">WARNING</option>
                        <option value="2">INFO</option>
                        <option value="3">DEBUG</option>
                    </select>
                </div>
                <div>
                    <label>UART Logs:</label>
                    <select name="log_level_uart" id="log_level_uart" style="width: 100px;">
                        <option value="-1">OFF</option>
                        <option value="0">ERROR</option>
                        <option value="1">WARNING</option>
                        <option value="2">INFO</option>
                        <option value="3">DEBUG</option>
                    </select>
                </div>
                <div>
                    <label>Network Logs:</label>
                    <select name="log_level_network" id="log_level_network" disabled style="width: 100px;">
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
        if (device4Role) device4Role.value = this.config.device4Role;
        
        // Set log levels
        const logLevelWeb = document.getElementById('log_level_web');
        const logLevelUart = document.getElementById('log_level_uart');
        const logLevelNetwork = document.getElementById('log_level_network');
        
        if (logLevelWeb) logLevelWeb.value = this.config.logLevelWeb;
        if (logLevelUart) logLevelUart.value = this.config.logLevelUart;
        if (logLevelNetwork) logLevelNetwork.value = this.config.logLevelNetwork;
        
        // Update device 2 pins display
        this.updateDevice2Pins();
    },
    
    attachListeners() {
        const device2Role = document.getElementById('device2_role');
        if (device2Role) {
            device2Role.addEventListener('change', () => this.updateDevice2Pins());
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
    },
    
    // Get role name for display
    getRoleName(device, role) {
        const roleNames = {
            device2: {
                '0': 'Disabled',
                '1': 'UART2',
                '2': 'USB'
            },
            device3: {
                '0': 'Disabled',
                '1': 'Mirror',
                '2': 'Bridge',
                '3': 'Logger'
            }
        };
        
        return roleNames[device]?.[role] || 'Unknown';
    }
};