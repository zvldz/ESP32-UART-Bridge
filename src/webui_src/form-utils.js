// ESP32 UART Bridge - Form Utilities Module

const FormUtils = {
    config: null,
    
    init(config) {
        this.config = config;
        this.populateSelects();
        this.setFormValues();
        this.attachListeners();
    },
    
    populateSelects() {
        // Populate baud rates
        const baudRates = [4800, 9600, 19200, 38400, 57600, 74880, 115200, 230400, 250000, 460800, 500000, 921600, 1000000];
        const baudSelect = document.getElementById('baudrate');
        if (baudSelect) {
            baudRates.forEach(rate => {
                const option = document.createElement('option');
                option.value = rate;
                option.textContent = rate;
                baudSelect.appendChild(option);
            });
        }
        
        // Populate data bits
        const dataBitsSelect = document.getElementById('databits');
        if (dataBitsSelect) {
            [7, 8].forEach(bits => {
                const option = document.createElement('option');
                option.value = bits;
                option.textContent = bits;
                dataBitsSelect.appendChild(option);
            });
        }
        
        // Populate parity
        const paritySelect = document.getElementById('parity');
        if (paritySelect) {
            const parityOptions = {none: 'None', even: 'Even', odd: 'Odd'};
            Object.entries(parityOptions).forEach(([value, text]) => {
                const option = document.createElement('option');
                option.value = value;
                option.textContent = text;
                paritySelect.appendChild(option);
            });
        }
        
        // Populate stop bits
        const stopBitsSelect = document.getElementById('stopbits');
        if (stopBitsSelect) {
            [1, 2].forEach(bits => {
                const option = document.createElement('option');
                option.value = bits;
                option.textContent = bits;
                stopBitsSelect.appendChild(option);
            });
        }
    },
    
    setFormValues() {
        // UART settings
        this.setSelectValue('baudrate', this.config.baudrate);
        this.setSelectValue('databits', this.config.databits);
        this.setSelectValue('parity', this.config.parity);
        this.setSelectValue('stopbits', this.config.stopbits);
        
        // Checkboxes
        const flowControl = document.getElementById('flowcontrol');
        if (flowControl) flowControl.checked = this.config.flowcontrol;
        
        const permanent = document.getElementById('permanent_wifi');
        if (permanent) permanent.checked = this.config.permanentWifi;
        
        // USB mode
        this.setSelectValue('usbmode', this.config.usbMode);

        // Disable USB Host for boards that don't support it
        if (this.config.usbHostSupported === false) {
            const usbModeSelect = document.getElementById('usbmode');
            if (usbModeSelect) {
                // Find and disable host option
                for (let option of usbModeSelect.options) {
                    if (option.value === 'host') {
                        option.disabled = true;
                        option.text = "Host (Not supported on this board)";
                        break;
                    }
                }
                // Force device mode if somehow set to host
                if (usbModeSelect.value === 'host') {
                    usbModeSelect.value = 'device';
                }
            }
        }
        
        // Protocol optimization
        this.setSelectValue('protocol_optimization', this.config.protocolOptimization || 0);
        
        // MAVLink routing
        const mavlinkRouting = document.getElementById('mavlink-routing');
        if (mavlinkRouting) {
            mavlinkRouting.checked = this.config.mavlinkRouting || false;
        }

        // SBUS Timing Keeper
        const sbusTimingKeeper = document.getElementById('sbus-timing-keeper');
        if (sbusTimingKeeper) {
            sbusTimingKeeper.checked = this.config.sbusTimingKeeper || false;
        }
        
        // Update MAVLink routing visibility
        this.updateMavlinkRoutingVisibility();

        // Update SBUS Timing Keeper visibility
        this.updateSbusTimingKeeperVisibility();
        
        // Set WiFi mode
        const wifiMode = document.getElementById('wifi_mode');
        if (wifiMode) wifiMode.value = this.config.wifiMode || 0;
        
        // WiFi settings
        const ssid = document.getElementById('ssid');
        const password = document.getElementById('password');
        if (ssid) ssid.value = this.config.ssid;
        if (password) password.value = this.config.password;
        
        // Set client WiFi networks
        const networks = this.config.wifiNetworks || [];
        for (let i = 0; i < 5; i++) {
            const ssidInput = document.getElementById(`wifi_network_ssid_${i}`);
            const passInput = document.getElementById(`wifi_network_pass_${i}`);
            if (ssidInput) ssidInput.value = networks[i]?.ssid || '';
            if (passInput) passInput.value = networks[i]?.password || '';
        }

        // WiFi TX Power, AP Channel and mDNS
        const wifiTxPower = document.getElementById('wifi_tx_power');
        const wifiApChannel = document.getElementById('wifi_ap_channel');
        const mdnsHostname = document.getElementById('mdns_hostname');
        const mdnsUrl = document.getElementById('mdns_url');
        if (wifiTxPower) wifiTxPower.value = this.config.wifiTxPower || 20;
        if (wifiApChannel) wifiApChannel.value = this.config.wifiApChannel || 1;
        if (mdnsHostname) mdnsHostname.value = this.config.mdnsHostname || '';
        if (mdnsUrl) mdnsUrl.textContent = this.config.mdnsHostname || 'hostname';
        
        // Show correct settings based on mode
        this.updateWiFiModeDisplay();

        // Note: updateAutoBroadcastState() is called from DeviceConfig.setFormValues()
        // after device4_role is properly set

        // Update log count display
        const logCount = document.getElementById('logCount');
        if (logCount) logCount.textContent = this.config.logDisplayCount;
    },
    
    setSelectValue(id, value) {
        const select = document.getElementById(id);
        if (select) select.value = value;
    },
    
    attachListeners() {
        // Form submission
        const form = document.getElementById('settingsForm');
        if (form) {
            form.addEventListener('submit', (e) => this.handleSubmit(e));
        }
        
        // WiFi mode change listener
        const wifiMode = document.getElementById('wifi_mode');
        if (wifiMode) {
            wifiMode.addEventListener('change', () => {
                this.updateWiFiModeDisplay();
                // Update auto broadcast state when WiFi mode changes
                if (typeof DeviceConfig !== 'undefined') {
                    DeviceConfig.updateAutoBroadcastState();
                }
            });
        }

        // mDNS hostname change listener - update preview URL
        const mdnsHostname = document.getElementById('mdns_hostname');
        if (mdnsHostname) {
            mdnsHostname.addEventListener('input', () => {
                const mdnsUrl = document.getElementById('mdns_url');
                if (mdnsUrl) {
                    mdnsUrl.textContent = mdnsHostname.value || 'hostname';
                }
            });
        }

        // Protocol optimization change listener
        const protocolOpt = document.getElementById('protocol_optimization');
        if (protocolOpt) {
            protocolOpt.addEventListener('change', () => {
                this.updateMavlinkRoutingVisibility();
                this.updateSbusTimingKeeperVisibility();
            });
        }
        
        // Password toggle - attach to global function
        window.togglePassword = () => this.togglePassword();
        window.toggleNetworkPassword = (index) => this.toggleNetworkPassword(index);
        window.toggleAdditionalNetworks = () => this.toggleAdditionalNetworks();
        
        // WiFi config toggle
        window.toggleWifiConfig = () => this.toggleWifiConfig();
        
        // Firmware update toggle
        window.toggleFirmwareUpdate = () => this.toggleFirmwareUpdate();
        
        // Config backup toggle
        window.toggleConfigBackup = () => this.toggleConfigBackup();
        
        // Config import handler
        window.handleConfigImport = (input) => this.handleConfigImport(input);
        
        // Firmware update handler
        window.handleFirmwareUpdate = () => this.handleFirmwareUpdate();

        // Factory reset handler
        window.factoryReset = () => this.factoryReset();
    },
    
    handleSubmit(e) {
        e.preventDefault(); // Prevent normal form submission
        
        // Validate form before submission
        if (!this.validateForm()) {
            return false;
        }
        
        const form = e.target;
        const submitButton = document.getElementById('saveButton');
        
        // Show loading state on submit button
        if (submitButton) {
            submitButton.disabled = true;
            submitButton.textContent = 'Saving...';
        }
        
        // Submit via fetch to handle JSON response
        const formData = new FormData(form);

        // Handle composite role values (e.g., "5_1" -> role=5, format=1)
        this.convertCompositeRoles(formData);

        fetch('/save', {
            method: 'POST',
            body: formData
        })
        .then(response => {
            // Check if response is JSON before parsing
            const contentType = response.headers.get('content-type');
            if (!contentType || !contentType.includes('application/json')) {
                // Device likely rebooting or disconnected - start reconnect
                throw new Error('DEVICE_REBOOTING');
            }
            if (!response.ok) {
                // Handle HTTP errors (like 400)
                return response.json().then(data => {
                    throw new Error(data.message || 'Server error');
                });
            }
            return response.json();
        })
        .then(data => {
            if (data.status === 'ok') {
                if (submitButton) {
                    submitButton.disabled = true;
                    submitButton.style.backgroundColor = '#2196F3';  // Blue for waiting
                }

                // Start reconnection countdown
                this.startReconnectCountdown(submitButton);
            } else if (data.status === 'unchanged') {
                if (submitButton) {
                    submitButton.textContent = 'No Changes';
                    submitButton.disabled = false;
                }
                // Reset button after a moment
                setTimeout(() => {
                    if (submitButton) {
                        submitButton.textContent = 'Save & Reboot';
                    }
                }, 2000);
            } else if (data.status === 'error') {
                // Handle server-side validation errors
                alert(data.message || 'Configuration error');
                if (submitButton) {
                    submitButton.textContent = 'Save Failed';
                    submitButton.disabled = false;
                    submitButton.style.backgroundColor = '#dc3545'; // Red color
                }
                setTimeout(() => {
                    if (submitButton) {
                        submitButton.textContent = 'Save & Reboot';
                        submitButton.style.backgroundColor = '#4CAF50'; // Reset to green
                    }
                }, 3000);
            }
        })
        .catch(error => {
            console.error('Save error:', error);

            // If device is rebooting (non-JSON response), start reconnect
            if (error.message === 'DEVICE_REBOOTING' || error.name === 'TypeError') {
                if (submitButton) {
                    submitButton.disabled = true;
                    submitButton.style.backgroundColor = '#2196F3';
                }
                this.startReconnectCountdown(submitButton);
                return;
            }

            // Show error message for other errors
            alert(error.message || 'Failed to save configuration');

            if (submitButton) {
                submitButton.textContent = 'Save Failed';
                submitButton.disabled = false;
                submitButton.style.backgroundColor = '#dc3545'; // Red color
            }
            // Reset button after a moment
            setTimeout(() => {
                if (submitButton) {
                    submitButton.textContent = 'Save & Reboot';
                    submitButton.style.backgroundColor = '#4CAF50'; // Reset to green
                }
            }, 3000);
        });
        
        return false;
    },

    // Countdown and auto-reconnect after save
    startReconnectCountdown(button) {
        const REBOOT_WAIT_SECONDS = 8;
        const MAX_RECONNECT_ATTEMPTS = 30;
        let countdown = REBOOT_WAIT_SECONDS;
        let attempts = 0;

        // Phase 1: Countdown while device reboots
        const countdownInterval = setInterval(() => {
            if (button) {
                button.textContent = `Rebooting... ${countdown}s`;
            }
            countdown--;

            if (countdown < 0) {
                clearInterval(countdownInterval);
                // Phase 2: Start reconnection attempts
                this.attemptReconnect(button, attempts, MAX_RECONNECT_ATTEMPTS);
            }
        }, 1000);
    },

    // Firmware update reconnect (longer wait for flash write)
    startFirmwareReconnect(button, statusText) {
        const REBOOT_WAIT_SECONDS = 12;  // Longer wait for firmware flash
        const MAX_RECONNECT_ATTEMPTS = 30;
        let countdown = REBOOT_WAIT_SECONDS;

        const countdownInterval = setInterval(() => {
            if (statusText) {
                statusText.textContent = `Device rebooting... ${countdown}s`;
            }
            countdown--;

            if (countdown < 0) {
                clearInterval(countdownInterval);
                this.attemptFirmwareReconnect(button, statusText, 0, MAX_RECONNECT_ATTEMPTS);
            }
        }, 1000);
    },

    attemptFirmwareReconnect(button, statusText, attempt, maxAttempts) {
        if (attempt >= maxAttempts) {
            if (statusText) {
                statusText.textContent = 'Please refresh page manually';
                statusText.style.color = '#ff9800';
            }
            if (button) {
                button.disabled = false;
                button.textContent = 'Refresh Page';
                button.onclick = () => window.location.reload();
            }
            return;
        }

        if (statusText) {
            statusText.textContent = `Reconnecting... (${attempt + 1}/${maxAttempts})`;
        }

        fetch('/status', { cache: 'no-store' })
            .then(response => {
                if (response.ok) {
                    if (statusText) {
                        statusText.textContent = 'Connected! Reloading...';
                        statusText.style.color = 'green';
                    }
                    setTimeout(() => window.location.reload(), 500);
                } else {
                    throw new Error('Not ready');
                }
            })
            .catch(() => {
                setTimeout(() => {
                    this.attemptFirmwareReconnect(button, statusText, attempt + 1, maxAttempts);
                }, 1000);
            });
    },

    attemptReconnect(button, attempt, maxAttempts) {
        if (attempt >= maxAttempts) {
            // Give up, show manual refresh message
            if (button) {
                button.textContent = 'Please refresh manually';
                button.style.backgroundColor = '#ff9800';  // Orange
                button.disabled = false;
                button.onclick = () => window.location.reload();
            }
            return;
        }

        if (button) {
            button.textContent = `Reconnecting... (${attempt + 1}/${maxAttempts})`;
        }

        // Try to fetch status
        fetch('/status', { cache: 'no-store' })
            .then(response => {
                if (response.ok) {
                    // Device is back online - reload page
                    if (button) {
                        button.textContent = 'Connected! Reloading...';
                        button.style.backgroundColor = '#4CAF50';
                    }
                    setTimeout(() => window.location.reload(), 500);
                } else {
                    throw new Error('Not ready');
                }
            })
            .catch(() => {
                // Device not ready yet, retry after 1 second
                setTimeout(() => {
                    this.attemptReconnect(button, attempt + 1, maxAttempts);
                }, 1000);
            });
    },

    togglePassword() {
        const passwordInput = document.getElementById('password');
        const icon = document.getElementById('toggleIcon');
        
        if (!passwordInput || !icon) return;
        
        if (passwordInput.type === 'password') {
            passwordInput.type = 'text';
            // Eye with slash (hidden)
            icon.innerHTML = `
                <svg width="20" height="20" viewBox="0 0 20 20" fill="none" stroke="#666" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
                    <path d="M13.875 13.875A8.5 8.5 0 016.125 6.125m-3.27.73A9.98 9.98 0 001 10s3 6 9 6a9.98 9.98 0 003.145-.855M8.29 4.21A4.95 4.95 0 0110 4c6 0 9 6 9 6a17.46 17.46 0 01-2.145 2.855M1 1l18 18"></path>
                </svg>
            `;
        } else {
            passwordInput.type = 'password';
            // Open eye
            icon.innerHTML = `
                <svg width="20" height="20" viewBox="0 0 20 20" fill="none" stroke="#666" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
                    <path d="M1 10s3-6 9-6 9 6 9 6-3 6-9 6-9-6-9-6z"></path>
                    <circle cx="10" cy="10" r="3"></circle>
                </svg>
            `;
        }
    },
    
    toggleNetworkPassword(index) {
        const passwordInput = document.getElementById(`wifi_network_pass_${index}`);
        if (!passwordInput) return;

        // Find the icon span next to the input
        const icon = passwordInput.parentElement.querySelector('span');
        if (!icon) return;

        if (passwordInput.type === 'password') {
            passwordInput.type = 'text';
            icon.innerHTML = `
                <svg width="20" height="20" viewBox="0 0 20 20" fill="none" stroke="#666" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
                    <path d="M13.875 13.875A8.5 8.5 0 016.125 6.125m-3.27.73A9.98 9.98 0 001 10s3 6 9 6a9.98 9.98 0 003.145-.855M8.29 4.21A4.95 4.95 0 0110 4c6 0 9 6 9 6a17.46 17.46 0 01-2.145 2.855M1 1l18 18"></path>
                </svg>
            `;
        } else {
            passwordInput.type = 'password';
            icon.innerHTML = `
                <svg width="20" height="20" viewBox="0 0 20 20" fill="none" stroke="#666" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
                    <path d="M1 10s3-6 9-6 9 6 9 6-3 6-9 6-9-6-9-6z"></path>
                    <circle cx="10" cy="10" r="3"></circle>
                </svg>
            `;
        }
    },

    toggleAdditionalNetworks() {
        Utils.toggleElement('additionalNetworksContent', 'additionalNetworksArrow');
    },
    
    updateWiFiModeDisplay() {
        const wifiMode = document.getElementById('wifi_mode');
        const apSettings = document.getElementById('apModeSettings');
        const clientSettings = document.getElementById('clientModeSettings');
        const apChannelGroup = document.getElementById('ap_channel_group');

        if (!wifiMode || !apSettings || !clientSettings) return;

        if (wifiMode.value === '0') {  // AP Mode
            apSettings.style.display = 'block';
            clientSettings.style.display = 'none';
            if (apChannelGroup) apChannelGroup.style.display = 'block';
        } else {  // Client Mode
            apSettings.style.display = 'none';
            clientSettings.style.display = 'block';
            if (apChannelGroup) apChannelGroup.style.display = 'none';
        }
    },
    
    toggleWifiConfig() {
        Utils.toggleElement('wifiConfigContent', 'wifiArrow');
    },
    
    async toggleFirmwareUpdate() {
        const el = document.getElementById('firmwareUpdateContent');
        if (el && el.dataset.fragment) {
            await Utils.loadFragment(el);
        }
        Utils.toggleElement('firmwareUpdateContent', 'firmwareArrow');
    },
    
    // Validate form before submission
    validateForm() {
        const wifiMode = document.getElementById('wifi_mode');
        
        if (wifiMode && wifiMode.value === '0') {
            // AP mode validation
            const ssid = document.getElementById('ssid');
            const password = document.getElementById('password');
            
            if (ssid && ssid.value.trim() === '') {
                alert('WiFi AP SSID cannot be empty');
                return false;
            }
            
            if (password && password.value.length > 0 && password.value.length < 8) {
                alert('WiFi AP password must be at least 8 characters or empty for open network');
                return false;
            }
        } else if (wifiMode && wifiMode.value === '1') {
            // Client mode validation - check primary network
            const primarySsid = document.getElementById('wifi_network_ssid_0');

            if (primarySsid && primarySsid.value.trim() === '') {
                alert('Primary network SSID cannot be empty in Client mode');
                return false;
            }

            // Validate all network passwords
            for (let i = 0; i < 5; i++) {
                const password = document.getElementById(`wifi_network_pass_${i}`);
                if (password && password.value.length > 0 && password.value.length < 8) {
                    alert(`Network ${i + 1} password must be at least 8 characters or empty for open network`);
                    return false;
                }
            }
        }
        
        return true;
    },
    
    async toggleConfigBackup() {
        const el = document.getElementById('configBackupContent');
        if (el && el.dataset.fragment) {
            await Utils.loadFragment(el);
        }
        Utils.toggleElement('configBackupContent', 'configArrow');
    },
    
    handleConfigImport(input) {
        const file = input.files[0];
        if (!file) {
            return;
        }
        
        // Validate file type
        if (!file.name.toLowerCase().endsWith('.json')) {
            alert('Please select a JSON configuration file');
            input.value = '';
            return;
        }
        
        // Show progress
        const progressDiv = document.getElementById('importProgress');
        const progressBar = document.getElementById('progressBar');
        const statusText = document.getElementById('importStatus');
        
        if (progressDiv && progressBar && statusText) {
            progressDiv.style.display = 'block';
            progressBar.style.width = '0%';
            statusText.textContent = 'Uploading configuration...';
        }
        
        // Create FormData and submit
        const formData = new FormData();
        formData.append('config', file);
        
        // Submit with progress indication
        fetch('/config/import', {
            method: 'POST',
            body: formData
        })
        .then(response => {
            if (progressBar) progressBar.style.width = '100%';
            
            if (response.ok) {
                return response.json();
            } else {
                throw new Error('Import failed');
            }
        })
        .then(data => {
            if (data.status === 'ok') {
                if (statusText) {
                    statusText.textContent = data.message;
                    statusText.style.color = 'green';
                }
                
                // Add instruction after 2 seconds
                setTimeout(() => {
                    if (statusText) {
                        statusText.textContent += ' Please refresh the page manually.';
                    }
                }, 2000);
            }
        })
        .catch(error => {
            if (statusText) {
                statusText.textContent = 'Import failed: ' + error.message;
                statusText.style.color = 'red';
            }
            if (progressBar) progressBar.style.width = '0%';
            console.error('Config import error:', error);
        });
        
        // Reset file input
        input.value = '';
    },

    factoryReset() {
        if (!confirm('Reset all settings to factory defaults?\n\nThis will erase all configuration including WiFi credentials.\nDevice will reboot after reset.')) {
            return;
        }

        const resetButton = document.querySelector('button[onclick="factoryReset()"]');
        if (resetButton) {
            resetButton.disabled = true;
            resetButton.textContent = 'Resetting...';
        }

        Utils.fetchWithReboot('/config/reset', { method: 'POST' })
            .then(data => {
                if (data.status === 'ok') {
                    alert('Factory reset complete. Device is rebooting...\n\nReconnect to WiFi: ESP-Bridge-xxxx');
                    Utils.startReconnect({ button: resetButton, waitSeconds: 8 });
                }
            })
            .catch(error => {
                if (error.isReboot) {
                    // Device rebooted during reset - expected behavior
                    alert('Factory reset complete. Device is rebooting...\n\nReconnect to WiFi: ESP-Bridge-xxxx');
                    Utils.startReconnect({ button: resetButton, waitSeconds: 8 });
                    return;
                }
                alert('Factory reset failed: ' + error.message);
                console.error('Factory reset error:', error);
                if (resetButton) {
                    resetButton.disabled = false;
                    resetButton.textContent = 'Factory Reset';
                }
            });
    },

    handleFirmwareUpdate() {
        const fileInput = document.getElementById('firmwareFile');
        const file = fileInput.files[0];
        const updateButton = document.getElementById('updateButton');
        const progressDiv = document.getElementById('updateProgress');
        const progressBar = document.getElementById('updateProgressBar');
        const statusText = document.getElementById('updateStatus');

        // Show progress and disable button
        if (progressDiv && progressBar && statusText && updateButton) {
            progressDiv.style.display = 'block';
            progressBar.style.width = '0%';
            statusText.textContent = 'Uploading firmware...';
            statusText.style.color = 'black';
            updateButton.disabled = true;
            updateButton.textContent = 'Updating...';
        }
        
        // Create FormData and submit
        const formData = new FormData();
        formData.append('update', file);
        
        // Submit with progress tracking
        const xhr = new XMLHttpRequest();
        
        // Track upload progress
        xhr.upload.addEventListener('progress', (event) => {
            if (event.lengthComputable && progressBar) {
                const percentComplete = (event.loaded / event.total) * 100;
                progressBar.style.width = percentComplete + '%';
                
                if (statusText) {
                    statusText.textContent = `Uploading firmware... ${Math.round(percentComplete)}%`;
                }
            }
        });
        
        // Handle response
        xhr.addEventListener('load', () => {
            if (progressBar) progressBar.style.width = '100%';

            // Check content-type to detect device reboot
            const contentType = xhr.getResponseHeader('content-type');
            const isJson = contentType && contentType.includes('application/json');

            // If not JSON, device likely rebooted - show success and start reconnect
            if (!isJson || xhr.responseText.startsWith('<')) {
                if (statusText) {
                    statusText.textContent = 'Firmware uploaded! Device is rebooting...';
                    statusText.style.color = 'green';
                }
                if (updateButton) {
                    updateButton.textContent = 'Rebooting...';
                }
                // Start reconnect countdown
                this.startFirmwareReconnect(updateButton, statusText);
                return;
            }

            try {
                const response = JSON.parse(xhr.responseText);

                if (xhr.status === 200 && response.status === 'ok') {
                    if (statusText) {
                        statusText.textContent = response.message;
                        statusText.style.color = 'green';
                    }
                    if (updateButton) {
                        updateButton.textContent = 'Rebooting...';
                    }
                    // Start reconnect countdown
                    this.startFirmwareReconnect(updateButton, statusText);
                } else {
                    throw new Error(response.message || 'Update failed');
                }
            } catch (error) {
                if (statusText) {
                    statusText.textContent = 'Update failed: ' + error.message;
                    statusText.style.color = 'red';
                }
                if (progressBar) progressBar.style.width = '0%';
                if (updateButton) {
                    updateButton.disabled = false;
                    updateButton.textContent = 'Update Firmware';
                }
            }
        });
        
        // Handle network errors
        xhr.addEventListener('error', () => {
            if (statusText) {
                statusText.textContent = 'Update failed: Network error';
                statusText.style.color = 'red';
            }
            if (progressBar) progressBar.style.width = '0%';
            if (updateButton) {
                updateButton.disabled = false;
                updateButton.textContent = 'Update Firmware';
            }
        });
        
        xhr.open('POST', '/update');
        xhr.send(formData);
    },
    
    updateMavlinkRoutingVisibility() {
        const protocolOpt = document.getElementById('protocol_optimization');
        const routingSection = document.getElementById('mavlink-routing-section');
        const routingDesc = document.getElementById('mavlink-routing-desc');
        
        if (protocolOpt && routingSection) {
            // Show routing options only for MAVLink protocol (value = 1)
            if (protocolOpt.value === '1') {
                routingSection.style.display = 'block';
                if (routingDesc) routingDesc.style.display = 'block';
            } else {
                routingSection.style.display = 'none';
                if (routingDesc) routingDesc.style.display = 'none';
            }
        }
    },

    updateSbusTimingKeeperVisibility() {
        const sbusSection = document.getElementById('sbus-timing-keeper-section');

        if (sbusSection) {
            // Show section ONLY for Device4 SBUS UDP RX (role 4)
            // Timing Keeper repeats last frame when WiFi lags to keep output stable
            // NOT needed for UART SBUS outputs (Device2/3) - they have no WiFi jitter
            const hasSbusUdpRx = (this.config.device4Role === '4');

            if (hasSbusUdpRx) {
                sbusSection.style.display = 'block';
            } else {
                sbusSection.style.display = 'none';
            }
        }
    },

    // Convert composite role values (e.g., "5_1") to separate role and format fields
    // Device 3: "5_0" -> device3_role=5, device3_sbus_format=0
    // Device 4: "3_1" -> device4_role=3, device4_sbus_format=1
    convertCompositeRoles(formData) {
        // Device 3
        const d3Role = formData.get('device3_role');
        if (d3Role && d3Role.includes('_')) {
            const [role, format] = d3Role.split('_');
            formData.set('device3_role', role);
            formData.set('device3_sbus_format', format);
        }

        // Device 4
        const d4Role = formData.get('device4_role');
        if (d4Role && d4Role.includes('_')) {
            const [role, format] = d4Role.split('_');
            formData.set('device4_role', role);
            formData.set('device4_sbus_format', format);
        }
    }
};