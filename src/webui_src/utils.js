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

    // Start reconnect with countdown-probe rounds
    // Without targetUrl: simple probe current origin (legacy behavior)
    // With targetUrl: probe mDNS + current origin in parallel, first wins
    startReconnect(options = {}) {
        const {
            retryDelay = 5,
            maxAttempts = 3,
            targetUrl = null,
            button = null,
            statusText = null
        } = options;

        this._reconnectRound({
            retryDelay, maxAttempts, targetUrl,
            fallbackUrl: window.location.origin,
            button, statusText, attempt: 0
        });
    },

    // Single round: countdown → probe → success or next round
    _reconnectRound(options) {
        const { retryDelay, maxAttempts, targetUrl, fallbackUrl, button, statusText, attempt } = options;

        if (attempt >= maxAttempts) {
            if (targetUrl) {
                this._showFallbackLinks(button, statusText, targetUrl, fallbackUrl);
            } else {
                // Legacy: no targetUrl, just show refresh button
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
            }
            return;
        }

        // Countdown phase
        let countdown = retryDelay;
        const label = attempt === 0 ? 'Rebooting' : 'Retrying';

        const interval = setInterval(() => {
            if (button) {
                button.disabled = true;
                button.textContent = `${label}... ${countdown}s`;
                button.style.backgroundColor = '#2196F3';
            }
            if (statusText) {
                statusText.textContent = `${label}... ${countdown}s`;
            }
            countdown--;

            if (countdown < 0) {
                clearInterval(interval);

                // Probe phase
                if (button) button.textContent = `Connecting... (${attempt + 1}/${maxAttempts})`;
                if (statusText) statusText.textContent = `Connecting... (${attempt + 1}/${maxAttempts})`;

                this._probeUrls(targetUrl, fallbackUrl)
                    .then(winnerUrl => {
                        if (button) {
                            button.textContent = 'Connected! Reloading...';
                            button.style.backgroundColor = '#4CAF50';
                        }
                        if (statusText) {
                            statusText.textContent = 'Connected! Reloading...';
                            statusText.style.color = 'green';
                        }
                        setTimeout(() => { window.location.href = winnerUrl; }, 500);
                    })
                    .catch(() => {
                        this._reconnectRound({ ...options, attempt: attempt + 1 });
                    });
            }
        }, 1000);
    },

    // Probe target and fallback URLs, return first successful
    _probeUrls(targetUrl, fallbackUrl) {
        const probes = [];

        if (targetUrl) {
            // Cross-origin probe via no-cors (opaque response = server alive)
            probes.push(
                fetch(targetUrl + 'api/status', { cache: 'no-store', mode: 'no-cors' })
                    .then(() => targetUrl)
            );
        }

        // Fallback probe: skip if fallback is on .local and targetUrl is set
        // (old .local name stays cached briefly after hostname change, would win race)
        const fallbackIsLocal = fallbackUrl.includes('.local');
        if (!targetUrl || !fallbackIsLocal) {
            probes.push(
                fetch(fallbackUrl + '/api/status', { cache: 'no-store' })
                    .then(r => { if (!r.ok) throw new Error('Not ready'); return fallbackUrl + '/'; })
            );
        }

        return Promise.any(probes);
    },

    _showFallbackLinks(button, statusText, targetUrl, fallbackUrl) {
        const links = [];
        if (targetUrl) links.push(targetUrl);
        if (fallbackUrl) links.push(fallbackUrl + '/');
        links.push('http://192.168.4.1/');

        // Remove duplicates
        const unique = [...new Set(links)];

        if (button) button.style.display = 'none';
        if (statusText) {
            statusText.innerHTML =
                '<span style="color:#ff9800">Could not reconnect automatically.</span><br>' +
                '<span style="font-size:13px">Try these links:</span><br>' +
                unique.map(url =>
                    `<a href="${url}" style="color:#2196F3; font-size:13px;">${url}</a>`
                ).join('<br>');
        }
    }
};
