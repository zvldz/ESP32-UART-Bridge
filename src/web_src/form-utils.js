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
    },
    
    handleSubmit(e) {
        // Show loading state on submit button
        const submitButton = e.target.querySelector('button[type="submit"]');
        if (submitButton) {
            submitButton.disabled = true;
            submitButton.textContent = 'Saving...';
        }
        
        // Form will submit normally
        return true;
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
    }
};