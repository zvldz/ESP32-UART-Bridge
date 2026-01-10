// ESP32 UART Bridge - Device Configuration Module

const DeviceConfig = {
    config: null,

    // XIAO ESP32-S3 GPIO to D-pin mapping
    // Based on official pinout from Seeed Studio wiki
    // All pins available on castellated edge holes
    xiaoGpioToDPin: {
        1: 'D0',   // RTS (flow control)
        2: 'D1',   // CTS (flow control)
        3: 'D2',   // Available GPIO
        4: 'D3',   // Device 1 RX (UART Bridge)
        5: 'D4',   // Device 1 TX (UART Bridge)
        6: 'D5',   // Available GPIO
        7: 'D6',   // Available GPIO
        8: 'D8',   // Device 2 RX (UART2)
        9: 'D9',   // Device 2 TX (UART2)
        10: 'D10', // Available GPIO
        43: 'D6',  // Device 3 TX (GPIO43 = D6 pin on board)
        44: 'D7'   // Device 3 RX (GPIO44 = D7 pin on board)
    },

    init(config) {
        this.config = config;
        this.render();
        this.attachListeners();
        this.updateVisibility();
    },

    // Format GPIO pin text with D-pin name for XIAO
    formatGpioPin(gpioNum) {
        const boardType = this.config.boardType || 's3zero';
        const isXiao = (boardType === 'xiao');

        if (isXiao && this.xiaoGpioToDPin[gpioNum]) {
            return `GPIO ${gpioNum} (${this.xiaoGpioToDPin[gpioNum]})`;
        }
        return `GPIO ${gpioNum}`;
    },

    // Format GPIO pin pair text with D-pin names for XIAO
    formatGpioPinPair(gpio1, gpio2) {
        const boardType = this.config.boardType || 's3zero';
        const isXiao = (boardType === 'xiao');

        if (isXiao && this.xiaoGpioToDPin[gpio1] && this.xiaoGpioToDPin[gpio2]) {
            return `GPIO ${gpio1}/${gpio2} (${this.xiaoGpioToDPin[gpio1]}/${this.xiaoGpioToDPin[gpio2]})`;
        }
        return `GPIO ${gpio1}/${gpio2}`;
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
            <div id="device-warnings" style="margin-bottom: 15px;"></div>
            <table class="device-table">
                <tr>
                    <th>Device</th>
                    <th>Pins</th>
                    <th>Role</th>
                    <th>Options</th>
                </tr>
                <tr>
                    <td><strong>Device 1</strong></td>
                    <td id="device1_pins">GPIO 4/5</td>
                    <td>
                        <select name="device1_role" id="device1_role" onchange="DeviceConfig.onDevice1RoleChange()" style="width: 100%;">
                            <option value="0">UART Bridge</option>
                            <option value="1">SBUS Input</option>
                        </select>
                    </td>
                    <td></td>
                </tr>
                <tr>
                    <td><strong>Device 2</strong></td>
                    <td id="device2_pins">Variable</td>
                    <td>
                        <select name="device2_role" id="device2_role" onchange="DeviceConfig.onDeviceRoleChange()" style="width: 100%;">
                            <option value="0">Disabled</option>
                            <option value="1">UART2</option>
                            <option value="2">USB</option>
                            <option value="3">SBUS Input</option>
                            <option value="4">SBUS Output</option>
                            <option value="5">USB SBUS Text Output</option>
                        </select>
                    </td>
                    <td id="device2_options">
                        <select id="device2_sbus_format" name="device2_sbus_format" style="display:none; width: 100%;">
                            <option value="0">Binary (SBUS)</option>
                            <option value="1">Text (RC)</option>
                            <option value="2">MAVLink (RC Override)</option>
                        </select>
                        <select id="device2_sbus_rate" name="device2_sbus_rate" style="display:none; width: 100px;">
                            <option value="10">10 Hz</option>
                            <option value="20">20 Hz</option>
                            <option value="30">30 Hz</option>
                            <option value="40">40 Hz</option>
                            <option value="50" selected>50 Hz</option>
                            <option value="60">60 Hz</option>
                            <option value="70">70 Hz</option>
                        </select>
                    </td>
                </tr>
                <tr>
                    <td><strong>Device 3</strong></td>
                    <td id="device3_pins">N/A</td>
                    <td>
                        <select name="device3_role" id="device3_role" onchange="DeviceConfig.onDeviceRoleChange()" style="width: 100%;">
                            <option value="0">Disabled</option>
                            <option value="1">UART3 Mirror</option>
                            <option value="2">UART3 Bridge</option>
                            <option value="3">UART3 Logger</option>
                            <option value="4">SBUS Input</option>
                            <option value="5_0">SBUS Output</option>
                            <option value="5_1">SBUS Text Output</option>
                        </select>
                    </td>
                    <td id="device3_options">
                        <select id="device3_sbus_rate" name="device3_sbus_rate" style="display:none; width: 100px;">
                            <option value="10">10 Hz</option>
                            <option value="20">20 Hz</option>
                            <option value="30">30 Hz</option>
                            <option value="40">40 Hz</option>
                            <option value="50" selected>50 Hz</option>
                            <option value="60">60 Hz</option>
                            <option value="70">70 Hz</option>
                        </select>
                    </td>
                </tr>
                <tr>
                    <td><strong>Device 4</strong></td>
                    <td id="device4_pins">Network</td>
                    <td>
                        <select name="device4_role" id="device4_role" onchange="DeviceConfig.onDeviceRoleChange()" style="width: 100%;">
                            <option value="0">Disabled</option>
                            <option value="1">Network Bridge</option>
                            <option value="2">Network Logger</option>
                            <option value="3_0">SBUS Output</option>
                            <option value="3_1">SBUS Text Output</option>
                            <option value="4">SBUS Input</option>
                        </select>
                    </td>
                    <td id="device4_options">
                        <select id="device4_sbus_rate" name="device4_sbus_rate" style="display:none; width: 100px;">
                            <option value="10">10 Hz</option>
                            <option value="20">20 Hz</option>
                            <option value="30">30 Hz</option>
                            <option value="40">40 Hz</option>
                            <option value="50" selected>50 Hz</option>
                            <option value="60">60 Hz</option>
                            <option value="70">70 Hz</option>
                        </select>
                    </td>
                </tr>
                <tr id="device5_row" style="display:none;">
                    <td><strong>Device 5</strong></td>
                    <td id="device5_pins">Bluetooth</td>
                    <td>
                        <select name="device5_role" id="device5_role" onchange="DeviceConfig.onDevice5RoleChange()" style="width: 100%;">
                            <option value="0">Disabled</option>
                            <option value="1">Bridge</option>
                            <option value="2">SBUS Text Output</option>
                        </select>
                    </td>
                    <td id="device5_options">
                        <select id="bt_send_rate" name="bt_send_rate" style="display: none; width: 100px;">
                            <option value="10">10 Hz</option>
                            <option value="20">20 Hz</option>
                            <option value="30">30 Hz</option>
                            <option value="40">40 Hz</option>
                            <option value="50">50 Hz</option>
                            <option value="60">60 Hz</option>
                            <option value="70">70 Hz</option>
                        </select>
                    </td>
                </tr>
            </table>
            <div id="sbus-config-warning" style="display:none; background:#fff3cd; color:#856404; padding:10px; border:1px solid #ffeeba; border-radius:4px; margin-top:10px; font-size:13px;">
                ⚠️ SBUS Output configured but no SBUS Input source detected
            </div>
        `;
    },
    
    setFormValues() {
        // Set device roles
        const device1Role = document.getElementById('device1_role');
        const device2Role = document.getElementById('device2_role');
        const device3Role = document.getElementById('device3_role');
        const device4Role = document.getElementById('device4_role');

        if (device1Role) {
            device1Role.value = this.config.device1Role || '0';
            this.onDevice1RoleChange();
        }
        if (device2Role) {
            // MiniKit: UART2 not available (GPIO8/9 used by SPI flash)
            // Available roles: Disabled (0), USB (2), USB SBUS Text (5)
            if (this.config.uart2Available === false) {
                // Disable UART2-based options only, keep Disabled and USB options enabled
                ['1', '3', '4'].forEach(val => {
                    const opt = device2Role.querySelector(`option[value="${val}"]`);
                    if (opt && !opt.text.includes('(not available)')) {
                        opt.disabled = true;
                        opt.text += ' (not available)';
                    }
                });
            }
            device2Role.value = this.config.device2Role;
            // Set Device 2 SBUS rate
            const device2SbusRate = document.getElementById('device2_sbus_rate');
            if (device2SbusRate) {
                device2SbusRate.value = this.config.device2SbusRate || 50;
            }
            this.updateDevice2Config();
        }
        if (device3Role) {
            // Convert role + format to composite value for SBUS Output
            const d3Role = this.config.device3Role;
            const d3Format = this.config.device3SbusFormat || '0';
            if (d3Role === '5') {
                device3Role.value = d3Format === '1' ? '5_1' : '5_0';
            } else {
                device3Role.value = d3Role;
            }
            // Set Device 3 SBUS rate
            const device3SbusRate = document.getElementById('device3_sbus_rate');
            if (device3SbusRate) {
                device3SbusRate.value = this.config.device3SbusRate || 50;
            }
            this.updateDevice3Pins();
            this.updateDevice3Config();
        }
        if (device4Role) {
            // Update Device4 options based on SBUS context first
            this.updateDevice4Options();
            // Convert role + format to composite value for SBUS Output
            const d4Role = this.config.device4Role;
            const d4Format = this.config.device4SbusFormat || '0';
            if (d4Role === '3') {
                device4Role.value = d4Format === '1' ? '3_1' : '3_0';
            } else {
                device4Role.value = d4Role;
            }
            // Set Device 4 SBUS rate in options
            const device4SbusRate = document.getElementById('device4_sbus_rate');
            if (device4SbusRate) {
                device4SbusRate.value = this.config.device4SendRate || 50;
            }
        }

        // Device 5 (Bluetooth SPP - MiniKit only)
        const device5Row = document.getElementById('device5_row');
        const device5Role = document.getElementById('device5_role');
        const btSendRate = document.getElementById('bt_send_rate');
        if (this.config.boardType === 'minikit' && device5Row && device5Role) {
            device5Row.style.display = '';  // Show row for MiniKit
            device5Role.value = this.config.device5Role || '0';
            if (btSendRate) {
                btSendRate.value = this.config.btSendRate || 50;
            }

            // Show mDNS hint about BT (name uses mDNS hostname)
            const mdnsBtHint = document.getElementById('mdns_bt_hint');
            if (mdnsBtHint) mdnsBtHint.style.display = '';

            // Show/hide BT config based on role
            this.updateDevice5Config();
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
        if (device4Port) {
            // Always use saved port from config
            device4Port.value = this.config.device4Port || '';
        }
        
        // Set UDP batching checkbox
        const udpBatching = document.getElementById('udp_batching');
        if (udpBatching) {
            udpBatching.checked = this.config.udpBatchingEnabled !== false;  // Default true
        }

        // Set auto broadcast checkbox and attach handler
        const autoBroadcast = document.getElementById('device4_auto_broadcast');
        if (autoBroadcast) {
            autoBroadcast.checked = this.config.device4AutoBroadcast === true;
            autoBroadcast.addEventListener('change', () => this.updateAutoBroadcastState());
        }

        // Set UDP source timeout
        const udpTimeout = document.getElementById('device4_udp_timeout');
        if (udpTimeout) {
            udpTimeout.value = this.config.device4UdpTimeout || 1000;
        }

        // Set UDP send rate
        const sendRate = document.getElementById('device4_send_rate');
        if (sendRate) {
            sendRate.value = this.config.device4SendRate || 50;
        }

        // Update device displays
        this.updateDevice1Pins();
        this.updateDevice2Pins();
        this.updateUsbModeVisibility(device2Role ? device2Role.value : '0');
        try {
            this.updateDevice3Pins();
            this.updateDevice4Pins();
        } catch(e) {
            console.log('Error updating device pins:', e);
        }
        this.updateDevice4Config();
        this.updateNetworkLogLevel();

        // Update auto broadcast visibility after device4_role is set
        this.updateAutoBroadcastState();

        // Update rate selectors visibility and SBUS warnings
        this.updateDevice3Config();
        this.updateDevice4SbusRate();
        this.updateSbusFormatOptions();
        this.updateDevice5BridgeState();
    },

    attachListeners() {
        const device2Role = document.getElementById('device2_role');
        if (device2Role) {
            device2Role.addEventListener('change', () => {
                this.config.device2Role = device2Role.value;  // Update config
                this.updateDevice2Pins();
                this.updateUartConfigVisibility();  // Update UART config visibility
                this.updateProtocolFieldState();  // Update protocol field when device2 role changes
                if (FormUtils.updateSbusTimingKeeperVisibility) {
                    FormUtils.updateSbusTimingKeeperVisibility.call(FormUtils);
                }
            });
        }
        
        const device3Role = document.getElementById('device3_role');
        if (device3Role) {
            device3Role.addEventListener('change', () => {
                this.config.device3Role = device3Role.value;  // Update config
                try {
                    this.updateDevice3Pins();
                } catch(e) {
                    console.log('Error in device3 pins update:', e);
                }
                this.updateDevice4Options();  // Update Device4 options based on context
                this.updateProtocolFieldState();  // Update protocol field when device3 role changes
                this.updateUartConfigVisibility();  // Update UART config visibility
                if (FormUtils.updateSbusTimingKeeperVisibility) {
                    FormUtils.updateSbusTimingKeeperVisibility.call(FormUtils);
                }
            });
        }

        const device4Role = document.getElementById('device4_role');
        if (device4Role) {
            device4Role.addEventListener('change', () => {
                try {
                    this.updateDevice4Pins();
                } catch(e) {
                    console.log('Error in device4 pins update:', e);
                }
                this.updateDevice4Config(true);
            });
        }

        // Listen for WiFi mode changes to update auto broadcast visibility and AP default IP
        const wifiMode = document.getElementById('wifi_mode');
        if (wifiMode) {
            wifiMode.addEventListener('change', () => {
                // When switching to AP mode, set default broadcast IP
                if (wifiMode.value === '0') {
                    const targetIpInput = document.getElementById('device4_target_ip');
                    const device4Role = document.getElementById('device4_role');
                    const isTxRole = device4Role && (device4Role.value === '1' || device4Role.value === '3');
                    if (targetIpInput && isTxRole) {
                        targetIpInput.value = '192.168.4.255';
                    }
                }
                this.updateAutoBroadcastState();
            });
        }
    },
    
    updateDevice2Pins() {
        const device2Role = document.getElementById('device2_role');
        const pinsCell = document.getElementById('device2_pins');

        if (!device2Role || !pinsCell) return;

        const role = device2Role.value;

        if (role === '1') { // UART2
            pinsCell.textContent = this.formatGpioPinPair(8, 9);
        } else if (role === '2' || role === '5') { // USB or USB SBUS Text
            pinsCell.textContent = 'USB';
        } else if (role === '3') { // SBUS IN
            pinsCell.textContent = this.formatGpioPin(8) + ' (RX)';
        } else if (role === '4') { // SBUS OUT
            pinsCell.textContent = this.formatGpioPin(9) + ' (TX)';
        } else {
            pinsCell.textContent = 'N/A';
        }

        // Show/hide USB Mode section based on Device2 role
        this.updateUsbModeVisibility(role);
        this.updateUartConfigVisibility();
    },

    updateUsbModeVisibility(device2Role) {
        // Find USB Mode block in Advanced Configuration
        const usbModeBlock = document.getElementById('usbModeBlock');

        if (usbModeBlock) {
            // Show only if Device2 is in USB mode
            if (device2Role === '2') { // USB
                usbModeBlock.style.display = 'block';
            } else {
                usbModeBlock.style.display = 'none';
            }
        }
    },

    updateDevice3Pins() {
        const device3Role = document.getElementById('device3_role');
        const pinsCell = document.getElementById('device3_pins');

        if (!device3Role || !pinsCell) return;

        const role = device3Role.value;
        const boardType = this.config.boardType || 's3zero';
        const isXiao = (boardType === 'xiao');
        const isMiniKit = (boardType === 'minikit');

        if (role === '1' || role === '2') { // UART3 Mirror/Bridge
            if (isMiniKit) {
                pinsCell.textContent = 'GPIO 16/17';
            } else {
                pinsCell.textContent = isXiao ? this.formatGpioPinPair(43, 44) : 'GPIO 11/12';
            }
        } else if (role === '3') { // UART3 Logger
            if (isMiniKit) {
                pinsCell.textContent = 'GPIO 17 (TX only)';
            } else {
                pinsCell.textContent = isXiao ? this.formatGpioPin(43) + ' (TX only)' : 'GPIO 12 (TX only)';
            }
        } else if (role === '4') { // SBUS IN
            if (isMiniKit) {
                pinsCell.textContent = 'GPIO 16 (RX)';
            } else {
                pinsCell.textContent = isXiao ? this.formatGpioPin(44) + ' (RX)' : 'GPIO 11 (RX)';
            }
        } else if (role === '5' || role === '5_0' || role === '5_1') { // SBUS OUT (Binary or Text)
            if (isMiniKit) {
                pinsCell.textContent = 'GPIO 17 (TX)';
            } else {
                pinsCell.textContent = isXiao ? this.formatGpioPin(43) + ' (TX)' : 'GPIO 12 (TX)';
            }
        } else {
            pinsCell.textContent = 'N/A';
        }
    },

    // Show/hide Device 3 SBUS rate selector based on role
    updateDevice3Config() {
        const device3Role = document.getElementById('device3_role');
        const device3SbusRate = document.getElementById('device3_sbus_rate');
        if (device3Role && device3SbusRate) {
            // Show rate selector only for SBUS Text Output mode (role = 5_1)
            device3SbusRate.style.display = (device3Role.value === '5_1') ? 'inline-block' : 'none';
        }
    },

    updateDevice4Pins() {
        const device4Role = document.getElementById('device4_role');
        const pinsCell = document.getElementById('device4_pins');

        if (!device4Role || !pinsCell) return;

        const role = device4Role.value;

        // Network roles: 1, 2, 3, 3_0, 3_1, 4
        if (role === '1' || role === '2' || role === '3' || role === '3_0' || role === '3_1' || role === '4') {
            pinsCell.textContent = 'Network';
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

        // Update UART Configuration visibility
        this.updateUartConfigVisibility();

        // Initialize Device4 config and validation on page load
        this.updateDevice4Config(false);

        // Update protocol field state based on SBUS roles
        this.updateProtocolFieldState();
    },

    updateUartConfigVisibility() {
        // Check if any device uses UART
        const hasUartDevices = (
            this.config.device1Role === '0' ||  // D1_UART1 (bridge mode)
            this.config.device2Role === '1' ||  // D2_UART2
            this.config.device3Role === '1' ||  // D3_UART3_MIRROR
            this.config.device3Role === '2' ||  // D3_UART3_BRIDGE
            this.config.device3Role === '3'     // D3_UART3_LOG
        );

        // Find UART Configuration block in Advanced Configuration
        const uartConfigBlock = document.getElementById('uartConfigBlock');

        if (uartConfigBlock) {
            if (hasUartDevices) {
                uartConfigBlock.style.display = 'block';
            } else {
                uartConfigBlock.style.display = 'none';
            }
        }
    },
    
    updateProtocolFieldState() {
        // Check if any device has SBUS role active (read from DOM for real-time updates)
        // D1: 1=SBUS_IN; D2: 3=SBUS_IN, 4=SBUS_OUT; D3: 4=SBUS_IN, 5/5_0/5_1=SBUS_OUT; D4: 3/3_0/3_1=SBUS_TX, 4=SBUS_RX
        const d1Role = document.getElementById('device1_role')?.value;
        const d2Role = document.getElementById('device2_role')?.value;
        const d3Role = document.getElementById('device3_role')?.value;
        const d4Role = document.getElementById('device4_role')?.value;

        const isSbusActive = (
            d1Role === '1' ||
            d2Role === '3' || d2Role === '4' ||
            d3Role === '4' || d3Role?.startsWith('5') ||
            d4Role?.startsWith('3') || d4Role === '4'
        );

        // Protocol optimization field should be blocked when SBUS is active
        const protocolField = document.getElementById('protocol_optimization');
        if (protocolField) {
            protocolField.disabled = isSbusActive;
            // Add visual indication
            protocolField.style.opacity = isSbusActive ? '0.5' : '1';
            protocolField.style.cursor = isSbusActive ? 'not-allowed' : 'default';

            if (isSbusActive) {
                // Set to SBUS protocol when SBUS is active
                if (protocolField.value !== '2') {
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
            } else {
                // Reset to None when no SBUS devices active (if was SBUS)
                if (protocolField.value === '2') {
                    protocolField.value = '0';  // Reset to None
                }
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

    updateDevice4Config(isRoleChange = false) {
        const device4Role = document.getElementById('device4_role');
        const device4AdvancedConfig = document.getElementById('device4AdvancedConfig');
        const targetIpInput = document.getElementById('device4_target_ip');
        const portInput = document.getElementById('device4_port');
        const networkLogLevel = document.getElementById('log_level_network');

        if (!device4Role || !device4AdvancedConfig) return;

        const role = device4Role.value;

        // Handle Disabled role first - hide config and remove validation
        if (role === '0') {
            device4AdvancedConfig.style.display = 'none';
            if (targetIpInput) {
                targetIpInput.disabled = true;
                targetIpInput.removeAttribute('required');
                targetIpInput.removeAttribute('pattern');
            }
            if (portInput) portInput.disabled = true;
            if (networkLogLevel) networkLogLevel.disabled = true;
            return;
        }

        // Extract base role (handle composite values like 3_0, 3_1)
        const baseRole = role.split('_')[0];

        // Network Configuration block needed for all network roles (1, 2, 3, 3_0, 3_1, 4)
        // - Network Bridge (1): needs IP and port to send
        // - Network Logger (2): needs IP and port to send
        // - SBUS→UDP TX (3, 3_0, 3_1): needs IP and port to send
        // - SBUS UDP RX (4): needs port to listen (IP ignored)
        if (baseRole === '1' || baseRole === '2' || baseRole === '3' || baseRole === '4') {
            device4AdvancedConfig.style.display = 'block';

            // Show/hide UDP timeout field (only for SBUS Input role)
            const timeoutGroup = document.getElementById('device4_udp_timeout_group');
            if (timeoutGroup) {
                timeoutGroup.style.display = (baseRole === '4') ? 'block' : 'none';
            }

            // Hide Send Rate from Network Config (now in device options column)
            const sendRateGroup = document.getElementById('device4_send_rate_group');
            if (sendRateGroup) {
                sendRateGroup.style.display = 'none';
            }

            // Enable/disable fields based on role
            if (baseRole === '4') {
                // SBUS UDP RX - only port needed (listens on all interfaces)
                if (targetIpInput) {
                    targetIpInput.disabled = true;  // IP not used for RX
                    targetIpInput.value = '0.0.0.0';  // Show that it listens on all
                    targetIpInput.removeAttribute('required');  // Not required when disabled
                    targetIpInput.removeAttribute('pattern');  // Remove pattern validation when disabled
                }
                if (portInput) portInput.disabled = false;
            } else {
                // TX roles - need both IP and port
                if (targetIpInput) {
                    targetIpInput.disabled = false;
                    targetIpInput.setAttribute('required', 'required');  // Required for TX
                    targetIpInput.setAttribute('pattern', '^(?:[0-9]{1,3}\\.){3}[0-9]{1,3}$');  // IP validation
                    // Clear invalid placeholder if switching from RX role
                    if (targetIpInput.value === '0.0.0.0') {
                        targetIpInput.value = '';
                    }
                }
                if (portInput) portInput.disabled = false;
            }

            // Only update port when role changes, not on initial load
            if (isRoleChange) {
                if (baseRole === '1') {  // Network Bridge
                    portInput.value = '14550';
                } else if (baseRole === '2') {  // Network Logger
                    portInput.value = '14560';
                } else if (baseRole === '3') {  // SBUS→UDP TX (Binary or Text)
                    portInput.value = '14551';
                } else if (baseRole === '4') {  // SBUS UDP RX
                    portInput.value = '14552';  // Listen port
                }
            }

            // Set IP defaults only if field is empty (and not RX role)
            if (baseRole !== '4' && (!targetIpInput.value || targetIpInput.value === '')) {
                const wifiMode = document.getElementById('wifi_mode');
                const isApMode = wifiMode && wifiMode.value === '0';

                if (isApMode) {
                    // AP mode: use broadcast by default
                    targetIpInput.value = '192.168.4.255';
                } else {
                    // Client mode: try to get client IP, fallback to broadcast
                    fetch('/client-ip')
                        .then(response => response.text())
                        .then(clientIP => {
                            targetIpInput.value = clientIP;
                        })
                        .catch(() => {
                            targetIpInput.value = '192.168.4.255';
                        });
                }
            }
        } else {
            device4AdvancedConfig.style.display = 'none';

            // Disable IP/Port fields to prevent validation when hidden
            if (targetIpInput) targetIpInput.disabled = true;
            if (portInput) portInput.disabled = true;
        }

        // Enable/disable Network Log Level based on Device 4 being active (any role except 0)
        if (networkLogLevel) {
            if (role !== '0') {
                networkLogLevel.disabled = false;
            } else {
                networkLogLevel.disabled = true;
                networkLogLevel.value = '-1'; // Set to OFF
            }
        }

        // Update auto broadcast state (visibility and target IP state)
        this.updateAutoBroadcastState();
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
    },

    onDevice1RoleChange() {
        const role = document.getElementById('device1_role').value;
        this.config.device1Role = role;  // Update config

        if (role === '1') {  // SBUS_IN
            // Disable incompatible Device2/3 options when D1_SBUS_IN
            this.updateDeviceOptionsForSBUS();
        } else {
            // Re-enable all options when D1_UART1
            this.enableAllDeviceOptions();
        }

        this.updateDevice1Pins();  // Update pin display
        this.updateUartConfigVisibility();  // Update UART config visibility
        this.updateDevice4Options();  // Update Device4 context
        this.updateSbusConfigWarning();  // Check SBUS config validity
        this.updateProtocolFieldState();  // Lock protocol to SBUS when SBUS role active
    },

    updateDevice1Pins() {
        const device1Role = document.getElementById('device1_role');
        const pinsCell = document.getElementById('device1_pins');

        if (!device1Role || !pinsCell) return;

        const role = device1Role.value;

        if (role === '0') { // UART Bridge
            pinsCell.textContent = this.formatGpioPinPair(4, 5);
        } else if (role === '1') { // SBUS IN
            pinsCell.textContent = this.formatGpioPin(4) + ' (RX)';
        }
    },

    updateDeviceOptionsForSBUS() {
        const device2Select = document.getElementById('device2_role');
        const device3Select = document.getElementById('device3_role');

        // Update Device2 options
        // Note: USB (value=2) remains enabled - it can output SBUS in Text/MAVLink format
        // UART2 disabled because SBUS uses different baud rate (100000 8E2 vs standard UART)
        Array.from(device2Select.options).forEach(option => {
            if (option.value === '1') {  // D2_UART2
                option.disabled = true;
            }
        });

        // Update Device3 options
        // UART3 Bridge disabled because SBUS uses different baud rate
        Array.from(device3Select.options).forEach(option => {
            if (option.value === '2') {  // D3_UART3_BRIDGE
                option.disabled = true;
            }
        });

    },

    enableAllDeviceOptions() {
        const device2Select = document.getElementById('device2_role');
        const device3Select = document.getElementById('device3_role');

        // Re-enable all Device2 options (except UART2-based options on MiniKit)
        Array.from(device2Select.options).forEach(option => {
            // Don't re-enable options that are physically unavailable (MiniKit)
            if (option.text.includes('(not available)')) {
                return;  // Keep disabled
            }
            option.disabled = false;
        });

        // Re-enable all Device3 options
        Array.from(device3Select.options).forEach(option => {
            option.disabled = false;
        });

    },

    updateDevice4Options() {
        const device1Role = document.getElementById('device1_role').value;
        const device2Role = document.getElementById('device2_role').value;
        const device3Role = document.getElementById('device3_role')?.value;

        // Check for SBUS roles (including composite values)
        const hasSbus = (device1Role === '1' ||  // D1_SBUS_IN
                         device2Role === '3' || device2Role === '4' || device2Role === '5' ||  // D2_SBUS_IN/OUT/USB_TEXT
                         device3Role === '4' || device3Role?.startsWith('5'));   // D3_SBUS_IN/OUT (5, 5_0, 5_1)

        const select = document.getElementById('device4_role');
        if (!select) return;

        const currentValue = select.value;
        select.innerHTML = '';

        if (hasSbus) {
            // SBUS context - split TX/RX with separate Binary/Text options
            this.addOption(select, '0', 'Disabled');
            this.addOption(select, '2', 'UDP Logger');
            this.addOption(select, '3_0', 'SBUS Output');
            this.addOption(select, '3_1', 'SBUS Text Output');
            this.addOption(select, '4', 'SBUS Input');
        } else {
            // Normal context
            this.addOption(select, '0', 'Disabled');
            this.addOption(select, '1', 'Network Bridge');
            this.addOption(select, '2', 'UDP Logger');
        }

        // Restore value if possible
        if (select.querySelector(`option[value="${currentValue}"]`)) {
            select.value = currentValue;
        }
    },

    // Called when any device role changes
    onDeviceRoleChange() {
        // Auto-set Device1 to SBUS_IN when any other device has SBUS role
        // This saves ~14KB RAM (minimal DMA buffers) since UART bridge is unused in SBUS mode
        const device1Role = document.getElementById('device1_role');
        const device2Role = document.getElementById('device2_role')?.value;
        const device3Role = document.getElementById('device3_role')?.value;
        const device4Role = document.getElementById('device4_role')?.value;
        const device5Role = document.getElementById('device5_role')?.value;

        // Check for SBUS roles (including composite values like 5_0, 5_1, 3_0, 3_1)
        const otherHasSbus = (
            device2Role === '3' || device2Role === '4' || device2Role === '5' ||  // D2_SBUS_IN/OUT/USB_SBUS_TEXT
            device3Role === '4' || device3Role?.startsWith('5') ||  // D3_SBUS_IN/OUT (5, 5_0, 5_1)
            device4Role?.startsWith('3') || device4Role === '4' ||  // D4_SBUS_UDP_TX (3, 3_0, 3_1) / RX
            device5Role === '2'  // D5_BT_SBUS_TEXT
        );

        if (device1Role && otherHasSbus && device1Role.value !== '1') {
            device1Role.value = '1';  // D1_SBUS_IN
            this.onDevice1RoleChange();  // Trigger Device1 change handlers
        }

        this.updateDevice2Config();
        this.updateDevice2Pins();
        this.updateDevice3Config();
        this.updateDevice4Options();
        this.updateDevice4SbusRate();
        this.updateSbusFormatOptions();
        this.updateProtocolFieldState();
        this.updateDevice5BridgeState();
    },

    // Update SBUS-related UI elements
    updateSbusFormatOptions() {
        // Format selectors are no longer used - format is now part of the role
        // Just update the configuration warning
        this.updateSbusConfigWarning();
    },

    // Check if SBUS Output is configured without SBUS Input
    updateSbusConfigWarning() {
        const device1Role = document.getElementById('device1_role');
        const device2Role = document.getElementById('device2_role');
        const device3Role = document.getElementById('device3_role');
        const device4Role = document.getElementById('device4_role');
        const warning = document.getElementById('sbus-config-warning');

        if (!warning) return;

        // Check for SBUS Input sources
        const hasSbusInput = (
            (device1Role && device1Role.value === '1') ||  // D1_SBUS_IN
            (device2Role && device2Role.value === '3') ||  // D2_SBUS_IN
            (device3Role && device3Role.value === '4') ||  // D3_SBUS_IN
            (device4Role && device4Role.value === '4')     // D4_UDP_SBUS_RX (also a source)
        );

        // Check for SBUS Output destinations (including composite values)
        const hasSbusOutput = (
            (device2Role && (device2Role.value === '4' || device2Role.value === '5')) ||  // D2_SBUS_OUT/USB_TEXT
            (device3Role && device3Role.value?.startsWith('5')) ||  // D3_SBUS_OUT (5, 5_0, 5_1)
            (device4Role && device4Role.value?.startsWith('3'))     // D4_SBUS_UDP_TX (3, 3_0, 3_1)
        );

        // Show warning if output without input
        warning.style.display = (hasSbusOutput && !hasSbusInput) ? 'block' : 'none';
    },

    addOption(select, value, text) {
        const option = document.createElement('option');
        option.value = value;
        option.textContent = text;
        select.appendChild(option);
    },

    // Update auto broadcast checkbox visibility and target IP state
    updateAutoBroadcastState() {
        const autoBroadcast = document.getElementById('device4_auto_broadcast');
        const autoBroadcastGroup = document.getElementById('device4_auto_broadcast_group');
        const targetIpInput = document.getElementById('device4_target_ip');
        const targetIpGroup = document.getElementById('device4_target_ip_group');
        const wifiMode = document.getElementById('wifi_mode');
        const device4Role = document.getElementById('device4_role');

        if (!autoBroadcast || !targetIpInput) return;

        // Auto broadcast only available in Client mode (wifi_mode == 1)
        // and only for TX roles (Network Bridge=1, SBUS UDP TX=3)
        const isClientMode = wifiMode && wifiMode.value === '1';
        const isApMode = wifiMode && wifiMode.value === '0';
        const isTxRole = device4Role && (device4Role.value === '1' || device4Role.value === '3');
        const showAutoBroadcast = isClientMode && isTxRole;

        // In AP mode, set default broadcast IP if target is empty
        if (isApMode && isTxRole && (!targetIpInput.value || targetIpInput.value === '')) {
            targetIpInput.value = '192.168.4.255';
        }

        // Show/hide auto broadcast checkbox (using visibility to keep layout stable)
        if (autoBroadcastGroup) {
            autoBroadcastGroup.style.visibility = showAutoBroadcast ? 'visible' : 'hidden';
        }

        // If not visible, uncheck auto broadcast
        if (!showAutoBroadcast && autoBroadcast.checked) {
            autoBroadcast.checked = false;
        }

        // Disable target IP when auto broadcast is checked
        if (autoBroadcast.checked) {
            targetIpInput.disabled = true;
            targetIpInput.style.opacity = '0.5';
            targetIpInput.removeAttribute('required');
            targetIpInput.removeAttribute('pattern');
            if (targetIpGroup) {
                targetIpGroup.style.opacity = '0.5';
            }
        } else {
            // Re-enable unless in RX mode (role 4) or Disabled (role 0)
            const device4Role = document.getElementById('device4_role');
            if (device4Role && device4Role.value !== '4' && device4Role.value !== '0') {
                targetIpInput.disabled = false;
                targetIpInput.style.opacity = '1';
                targetIpInput.setAttribute('required', 'required');
                targetIpInput.setAttribute('pattern', '^(?:[0-9]{1,3}\\.){3}[0-9]{1,3}(?:,\\s*(?:[0-9]{1,3}\\.){3}[0-9]{1,3})*$');
                if (targetIpGroup) {
                    targetIpGroup.style.opacity = '1';
                }
            }
        }
    },

    // Device 5 role change handler (Bluetooth SPP - MiniKit only)
    onDevice5RoleChange() {
        const device5Role = document.getElementById('device5_role');
        if (device5Role) {
            this.config.device5Role = device5Role.value;
            this.updateDevice5Config();
            this.onDeviceRoleChange();  // Check if Device1 should be auto-set to SBUS_IN
        }
    },

    // Show/hide Device 2 SBUS rate selector based on role
    updateDevice2Config() {
        const device2Role = document.getElementById('device2_role');
        const device2SbusRate = document.getElementById('device2_sbus_rate');
        if (device2Role && device2SbusRate) {
            // Show rate selector only for USB SBUS Text mode (role = 5)
            device2SbusRate.style.display = (device2Role.value === '5') ? 'inline-block' : 'none';
        }
    },

    // Show/hide Device 5 options based on role
    updateDevice5Config() {
        const device5Role = document.getElementById('device5_role');
        const btSendRate = document.getElementById('bt_send_rate');
        if (device5Role && btSendRate) {
            // Show rate selector only for SBUS Text mode (role = 2)
            btSendRate.style.display = (device5Role.value === '2') ? 'inline-block' : 'none';
        }
    },

    // Show/hide Device 4 SBUS rate selector based on role
    updateDevice4SbusRate() {
        const device4Role = document.getElementById('device4_role');
        const device4SbusRate = document.getElementById('device4_sbus_rate');
        if (device4Role && device4SbusRate) {
            // Show rate selector only for SBUS Text Output mode (role = 3_1)
            device4SbusRate.style.display = (device4Role.value === '3_1') ? 'inline-block' : 'none';
        }
    },

    // Disable Device 5 Bridge option when SBUS roles are active in other devices
    updateDevice5BridgeState() {
        const device5Role = document.getElementById('device5_role');
        if (!device5Role) return;

        const device1Role = document.getElementById('device1_role')?.value;
        const device2Role = document.getElementById('device2_role')?.value;
        const device3Role = document.getElementById('device3_role')?.value;
        const device4Role = document.getElementById('device4_role')?.value;

        // Check if any SBUS role is active
        const hasSbusActive = (
            device1Role === '1' ||  // D1_SBUS_IN
            device2Role === '3' || device2Role === '4' || device2Role === '5' ||  // D2_SBUS_IN/OUT/USB_TEXT
            device3Role === '4' || device3Role?.startsWith('5') ||  // D3_SBUS_IN/OUT
            device4Role?.startsWith('3') || device4Role === '4'  // D4_SBUS_TX/RX
        );

        // Find Bridge option (value = 1)
        const bridgeOption = device5Role.querySelector('option[value="1"]');
        if (bridgeOption) {
            if (hasSbusActive) {
                bridgeOption.disabled = true;
                if (!bridgeOption.text.includes('(SBUS mode)')) {
                    bridgeOption.text += ' (SBUS mode)';
                }
                // If Bridge was selected, switch to Disabled
                if (device5Role.value === '1') {
                    device5Role.value = '0';
                    this.updateDevice5Config();
                }
            } else {
                bridgeOption.disabled = false;
                bridgeOption.text = bridgeOption.text.replace(' (SBUS mode)', '');
            }
        }
    }
};