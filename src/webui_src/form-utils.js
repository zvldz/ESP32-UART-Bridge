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
        
        // WiFi settings
        const ssid = document.getElementById('ssid');
        const password = document.getElementById('password');
        if (ssid) ssid.value = this.config.ssid;
        if (password) password.value = this.config.password;
        
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
        
        // Password toggle - attach to global function
        window.togglePassword = () => this.togglePassword();
        
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
    },
    
    handleSubmit(e) {
        e.preventDefault(); // Prevent normal form submission
        
        const form = e.target;
        const submitButton = document.getElementById('saveButton');
        
        // Show loading state on submit button
        if (submitButton) {
            submitButton.disabled = true;
            submitButton.textContent = 'Saving...';
        }
        
        // Submit via fetch to handle JSON response
        const formData = new FormData(form);
        
        fetch('/save', {
            method: 'POST',
            body: formData
        })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'ok') {
                if (submitButton) {
                    submitButton.disabled = false;
                    submitButton.textContent = data.message;
                    submitButton.style.backgroundColor = '#4CAF50';
                }
                
                // Add instruction after 2 seconds
                setTimeout(() => {
                    if (submitButton) {
                        submitButton.textContent += ' Please refresh the page manually.';
                    }
                }, 2000);
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
            }
        })
        .catch(error => {
            console.error('Save error:', error);
            if (submitButton) {
                submitButton.textContent = 'Save Failed';
                submitButton.disabled = false;
            }
            // Reset button after a moment
            setTimeout(() => {
                if (submitButton) {
                    submitButton.textContent = 'Save & Reboot';
                }
            }, 2000);
        });
        
        return false;
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
    
    toggleWifiConfig() {
        Utils.toggleElement('wifiConfigContent', 'wifiArrow');
    },
    
    toggleFirmwareUpdate() {
        Utils.toggleElement('firmwareUpdateContent', 'firmwareArrow');
    },
    
    // Validate form before submission
    validateForm() {
        const ssid = document.getElementById('ssid');
        const password = document.getElementById('password');
        
        if (ssid && ssid.value.trim() === '') {
            alert('WiFi SSID cannot be empty');
            return false;
        }
        
        if (password && password.value.length > 0 && password.value.length < 8) {
            alert('WiFi password must be at least 8 characters or empty for open network');
            return false;
        }
        
        return true;
    },
    
    toggleConfigBackup() {
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
    
    handleFirmwareUpdate() {
        const fileInput = document.getElementById('firmwareFile');
        const updateButton = document.getElementById('updateButton');
        const progressDiv = document.getElementById('updateProgress');
        const progressBar = document.getElementById('updateProgressBar');
        const statusText = document.getElementById('updateStatus');
        
        if (!fileInput.files[0]) {
            alert('Please select a firmware file (.bin)');
            return;
        }
        
        const file = fileInput.files[0];
        
        // Validate file type
        if (!file.name.toLowerCase().endsWith('.bin')) {
            alert('Please select a .bin firmware file');
            return;
        }
        
        // Confirm update
        if (!confirm('Are you sure you want to update firmware? This will restart the device.')) {
            return;
        }
        
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
            
            try {
                const response = JSON.parse(xhr.responseText);
                
                if (xhr.status === 200 && response.status === 'ok') {
                    if (statusText) {
                        statusText.textContent = response.message;
                        statusText.style.color = 'green';
                    }
                    if (updateButton) {
                        updateButton.disabled = false;
                    }
                    
                    // Add instruction after 2 seconds
                    setTimeout(() => {
                        if (statusText) {
                            statusText.textContent += ' Please refresh the page manually.';
                        }
                    }, 2000);
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
    }
};