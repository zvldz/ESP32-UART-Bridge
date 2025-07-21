// ESP32 UART Bridge - Status Updates Module

const StatusUpdates = {
    config: null,
    elements: null,
    
    init(config) {
        this.config = config;
        this.elements = this.cacheElements();
        this.buildSystemInfo();
        this.buildDeviceStats();
        this.updateAll();
    },
    
    cacheElements() {
        // Cache frequently accessed elements for better performance
        return {
            // System info
            freeRam: document.getElementById('freeRam'),
            uptime: document.getElementById('uptime'),
            
            // Device 1 stats
            device1Rx: document.getElementById('device1Rx'),
            device1Tx: document.getElementById('device1Tx'),
            
            // Device 2 stats
            device2Stats: document.getElementById('device2Stats'),
            device2Role: document.getElementById('device2Role'),
            device2Rx: document.getElementById('device2Rx'),
            device2Tx: document.getElementById('device2Tx'),
            
            // Device 3 stats
            device3Stats: document.getElementById('device3Stats'),
            device3Role: document.getElementById('device3Role'),
            device3Rx: document.getElementById('device3Rx'),
            device3Tx: document.getElementById('device3Tx'),
            
            // Totals
            totalTraffic: document.getElementById('totalTraffic'),
            lastActivity: document.getElementById('lastActivity'),
            
            // Logs
            logContainer: document.getElementById('logContainer'),
            logEntries: document.getElementById('logEntries')
        };
    },
    
    buildSystemInfo() {
        const container = document.getElementById('systemInfo');
        if (!container) return;
        
        container.innerHTML = `
            <table>
                <tr><td><strong>Device:</strong></td><td>${this.config.deviceName} v${this.config.version}</td></tr>
                <tr><td><strong>Free RAM:</strong></td><td id="freeRam">${this.config.freeRam} bytes</td></tr>
                <tr><td><strong>Uptime:</strong></td><td id="uptime">${this.config.uptime} seconds</td></tr>
                <tr><td><strong>WiFi SSID:</strong></td><td>${this.config.ssid}</td></tr>
                <tr><td><strong>Current UART:</strong></td><td>${this.config.uartConfig}</td></tr>
                <tr><td><strong>Flow Control:</strong></td><td>${this.config.flowControl}</td></tr>
            </table>
        `;
        
        // Re-cache elements after building HTML
        this.elements.freeRam = document.getElementById('freeRam');
        this.elements.uptime = document.getElementById('uptime');
    },
    
    buildDeviceStats() {
        const container = document.getElementById('deviceStats');
        if (!container) return;
        
        container.innerHTML = `
            <table>
                <tr><td colspan="2" style="text-align: center; font-weight: bold; background-color: #f0f0f0;">Device Statistics</td></tr>
                <tr id="device1Stats">
                    <td><strong>Device 1 (UART1):</strong></td>
                    <td>RX: <span id="device1Rx">${this.config.device1Rx}</span> bytes, TX: <span id="device1Tx">${this.config.device1Tx}</span> bytes</td>
                </tr>
                <tr id="device2Stats" style="display: none;">
                    <td><strong>Device 2 (<span id="device2Role">${this.config.device2RoleName}</span>):</strong></td>
                    <td>RX: <span id="device2Rx">${this.config.device2Rx}</span> bytes, TX: <span id="device2Tx">${this.config.device2Tx}</span> bytes</td>
                </tr>
                <tr id="device3Stats" style="display: none;">
                    <td><strong>Device 3 (<span id="device3Role">${this.config.device3RoleName}</span>):</strong></td>
                    <td>RX: <span id="device3Rx">${this.config.device3Rx}</span> bytes, TX: <span id="device3Tx">${this.config.device3Tx}</span> bytes</td>
                </tr>
                <tr><td colspan="2" style="border-top: 1px solid #ddd; padding-top: 5px;"></td></tr>
                <tr><td><strong>Total Traffic:</strong></td><td id="totalTraffic">${this.config.totalTraffic} bytes</td></tr>
                <tr><td><strong>Last Activity:</strong></td><td id="lastActivity">${this.config.lastActivity}</td></tr>
            </table>
            <div style="text-align: center; margin: 15px 0;">
                <button style="background-color: #ff9800;" onclick="StatusUpdates.resetStatistics(this)">Reset Statistics</button>
            </div>
        `;
        
        // Re-cache all device elements
        this.elements = this.cacheElements();
    },
    
    updateAll() {
        this.updateStatus();
        this.updateLogs();
    },
    
    updateStatus() {
        Utils.safeFetch('/status', (data) => {
            // Update system info
            if (this.elements.freeRam) {
                this.elements.freeRam.textContent = `${data.freeRam} bytes`;
            }
            if (this.elements.uptime) {
                this.elements.uptime.textContent = `${data.uptime} seconds`;
            }
            
            // Update Device 1 stats (always visible)
            if (this.elements.device1Rx) {
                this.elements.device1Rx.textContent = data.device1Rx;
            }
            if (this.elements.device1Tx) {
                this.elements.device1Tx.textContent = data.device1Tx;
            }
            
            // Update Device 2 stats if present
            if (data.device2Role && this.elements.device2Stats) {
                this.elements.device2Stats.style.display = 'table-row';
                if (this.elements.device2Role) {
                    this.elements.device2Role.textContent = data.device2Role;
                }
                if (this.elements.device2Rx) {
                    this.elements.device2Rx.textContent = data.device2Rx;
                }
                if (this.elements.device2Tx) {
                    this.elements.device2Tx.textContent = data.device2Tx;
                }
            } else if (this.elements.device2Stats) {
                this.elements.device2Stats.style.display = 'none';
            }
            
            // Update Device 3 stats if present
            if (data.device3Role && this.elements.device3Stats) {
                this.elements.device3Stats.style.display = 'table-row';
                if (this.elements.device3Role) {
                    this.elements.device3Role.textContent = data.device3Role;
                }
                if (this.elements.device3Rx) {
                    this.elements.device3Rx.textContent = data.device3Rx;
                }
                if (this.elements.device3Tx) {
                    this.elements.device3Tx.textContent = data.device3Tx;
                }
            } else if (this.elements.device3Stats) {
                this.elements.device3Stats.style.display = 'none';
            }
            
            // Update totals
            if (this.elements.totalTraffic) {
                this.elements.totalTraffic.textContent = `${data.totalTraffic} bytes`;
            }
            
            // Handle last activity
            if (this.elements.lastActivity) {
                if (data.lastActivity === "Never") {
                    this.elements.lastActivity.textContent = "Never";
                } else {
                    this.elements.lastActivity.textContent = `${data.lastActivity} seconds ago`;
                }
            }
        });
    },
    
    updateLogs() {
        Utils.safeFetch('/logs', (data) => {
            if (!this.elements.logEntries) return;
            
            this.elements.logEntries.innerHTML = '';
            
            if (data.logs && data.logs.length > 0) {
                data.logs.forEach(log => {
                    const entry = document.createElement('div');
                    entry.className = 'log-entry';
                    entry.textContent = log;
                    this.elements.logEntries.appendChild(entry);
                });
            } else {
                const entry = document.createElement('div');
                entry.className = 'log-entry';
                entry.textContent = 'No logs available';
                this.elements.logEntries.appendChild(entry);
            }
            
            // Auto-scroll to bottom
            if (this.elements.logContainer) {
                this.elements.logContainer.scrollTop = this.elements.logContainer.scrollHeight;
            }
        });
    },
    
    resetStatistics(button) {
        if (!button) return;
        
        const originalText = button.textContent;
        
        button.disabled = true;
        button.textContent = 'Resetting...';
        
        fetch('/reset_stats')
            .then(response => {
                if (!response.ok) {
                    throw new Error('Reset failed');
                }
                
                // Update UI immediately
                const updates = {
                    'device1Rx': '0',
                    'device1Tx': '0',
                    'device2Rx': '0',
                    'device2Tx': '0',
                    'device3Rx': '0',
                    'device3Tx': '0',
                    'totalTraffic': '0 bytes',
                    'lastActivity': 'Never'
                };
                
                Utils.updateElements(updates);
                
                // Clear logs
                if (this.elements.logEntries) {
                    this.elements.logEntries.innerHTML = '<div class="log-entry">Statistics and logs cleared</div>';
                }
                
                // Show success feedback
                Utils.showButtonFeedback(button, 'Reset Complete ✓', 'success-button', 2000);
                
                // Force immediate update
                setTimeout(() => {
                    this.updateAll();
                }, 100);
            })
            .catch(error => {
                console.error('Reset error:', error);
                alert('Failed to reset statistics');
                button.disabled = false;
                button.textContent = originalText;
            });
    },
    
    // Copy logs to clipboard
    copyLogs() {
        if (!this.elements.logEntries) return;
        
        const logs = this.elements.logEntries.innerText || this.elements.logEntries.textContent || '';
        if (!logs || logs === 'Loading logs...' || logs === 'No logs available') return;
        
        const textArea = document.createElement('textarea');
        textArea.value = logs;
        textArea.style.position = 'fixed';
        textArea.style.left = '-999999px';
        document.body.appendChild(textArea);
        textArea.select();
        
        try {
            document.execCommand('copy');
            const icon = document.getElementById('copyBtn').querySelector('svg');
            if (icon) {
                const originalStroke = icon.getAttribute('stroke');
                const originalContent = icon.innerHTML;
                
                icon.setAttribute('stroke', '#4CAF50');
                icon.innerHTML = '<polyline points="5 10 8 13 15 6"></polyline>';
                
                setTimeout(() => {
                    icon.setAttribute('stroke', originalStroke);
                    icon.innerHTML = originalContent;
                }, 1500);
            }
        } catch (err) {
            console.error('Copy failed:', err);
        }
        
        document.body.removeChild(textArea);
    }
};

// Global functions for onclick handlers
function copyLogs() {
    StatusUpdates.copyLogs();
}