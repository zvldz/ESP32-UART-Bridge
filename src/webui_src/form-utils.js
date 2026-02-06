// ESP32 UART Bridge - Form Utilities Module
// Note: Form values and saving migrated to Alpine.js ($store.app)
// This file provides global onclick handlers used in HTML

const FormUtils = {
    // Called from app.js - attach global handlers for onclick in HTML
    attachListeners() {
        window.handleConfigImport = (input) => this.handleConfigImport(input);
        window.handleFirmwareUpdate = () => this.handleFirmwareUpdate();
        window.factoryReset = () => this.factoryReset();
    },

    // Note: togglePassword removed - now pure Alpine with x-data="{ show: false }"

    handleConfigImport(input) {
        const file = input.files[0];
        if (!file) return;

        if (!file.name.toLowerCase().endsWith('.json')) {
            alert('Please select a JSON configuration file');
            input.value = '';
            return;
        }

        const statusText = document.getElementById('importStatus');
        if (statusText) {
            statusText.style.display = 'block';
            statusText.textContent = 'Importing...';
            statusText.style.color = 'black';
        }

        const formData = new FormData();
        formData.append('config', file);

        fetch('/config/import', { method: 'POST', body: formData })
            .then(r => r.ok ? r.json() : Promise.reject(new Error('Import failed')))
            .then(data => {
                if (data.status === 'ok' && statusText) {
                    statusText.textContent = data.message + ' Please refresh the page.';
                    statusText.style.color = 'green';
                }
            })
            .catch(error => {
                if (statusText) {
                    statusText.textContent = 'Import failed: ' + error.message;
                    statusText.style.color = 'red';
                }
            });

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

        // After factory reset, device reboots into AP mode with default hostname
        // Captive portal will auto-open when user reconnects to new AP
        const app = typeof Alpine !== 'undefined' ? Alpine.store('app') : null;
        const apName = app?.defaultHostname || app?.mdnsHostname || 'esp-bridge-XXXX';

        const onReset = () => {
            alert(`Factory reset complete. Device is rebooting...\n\nReconnect to WiFi: ${apName}`);
        };

        Utils.fetchWithReboot('/config/reset', { method: 'POST' })
            .then(data => { if (data.status === 'ok') onReset(); })
            .catch(error => {
                if (error.isReboot) { onReset(); return; }
                alert('Factory reset failed: ' + error.message);
                if (resetButton) {
                    resetButton.disabled = false;
                    resetButton.textContent = 'Factory Reset';
                }
            });
    },

    handleFirmwareUpdate() {
        const fileInput = document.getElementById('firmwareFile');
        const file = fileInput.files[0];
        const store = typeof Alpine !== 'undefined' ? Alpine.store('firmware') : null;

        if (store) store.start();

        const formData = new FormData();
        formData.append('update', file);

        const xhr = new XMLHttpRequest();

        xhr.upload.addEventListener('progress', (event) => {
            if (event.lengthComputable && store) {
                store.setProgress((event.loaded / event.total) * 100);
            }
        });

        xhr.addEventListener('load', () => {
            const contentType = xhr.getResponseHeader('content-type');
            const isJson = contentType && contentType.includes('application/json');

            if (!isJson || xhr.responseText.startsWith('<')) {
                if (store) store.setSuccess('Firmware uploaded! Device is rebooting...');
                this._firmwareReconnect();
                return;
            }

            try {
                const response = JSON.parse(xhr.responseText);
                if (xhr.status === 200 && response.status === 'ok') {
                    if (store) store.setSuccess(response.message);
                    this._firmwareReconnect();
                } else {
                    throw new Error(response.message || 'Update failed');
                }
            } catch (error) {
                if (store) store.setError('Update failed: ' + error.message);
            }
        });

        xhr.addEventListener('error', () => {
            if (store) store.setError('Update failed: Network error');
        });

        xhr.open('POST', '/update');
        xhr.send(formData);
    },

    // Firmware reconnect with longer wait for flash write
    _firmwareReconnect() {
        const store = typeof Alpine !== 'undefined' ? Alpine.store('firmware') : null;
        let countdown = 12;
        let attempt = 0;
        const maxAttempts = 30;

        const countdownInterval = setInterval(() => {
            if (store) store.status = `Device rebooting... ${countdown}s`;
            countdown--;
            if (countdown < 0) {
                clearInterval(countdownInterval);
                tryReconnect();
            }
        }, 1000);

        function tryReconnect() {
            if (attempt >= maxAttempts) {
                if (store) {
                    store.status = 'Please refresh page manually';
                    store.statusColor = '#ff9800';
                }
                return;
            }

            if (store) store.status = `Reconnecting... (${attempt + 1}/${maxAttempts})`;

            fetch('/api/status', { cache: 'no-store' })
                .then(r => {
                    if (r.ok) {
                        if (store) {
                            store.status = 'Connected! Reloading...';
                            store.statusColor = 'green';
                        }
                        setTimeout(() => window.location.reload(), 500);
                    } else {
                        throw new Error('Not ready');
                    }
                })
                .catch(() => {
                    attempt++;
                    setTimeout(tryReconnect, 1000);
                });
        }
    }
};