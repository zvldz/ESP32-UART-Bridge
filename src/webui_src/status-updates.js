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
                <tr><td><strong>Version:</strong></td><td>v${this.config.version} / ${this.config.arduinoVersion} / ${this.config.idfVersion}</td></tr>
                <tr><td><strong>Board:</strong></td><td>${(this.config.boardType || 's3zero') === 's3supermini' ? 'ESP32-S3 Super Mini' : 'ESP32-S3-Zero'}</td></tr>
                <tr><td><strong>Uptime:</strong></td><td id="uptime">${this.config.uptime} seconds</td></tr>
                <tr><td><strong>Free RAM:</strong></td><td id="freeRam">${Utils.formatBytes(this.config.freeRam)}</td></tr>
                <tr><td><strong>UART Config:</strong></td><td>${this.config.uartConfig}, ${this.config.flowControl === 'Disabled' ? 'No FC' : 'RTS/CTS'}</td></tr>
                ${wifiInfo}
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
        const protocolType = stats.protocolType || 0;
        
        // Protocol name mapping
        const PROTOCOL_NAMES = {
            0: 'RAW',
            1: 'MAVLink',
            2: 'SBUS'
        };
        
        const protocolName = PROTOCOL_NAMES[protocolType] || 'Unknown';
        
        // Build HTML based on protocol type
        let html = `<h4 style="margin: 0 0 15px 0; color: #333;">${protocolName} Protocol</h4>`;
        
        // Protocol-specific statistics
        if (protocolType === 0) {
            // RAW protocol statistics
            html += this.renderRawStats(stats);
        } else if (protocolType === 1) {
            // MAVLink protocol statistics
            html += this.renderMavlinkStats(stats);
        } else if (protocolType === 2) {
            // SBUS protocol statistics
            html += this.renderSbusStats(stats);
        } else {
            // Future protocols (CRSF, etc.)
            html += this.renderGenericStats(stats);
        }
        
        // Sender statistics (common for all protocols)
        if (stats.senders && stats.senders.length > 0) {
            html += this.renderSenderStats(stats.senders, protocolType);
        }
        
        // Check for UDP batching stats
        if (stats.udpBatching) {
            if (stats.udpBatching.batching) {
                const status = stats.udpBatching.totalBatches > 0 ? 
                    `${stats.udpBatching.avgPacketsPerBatch} pkts/batch avg, ${stats.udpBatching.maxPacketsInBatch} max, ${stats.udpBatching.batchEfficiency} efficiency` :
                    'Ready (no MAVLink traffic yet)';
                    
                html += `
                    <div style="margin-top: 10px; padding: 10px; background: #e8f5e8; border-radius: 4px;">
                        <strong>UDP Batching:</strong> <span style="color: #2e7d2e;">ENABLED</span> - ${status}
                    </div>
                `;
            } else {
                html += `
                    <div style="margin-top: 10px; padding: 10px; background: #f8f9fa; border-radius: 4px;">
                        <strong>UDP Batching:</strong> <span style="color: #6c757d;">DISABLED</span> - Single packet per datagram
                    </div>
                `;
            }
        }
        
        // Footer note
        html += `
            <div style="padding: 8px; background: #f8f9fa; border-radius: 4px; font-size: 13px; color: #666; margin-top: 15px;">
                ℹ️ Protocol detection applies to Device 1↔2 data flow. Reset statistics to clear counters.
            </div>
        `;
        
        protocolStatsContent.innerHTML = html;
    },

    renderRawStats(stats) {
        const p = stats.parser || {};
        const b = stats.buffer || {};
        
        return `
            <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 15px; margin-bottom: 15px;">
                <div style="padding: 10px; border: 1px solid #ddd; border-radius: 4px;">
                    <h5 style="margin: 0 0 10px 0; color: #333;">Data Processing</h5>
                    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px; font-size: 14px;">
                        <div>Chunks Created:</div><div><strong>${p.chunksCreated || 0}</strong></div>
                        <div>Bytes Processed:</div><div><strong>${Utils.formatBytes(p.bytesProcessed || 0)}</strong></div>
                        <div>Average Chunk:</div><div><strong>${p.avgPacketSize || 0} bytes</strong></div>
                        <div>Size Range:</div><div><strong>${p.minPacketSize || 0}-${p.maxPacketSize || 0}B</strong></div>
                    </div>
                </div>
                
                <div style="padding: 10px; border: 1px solid #ddd; border-radius: 4px;">
                    <h5 style="margin: 0 0 10px 0; color: #333;">Buffer Status</h5>
                    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px; font-size: 14px;">
                        <div>Buffer Usage:</div><div><strong>${b.used || 0}/${b.capacity || 0} bytes</strong></div>
                        <div>Utilization:</div><div><strong>${b.utilizationPercent || 0}%</strong></div>
                        <div>Last Activity:</div><div><strong>${this.formatLastActivity(p.lastActivityMs)}</strong></div>
                    </div>
                </div>
            </div>
        `;
    },

    renderMavlinkStats(stats) {
        const p = stats.parser || {};
        
        return `
            <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 15px; margin-bottom: 15px;">
                <div style="padding: 10px; border: 1px solid #ddd; border-radius: 4px;">
                    <h5 style="margin: 0 0 10px 0; color: #333;">Packet Statistics</h5>
                    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px; font-size: 14px;">
                        <div>Parsed:</div><div><strong>${p.packetsParsed || 0}</strong></div>
                        <div>Sent:</div><div><strong>${p.packetsSent || 0}</strong></div>
                        <div>Dropped:</div><div><strong>${p.packetsDropped || 0}</strong></div>
                        <div>Errors:</div><div><strong>${p.detectionErrors || 0}</strong></div>
                    </div>
                </div>
                
                <div style="padding: 10px; border: 1px solid #ddd; border-radius: 4px;">
                    <h5 style="margin: 0 0 10px 0; color: #333;">Packet Analysis</h5>
                    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px; font-size: 14px;">
                        <div>Average Size:</div><div><strong>${p.avgPacketSize || 0} bytes</strong></div>
                        <div>Size Range:</div><div><strong>${p.minPacketSize || 0}-${p.maxPacketSize || 0}B</strong></div>
                        <div>Last Activity:</div><div><strong>${this.formatLastActivity(p.lastActivityMs)}</strong></div>
                    </div>
                </div>
            </div>
        `;
    },

    renderSbusStats(stats) {
        const p = stats.parser || {};
        
        // Calculate quality metrics
        const totalFrames = (p.validFrames || 0) + (p.invalidFrames || 0);
        const validPercent = totalFrames > 0 ? ((p.validFrames || 0) / totalFrames * 100).toFixed(1) : '0.0';
        const frameLostPercent = (p.validFrames || 0) > 0 ? ((p.frameLostCount || 0) / (p.validFrames || 0) * 100).toFixed(2) : '0.00';
        const failsafePercent = (p.validFrames || 0) > 0 ? ((p.failsafeCount || 0) / (p.validFrames || 0) * 100).toFixed(2) : '0.00';
        
        return `
            <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 15px; margin-bottom: 15px;">
                <div style="padding: 10px; border: 1px solid #ddd; border-radius: 4px;">
                    <h5 style="margin: 0 0 10px 0; color: #333;">Frame Statistics</h5>
                    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px; font-size: 14px;">
                        <div>Frames Detected:</div><div><strong>${p.framesDetected || 0}</strong></div>
                        <div>Valid Frames:</div><div><strong style="color: #28a745;">${p.validFrames || 0}</strong></div>
                        <div>Invalid Frames:</div><div><strong style="color: #dc3545;">${p.invalidFrames || 0}</strong></div>
                        <div>Success Rate:</div><div><strong>${validPercent}%</strong></div>
                    </div>
                </div>
                
                <div style="padding: 10px; border: 1px solid #ddd; border-radius: 4px;">
                    <h5 style="margin: 0 0 10px 0; color: #333;">SBUS Signal Quality</h5>
                    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px; font-size: 14px;">
                        <div>Frame Lost:</div><div><strong style="color: ${(p.frameLostCount || 0) > 0 ? '#dc3545' : '#28a745'}">${p.frameLostCount || 0}</strong></div>
                        <div>Lost Rate:</div><div><strong>${frameLostPercent}%</strong></div>
                        <div>Failsafe:</div><div><strong style="color: ${(p.failsafeCount || 0) > 0 ? '#dc3545' : '#28a745'}">${p.failsafeCount || 0}</strong></div>
                        <div>Failsafe Rate:</div><div><strong>${failsafePercent}%</strong></div>
                    </div>
                </div>
                
                <div style="padding: 10px; border: 1px solid #ddd; border-radius: 4px;">
                    <h5 style="margin: 0 0 10px 0; color: #333;">Protocol Info</h5>
                    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px; font-size: 14px;">
                        <div>Frame Size:</div><div><strong>25 bytes</strong></div>
                        <div>Baud Rate:</div><div><strong>100,000</strong></div>
                        <div>Format:</div><div><strong>8E2 Inverted</strong></div>
                        <div>Update Rate:</div><div><strong>14ms (71Hz)</strong></div>
                    </div>
                </div>
                
                <div style="padding: 10px; border: 1px solid #ddd; border-radius: 4px;">
                    <h5 style="margin: 0 0 10px 0; color: #333;">Activity</h5>
                    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px; font-size: 14px;">
                        <div>Detection Errors:</div><div><strong style="color: ${(p.framingErrors || 0) > 0 ? '#dc3545' : '#28a745'}">${p.framingErrors || 0}</strong></div>
                        <div>Avg Frame Size:</div><div><strong>${p.avgPacketSize || 25} bytes</strong></div>
                        <div>Last Activity:</div><div><strong>${this.formatLastActivity(p.lastActivityMs || 0)}</strong></div>
                        <div>Status:</div><div><strong style="color: ${(p.lastActivityMs || 0) < 5000 ? '#28a745' : '#dc3545'}">${(p.lastActivityMs || 0) < 5000 ? 'Active' : 'Inactive'}</strong></div>
                    </div>
                </div>
            </div>
        `;
    },

    renderGenericStats(stats) {
        // Fallback for unknown protocols
        const p = stats.parser || {};
        
        return `
            <div style="padding: 10px; border: 1px solid #ddd; border-radius: 4px;">
                <h5 style="margin: 0 0 10px 0; color: #333;">Protocol Statistics</h5>
                <div style="font-size: 14px;">
                    <div>Packets: <strong>${p.packetsTransmitted || 0}</strong></div>
                    <div>Bytes: <strong>${Utils.formatBytes(p.bytesProcessed || 0)}</strong></div>
                </div>
            </div>
        `;
    },

    renderSenderStats(senders, protocolType) {
        if (!senders || senders.length === 0) return '';
        
        let html = `
            <div style="margin-top: 20px;">
                <h5 style="margin: 0 0 10px 0; color: #333;">Output Devices</h5>
                <table style="width: 100%; border-collapse: collapse;">
                    <thead>
                        <tr style="background: #f0f0f0;">
                            <th style="text-align: left; padding: 8px;">Device</th>
                            <th style="text-align: left; padding: 8px;">Sent</th>
                            <th style="text-align: left; padding: 8px;">Dropped</th>
                            <th style="text-align: left; padding: 8px;">Queue</th>
                        </tr>
                    </thead>
                    <tbody>
        `;
        
        senders.forEach(s => {
            const dropRate = s.sent > 0 ? ((s.dropped / (s.sent + s.dropped)) * 100).toFixed(1) : 0;
            
            // Build dropped string with percentage
            let droppedStr = `${s.dropped}`;
            if (s.dropped > 0) {
                // Show percentage for all protocols
                droppedStr += ` (${dropRate}%)`;
            }
            
            html += `
                <tr>
                    <td style="padding: 8px; border-top: 1px solid #ddd;"><strong>${s.name}</strong></td>
                    <td style="padding: 8px; border-top: 1px solid #ddd;">${s.sent}</td>
                    <td style="padding: 8px; border-top: 1px solid #ddd;">${droppedStr}</td>
                    <td style="padding: 8px; border-top: 1px solid #ddd;">${s.queueDepth}/${s.maxQueueDepth}</td>
                </tr>
            `;
        });
        
        html += `
                    </tbody>
                </table>
            </div>
        `;
        
        return html;
    },

    formatLastActivity(ms) {
        if (!ms || ms === 0) return 'just now';
        if (ms < 1000) return `${ms}ms ago`;
        if (ms < 60000) return `${Math.floor(ms/1000)}s ago`;
        return `${Math.floor(ms/60000)}m ago`;
    }
};

// Global functions for onclick handlers
function copyLogs() {
    StatusUpdates.copyLogs();
}