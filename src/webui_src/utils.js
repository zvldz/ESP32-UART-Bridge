// ESP32 UART Bridge - Common Utilities

const Utils = {
    // Safe fetch with error handling
    safeFetch(url, callback, errorCallback) {
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
    },

    // Format bytes to human readable format
    formatBytes(bytes, decimals = 0) {
        if (!bytes || bytes === 0) return '0';
        const k = 1024;
        const sizes = ['B', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(decimals)) + sizes[i];
    },

    // Format uptime from seconds
    formatUptime(seconds) {
        if (!seconds || seconds === 0) return '0s';

        const d = Math.floor(seconds / 86400);
        const h = Math.floor((seconds % 86400) / 3600);
        const m = Math.floor((seconds % 3600) / 60);
        const s = seconds % 60;

        if (d > 0) return `${d}d ${h}h`;
        if (h > 0) return `${h}h ${m}m`;
        if (m > 0) return `${m}m ${s}s`;
        return `${s}s`;
    },

    // Fetch with reboot handling - for endpoints that trigger device restart
    // Returns Promise that resolves with JSON data or starts reconnect on reboot
    fetchWithReboot(url, options = {}) {
        return fetch(url, options)
            .then(response => {
                const contentType = response.headers.get('content-type');
                if (!contentType || !contentType.includes('application/json')) {
                    // Device likely rebooting - throw special error
                    const err = new Error('DEVICE_REBOOTING');
                    err.isReboot = true;
                    throw err;
                }
                if (!response.ok) {
                    return response.json().then(data => {
                        throw new Error(data.message || 'Server error');
                    });
                }
                return response.json();
            });
    },

    // Start reconnect countdown and attempts
    startReconnect(options = {}) {
        const {
            waitSeconds = 8,
            maxAttempts = 30,
            button = null,
            statusText = null,
            onConnected = () => window.location.reload()
        } = options;

        let countdown = waitSeconds;

        const countdownInterval = setInterval(() => {
            if (button) {
                button.disabled = true;
                button.textContent = `Rebooting... ${countdown}s`;
                button.style.backgroundColor = '#2196F3';
            }
            if (statusText) {
                statusText.textContent = `Device rebooting... ${countdown}s`;
            }
            countdown--;

            if (countdown < 0) {
                clearInterval(countdownInterval);
                this.attemptReconnect({ maxAttempts, button, statusText, onConnected, attempt: 0 });
            }
        }, 1000);
    },

    attemptReconnect(options) {
        const { maxAttempts, button, statusText, onConnected, attempt } = options;

        if (attempt >= maxAttempts) {
            if (button) {
                button.textContent = 'Please refresh manually';
                button.style.backgroundColor = '#ff9800';
                button.disabled = false;
                button.onclick = () => window.location.reload();
            }
            if (statusText) {
                statusText.textContent = 'Please refresh page manually';
                statusText.style.color = '#ff9800';
            }
            return;
        }

        if (button) {
            button.textContent = `Reconnecting... (${attempt + 1}/${maxAttempts})`;
        }
        if (statusText) {
            statusText.textContent = `Reconnecting... (${attempt + 1}/${maxAttempts})`;
        }

        fetch('/api/status', { cache: 'no-store' })
            .then(response => {
                if (response.ok) {
                    if (button) {
                        button.textContent = 'Connected! Reloading...';
                        button.style.backgroundColor = '#4CAF50';
                    }
                    if (statusText) {
                        statusText.textContent = 'Connected! Reloading...';
                        statusText.style.color = 'green';
                    }
                    setTimeout(onConnected, 500);
                } else {
                    throw new Error('Not ready');
                }
            })
            .catch(() => {
                setTimeout(() => {
                    this.attemptReconnect({ ...options, attempt: attempt + 1 });
                }, 1000);
            });
    }
};
