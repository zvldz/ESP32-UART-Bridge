// ESP32 UART Bridge - Status Updates Module

const StatusUpdates = {
    config: null,
    elements: null,
    
    // Track previous values for activity indicators
    previousValues: {
        device1Rx: 0, device1Tx: 0,
        device2Rx: 0, device2Tx: 0, 
        device3Rx: 0, device3Tx: 0,
        device4RxBytes: 0, device4TxBytes: 0, 
        device4RxPackets: 0, device4TxPackets: 0
    },
    
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
            
            // Device traffic elements (new format)
            device1Traffic: document.getElementById('device1Traffic'),
            device2Traffic: document.getElementById('device2Traffic'),
            device3Traffic: document.getElementById('device3Traffic'),
            device4Traffic: document.getElementById('device4Traffic'),
            
            // Device stats containers
            device2Stats: document.getElementById('device2Stats'),
            device3Stats: document.getElementById('device3Stats'),
            device4Stats: document.getElementById('device4Stats'),
            
            // Device roles
            device2Role: document.getElementById('device2Role'),
            device3Role: document.getElementById('device3Role'),
            device4Role: document.getElementById('device4Role'),
            
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
        
        // Build WiFi info based on mode
        let wifiInfo = '';
        if (this.config.wifiMode === 0) {  // AP Mode
            wifiInfo = `<tr><td><strong>WiFi Mode:</strong></td><td>Access Point (${this.config.ssid})</td></tr>`;
        } else {  // Client Mode
            if (this.config.wifiClientConnected) {
                const rssiPercent = this.config.rssiPercent || 0;
                const wifiSignal = Utils.getWifiSignalBars(rssiPercent);
                wifiInfo = `<tr><td><strong>WiFi Mode:</strong></td><td>Client (${this.config.wifiClientSsid} ${wifiSignal} ${rssiPercent}%)</td></tr>`;
                wifiInfo += `<tr><td><strong>IP Address:</strong></td><td>${this.config.ipAddress || 'N/A'}</td></tr>`;
            } else {
                wifiInfo = `<tr><td><strong>WiFi Mode:</strong></td><td>Client (Searching: ${this.config.wifiClientSsid})</td></tr>`;
            }
        }
        
        container.innerHTML = `
            <table>
                <tr><td><strong>Device:</strong></td><td>${this.config.deviceName} v${this.config.version}</td></tr>
                <tr><td><strong>Free RAM:</strong></td><td id="freeRam">${Utils.formatBytes(this.config.freeRam)}</td></tr>
                <tr><td><strong>Uptime:</strong></td><td id="uptime">${this.config.uptime} seconds</td></tr>
                ${wifiInfo}
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
                    <td id="device1Traffic">${Utils.formatTraffic(this.config.device1Rx || 0, this.config.device1Tx || 0)}</td>
                </tr>
                <tr id="device2Stats" style="display: none;">
                    <td><strong>Device 2 (<span id="device2Role">${this.config.device2RoleName}</span>):</strong></td>
                    <td id="device2Traffic">${Utils.formatTraffic(this.config.device2Rx || 0, this.config.device2Tx || 0)}</td>
                </tr>
                <tr id="device3Stats" style="display: none;">
                    <td><strong>Device 3 (<span id="device3Role">${this.config.device3RoleName}</span>):</strong></td>
                    <td id="device3Traffic">${Utils.formatTraffic(this.config.device3Rx || 0, this.config.device3Tx || 0)}</td>
                </tr>
                <tr id="device4Stats" style="display: none;">
                    <td><strong>Device 4 (<span id="device4Role">-</span>):</strong></td>
                    <td id="device4Traffic">-</td>
                </tr>
                <tr><td colspan="2" style="border-top: 1px solid #ddd; padding-top: 5px;"></td></tr>
                <tr><td><strong>Total Traffic:</strong></td><td id="totalTraffic">${Utils.formatBytes(this.config.totalTraffic || 0)}</td></tr>
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
                this.elements.freeRam.textContent = Utils.formatBytes(data.freeRam);
            }
            if (this.elements.uptime) {
                this.elements.uptime.textContent = `${data.uptime} seconds`;
            }
            
            // Update Device 1 traffic (always visible)
            if (this.elements.device1Traffic) {
                const formattedTraffic = Utils.formatTraffic(
                    data.device1Rx || 0, 
                    data.device1Tx || 0,
                    this.previousValues.device1Rx,
                    this.previousValues.device1Tx
                );
                this.elements.device1Traffic.innerHTML = formattedTraffic;
                this.previousValues.device1Rx = data.device1Rx || 0;
                this.previousValues.device1Tx = data.device1Tx || 0;
            }
            
            // Update Device 2 stats if present
            if (data.device2Role && this.elements.device2Stats) {
                this.elements.device2Stats.style.display = 'table-row';
                if (this.elements.device2Role) {
                    this.elements.device2Role.textContent = data.device2RoleName;
                }
                if (this.elements.device2Traffic) {
                    const formattedTraffic = Utils.formatTraffic(
                        data.device2Rx || 0, 
                        data.device2Tx || 0,
                        this.previousValues.device2Rx,
                        this.previousValues.device2Tx
                    );
                    this.elements.device2Traffic.innerHTML = formattedTraffic;
                    this.previousValues.device2Rx = data.device2Rx || 0;
                    this.previousValues.device2Tx = data.device2Tx || 0;
                }
            } else if (this.elements.device2Stats) {
                this.elements.device2Stats.style.display = 'none';
            }
            
            // Update Device 3 stats if present
            if (data.device3Role && this.elements.device3Stats) {
                this.elements.device3Stats.style.display = 'table-row';
                if (this.elements.device3Role) {
                    this.elements.device3Role.textContent = data.device3RoleName;
                }
                if (this.elements.device3Traffic) {
                    const formattedTraffic = Utils.formatTraffic(
                        data.device3Rx || 0, 
                        data.device3Tx || 0,
                        this.previousValues.device3Rx,
                        this.previousValues.device3Tx
                    );
                    this.elements.device3Traffic.innerHTML = formattedTraffic;
                    this.previousValues.device3Rx = data.device3Rx || 0;
                    this.previousValues.device3Tx = data.device3Tx || 0;
                }
            } else if (this.elements.device3Stats) {
                this.elements.device3Stats.style.display = 'none';
            }
            
            // Update Device 4 stats if present
            if (data.device4Role && this.elements.device4Stats) {
                this.elements.device4Stats.style.display = 'table-row';
                if (this.elements.device4Role) {
                    this.elements.device4Role.textContent = data.device4RoleName;
                }
                if (this.elements.device4Traffic) {
                    const formattedTraffic = Utils.formatNetworkTraffic(
                        data.device4RxBytes || 0,
                        data.device4TxBytes || 0,
                        data.device4RxPackets || 0,
                        data.device4TxPackets || 0,
                        this.previousValues.device4RxBytes,
                        this.previousValues.device4TxBytes,
                        this.previousValues.device4RxPackets,
                        this.previousValues.device4TxPackets
                    );
                    this.elements.device4Traffic.innerHTML = formattedTraffic;
                    this.previousValues.device4RxBytes = data.device4RxBytes || 0;
                    this.previousValues.device4TxBytes = data.device4TxBytes || 0;
                    this.previousValues.device4RxPackets = data.device4RxPackets || 0;
                    this.previousValues.device4TxPackets = data.device4TxPackets || 0;
                }
            } else if (this.elements.device4Stats) {
                this.elements.device4Stats.style.display = 'none';
            }
            
            // Update totals with smart formatting
            if (this.elements.totalTraffic) {
                this.elements.totalTraffic.textContent = Utils.formatBytes(data.totalTraffic || 0);
            }
            
            // Handle last activity (backend already adds "seconds ago")
            if (this.elements.lastActivity) {
                this.elements.lastActivity.textContent = data.lastActivity || "Never";
            }
            
            // Update protocol statistics
            this.updateProtocolStats(data);
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
        const originalClass = button.className;
        
        button.disabled = true;
        button.textContent = 'Resetting...';
        
        fetch('/reset_stats')
            .then(response => {
                if (!response.ok) {
                    throw new Error('Reset failed');
                }
                return response.json(); // Parse JSON response
            })
            .then(data => {
                // Check if response indicates success
                if (data.status !== 'ok') {
                    throw new Error(data.message || 'Reset failed');
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
                
                // Reset button state properly
                button.disabled = false;
                button.textContent = 'Reset Complete ✓';
                button.style.backgroundColor = '#4CAF50';
                
                // Return to original state after 2 seconds
                setTimeout(() => {
                    button.textContent = originalText;
                    button.style.backgroundColor = '#ff9800';
                    button.className = originalClass;
                }, 2000);
                
                // Force immediate update
                setTimeout(() => {
                    this.updateAll();
                }, 100);
            })
            .catch(error => {
                console.error('Reset error:', error);
                
                // Reset button state on error
                button.disabled = false;
                button.textContent = 'Error!';
                button.style.backgroundColor = '#f44336';
                
                // Return to original state after 3 seconds
                setTimeout(() => {
                    button.textContent = originalText;
                    button.style.backgroundColor = '#ff9800';
                    button.className = originalClass;
                }, 3000);
                
                alert('Failed to reset statistics: ' + error.message);
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
    },
    
    updateProtocolStats(data) {
        const protocolStatsContent = document.getElementById('protocolStatsContent');
        if (!protocolStatsContent) return;
        
        // Check if protocol stats are available
        if (!data.protocolStats) {
            protocolStatsContent.innerHTML = '<p style="color: #666; text-align: center; margin: 20px 0;">No protocol statistics available</p>';
            return;
        }
        
        const stats = data.protocolStats;
        const protocolName = data.protocolOptimization === 1 ? 'MAVLink' : 'Unknown';
        
        protocolStatsContent.innerHTML = `
            <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 15px; margin-bottom: 15px;">
                <div style="padding: 10px; border: 1px solid #ddd; border-radius: 4px;">
                    <h5 style="margin: 0 0 10px 0; color: #333;">Detection Summary</h5>
                    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px; font-size: 14px;">
                        <div>Protocol: <strong>${protocolName}</strong></div>
                        <div>Rate: <strong>${stats.packetsPerSecond}/sec</strong></div>
                        <div>Detected: <strong>${stats.packetsDetected}</strong></div>
                        <div>Transmitted: <strong>${stats.packetsTransmitted}</strong></div>
                        <div>Errors: <strong>${stats.detectionErrors}</strong></div>
                        <div>Resyncs: <strong>${stats.resyncEvents}</strong></div>
                    </div>
                </div>
                
                <div style="padding: 10px; border: 1px solid #ddd; border-radius: 4px;">
                    <h5 style="margin: 0 0 10px 0; color: #333;">Packet Analysis</h5>
                    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px; font-size: 14px;">
                        <div>Total Bytes: <strong>${stats.totalBytes}</strong></div>
                        <div>Average Size: <strong>${stats.avgPacketSize}B</strong></div>
                        <div>Min Size: <strong>${stats.minPacketSize}B</strong></div>
                        <div>Max Size: <strong>${stats.maxPacketSize}B</strong></div>
                        <div>Last Packet: <strong>${stats.lastPacketTime}</strong></div>
                        <div>Max Errors: <strong>${stats.maxConsecutiveErrors}</strong></div>
                    </div>
                </div>
            </div>
            
            <div style="padding: 8px; background: #f8f9fa; border-radius: 4px; font-size: 13px; color: #666;">
                ℹ️ Protocol detection applies to Device 1↔2 data flow only. Statistics reset on device restart or manual reset.
            </div>
        `;
    }
};

// Global functions for onclick handlers
function copyLogs() {
    StatusUpdates.copyLogs();
}