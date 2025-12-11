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

    // Generate WiFi signal icon based on RSSI percentage
    getWifiIcon(rssiPercent) {
        if (rssiPercent >= 75) return '📶'; // Full signal
        if (rssiPercent >= 50) return '📶'; // Good signal
        if (rssiPercent >= 25) return '📶'; // Fair signal
        return '📶'; // Weak signal
    },

    // Generate WiFi signal bars based on RSSI percentage (mobile style)
    getWifiSignalBars(rssiPercent) {
        if (rssiPercent >= 75) return '<span style="font-family: monospace;">▂▄▆█</span>'; // 4 bars
        if (rssiPercent >= 50) return '<span style="font-family: monospace;">▂▄▆</span>';  // 3 bars  
        if (rssiPercent >= 25) return '<span style="font-family: monospace;">▂▄</span>';   // 2 bars
        return '<span style="font-family: monospace;">▂</span>';                          // 1 bar
    },

    // Update element text content safely
    updateElementText(id, text) {
        const el = document.getElementById(id);
        if (el) el.textContent = text;
    },

    // Update multiple elements at once
    updateElements(updates) {
        Object.entries(updates).forEach(([id, text]) => {
            this.updateElementText(id, text);
        });
    },

    // Toggle element visibility
    toggleElement(contentId, arrowId) {
        const content = document.getElementById(contentId);
        const arrow = document.getElementById(arrowId);

        if (!content) return false;

        if (content.style.display === 'none') {
            content.style.display = 'block';
            if (arrow) arrow.textContent = '▼';
            return true; // opened
        } else {
            content.style.display = 'none';
            if (arrow) arrow.textContent = '▶';
            return false; // closed
        }
    },

    // Toggle element with localStorage persistence
    rememberedToggle(blockId, arrowId, storageKey) {
        const el = document.getElementById(blockId);
        const arrow = document.getElementById(arrowId);
        if (!el) return;

        const nowHidden = el.style.display !== 'none' ? true : false;
        if (nowHidden) {
            el.style.display = 'none';
            if (arrow) arrow.textContent = '▶';
        } else {
            el.style.display = 'block';
            if (arrow) arrow.textContent = '▼';
        }

        try {
            localStorage.setItem(storageKey, el.style.display === 'none' ? 'hidden' : 'shown');
        } catch(e) {
            console.warn('localStorage not available:', e);
        }
    },

    // Restore toggle state from localStorage
    restoreToggle(blockId, arrowId, storageKey) {
        const el = document.getElementById(blockId);
        const arrow = document.getElementById(arrowId);
        if (!el) return;

        let v = null;
        try {
            v = localStorage.getItem(storageKey);
        } catch(e) {
            console.warn('localStorage not available:', e);
        }

        if (v === 'shown') {
            el.style.display = 'block';
            if (arrow) arrow.textContent = '▼';
        } else {
            // Default: collapsed (including when v === null - no saved state)
            el.style.display = 'none';
            if (arrow) arrow.textContent = '▶';
        }
    },

    // Show temporary feedback on button
    showButtonFeedback(button, message, className, duration = 2000) {
        const originalText = button.textContent;
        const originalClass = button.className;
        
        button.disabled = true;
        button.textContent = message;
        if (className) button.className = className;
        
        setTimeout(() => {
            button.disabled = false;
            button.textContent = originalText;
            button.className = originalClass;
        }, duration);
    },

    // Debounce function for input handlers
    debounce(func, wait) {
        let timeout;
        return function executedFunction(...args) {
            const later = () => {
                clearTimeout(timeout);
                func(...args);
            };
            clearTimeout(timeout);
            timeout = setTimeout(later, wait);
        };
    },

    // Parse query parameters
    getQueryParam(param) {
        const urlParams = new URLSearchParams(window.location.search);
        return urlParams.get(param);
    },

    // Safe JSON parse
    parseJSON(str, defaultValue = null) {
        try {
            return JSON.parse(str);
        } catch (e) {
            console.error('JSON parse error:', e);
            return defaultValue;
        }
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

        fetch('/status', { cache: 'no-store' })
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
    },

    // Format traffic statistics with visual indicators and alignment
    formatTraffic(rxBytes, txBytes, rxPrevious = 0, txPrevious = 0) {
        const rxFormatted = this.formatBytes(rxBytes);
        const txFormatted = this.formatBytes(txBytes);
        
        // Detect activity (bytes increased since last update)
        const rxActive = rxBytes > rxPrevious;
        const txActive = txBytes > txPrevious;
        
        // Create colored indicators
        const rxIndicator = rxActive ? '<span style="color: #28a745;">●</span>' : '<span style="color: #6c757d;">○</span>';
        const txIndicator = txActive ? '<span style="color: #28a745;">●</span>' : '<span style="color: #6c757d;">○</span>';
        
        // Create aligned layout using flexbox with proper right alignment
        return `<div style="display: flex; justify-content: space-between; align-items: center; width: 100%;">
            <span>${rxIndicator} ↓ <strong>${rxFormatted}</strong></span>
            <span><strong>${txFormatted}</strong> ↑ ${txIndicator}</span>
        </div>`;
    },

    // Format traffic for Device 4 (Network Bridge/Logger) with RX/TX and packets
    formatNetworkTraffic(rxBytes, txBytes, rxPackets, txPackets, rxBytesPrevious = 0, txBytesPrevious = 0, rxPacketsPrevious = 0, txPacketsPrevious = 0) {
        const rxFormatted = this.formatBytes(rxBytes);
        const txFormatted = this.formatBytes(txBytes);
        
        // Detect activity
        const rxActive = rxBytes > rxBytesPrevious || rxPackets > rxPacketsPrevious;
        const txActive = txBytes > txBytesPrevious || txPackets > txPacketsPrevious;
        
        // Create colored indicators
        const rxIndicator = rxActive ? '<span style="color: #28a745;">●</span>' : '<span style="color: #6c757d;">○</span>';
        const txIndicator = txActive ? '<span style="color: #28a745;">●</span>' : '<span style="color: #6c757d;">○</span>';
        
        // Create compact aligned layout with packet counts and proper right alignment
        return `<div style="display: flex; justify-content: space-between; align-items: center; width: 100%; font-size: 0.9em;">
            <span>${rxIndicator} ↓ <strong>${rxFormatted}</strong> <span style="color: #6c757d; font-size: 0.85em;">${rxPackets}p</span></span>
            <span><strong>${txFormatted}</strong> <span style="color: #6c757d; font-size: 0.85em;">${txPackets}p</span> ↑ ${txIndicator}</span>
        </div>`;
    }
};