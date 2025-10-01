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
        this.createSbusSourceControls();
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
            device1Role: document.getElementById('device1Role'),
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
                <tr><td colspan="2" style="text-align: center; font-weight: bold; background-color: #f0f0f0;">Device Info</td></tr>
                <tr><td><strong>Version:</strong></td><td>v${this.config.version} / ${this.config.arduinoVersion} / ${this.config.idfVersion}</td></tr>
                <tr><td><strong>Board:</strong></td><td>${(this.config.boardType || 's3zero') === 's3supermini' ? 'ESP32-S3 Super Mini' : 'ESP32-S3-Zero'}</td></tr>
                <tr><td><strong>Uptime:</strong></td><td id="uptime">${this.config.uptime} seconds</td></tr>
                <tr><td><strong>Free RAM:</strong></td><td id="freeRam">${Utils.formatBytes(this.config.freeRam)}</td></tr>
                <tr><td><strong>UART Config:</strong></td><td>${this.config.uartConfig}, ${this.config.flowControl === 'Disabled' ? '<s>RTS/CTS</s>' : 'RTS/CTS'}</td></tr>
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
                    <td><strong>Device 1 (<span id="device1Role">${this.config.device1RoleName}</span>):</strong></td>
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
            <div id="udpBatchingInfo" style="padding: 8px; background: #f8f9fa; border-radius: 4px; font-size: 13px; color: #666; margin-top: 10px; text-align: center;"></div>
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
            if (this.elements.device1Role) {
                this.elements.device1Role.textContent = data.device1RoleName;
            }
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

            // Update UDP Batching info
            const udpBatchingInfo = document.getElementById('udpBatchingInfo');
            if (udpBatchingInfo) {
                // Check if we have detailed batching stats from protocol statistics
                const batchingStats = data.protocolStats?.udpBatching;

                if (data.udpBatchingEnabled) {
                    if (batchingStats && batchingStats.batching && batchingStats.totalBatches > 0) {
                        // Show detailed stats
                        udpBatchingInfo.innerHTML = `ℹ️ UDP Batching: <span style="color: #2e7d2e;">ENABLED</span> - ${batchingStats.avgPacketsPerBatch} pkts/batch avg, ${batchingStats.maxPacketsInBatch} max, ${batchingStats.batchEfficiency} efficiency`;
                    } else {
                        // Enabled but no traffic yet
                        udpBatchingInfo.innerHTML = 'ℹ️ UDP Batching: <span style="color: #2e7d2e;">ENABLED</span> - Ready (no MAVLink traffic yet)';
                    }
                } else {
                    udpBatchingInfo.innerHTML = 'ℹ️ UDP Batching: <span style="color: #6c757d;">DISABLED</span> - Single packet per datagram';
                }
            }

            // Update protocol statistics
            this.updateProtocolStats(data);
        });
    },
    
    updateLogs() {
        // Only fetch logs if section is visible
        const logsBlock = document.getElementById('logsBlock');
        const logsHidden = logsBlock && logsBlock.style.display === 'none';
        if (logsHidden) return;

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

        // Update SBUS source controls if SBUS protocol is active
        if (protocolType === 2 && document.getElementById('sbusSourceButtons')) {
            this.updateSbusSourceStatus();
        }
        
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

        // Determine Success Rate color
        const successRateNum = parseFloat(validPercent);
        let successRateColor = '#28a745'; // Green by default (>= 95%)
        if (successRateNum < 60) {
            successRateColor = '#dc3545'; // Dark red (< 60%)
        } else if (successRateNum < 80) {
            successRateColor = '#ff6b6b'; // Light red (60-80%)
        } else if (successRateNum < 95) {
            successRateColor = '#ffc107'; // Yellow (80-95%)
        }

        // Build HTML including source switching controls
        let html = `
            <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 15px; margin-bottom: 15px;">
                <div style="padding: 10px; border: 1px solid #ddd; border-radius: 4px;">
                    <h5 style="margin: 0 0 10px 0; color: #333;">SBUS Fast Path Statistics</h5>
                    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px; font-size: 14px;">
                        <div>Valid Frames:</div><div><strong style="color: #28a745;">${p.validFrames || 0}</strong></div>
                        <div>Invalid Frames:</div><div><strong style="color: #dc3545;">${p.invalidFrames || 0}</strong></div>
                        <div>Success Rate:</div><div><strong style="color: ${successRateColor}">${validPercent}%</strong></div>
                        <div>Last Activity:</div><div><strong style="color: ${(p.lastActivityMs || 0) < 5000 ? '#28a745' : '#dc3545'}">${this.formatLastActivity(p.lastActivityMs || 0)}</strong></div>
                    </div>
                </div>
            </div>
        `;

        return html;
    },

    createSbusSourceControls() {
        // Count actual SBUS input devices
        let inputCount = 0;

        // Count each SBUS input device
        if (this.config.device1Role === '1') {  // D1_SBUS_IN
            inputCount++;
        }
        if (this.config.device2Role === '3') {  // D2_SBUS_IN
            inputCount++;
        }
        if (this.config.device4Role === '4') {  // D4_SBUS_UDP_RX
            inputCount++;
        }

        // Only show controls if we have MORE THAN ONE input
        if (inputCount < 2) {
            return;  // Don't show controls if only one input
        }

        // Check if SBUS controls already exist
        const existingSbusContainer = document.querySelector('.sbus-source-container');
        if (existingSbusContainer) {
            return;  // Already exists, don't recreate
        }

        // Find placeholder to insert SBUS controls
        const placeholder = document.getElementById('sbusSourcePlaceholder');
        if (!placeholder) return;

        // Create new SBUS controls container as a section
        const sbusContainer = document.createElement('div');
        sbusContainer.className = 'section sbus-source-container';
        sbusContainer.style.cssText = 'padding: 15px; border: 2px solid #007bff; border-radius: 4px; background: #f0f8ff;';

        sbusContainer.innerHTML = `
            <h4 style="margin: 0 0 15px 0; color: #333;">🎮 SBUS Source Selection</h4>
            <div id="sbusSourceButtons" style="display: flex; gap: 10px; flex-wrap: wrap;">
                <span style="color: #666; font-style: italic;">Loading sources...</span>
            </div>
            <div id="sbusSourceStatus" style="margin-top: 10px; font-size: 13px; color: #666;">
                Active source: <span id="activeSourceName">Loading...</span>
            </div>
        `;

        // Insert at placeholder location
        placeholder.appendChild(sbusContainer);

        // Initialize SBUS source status after DOM is ready
        setTimeout(() => this.updateSbusSourceStatus(), 100);
    },

    renderSbusSourceControls() {
        // This function is now deprecated - controls are created once in createSbusSourceControls()
        return '';
    },

    updateSbusSourceStatus() {
        fetch('/sbus/status')
            .then(response => response.json())
            .then(data => {
                if (data.status === 'ok' && data.sources) {
                    const buttonsContainer = document.getElementById('sbusSourceButtons');
                    if (!buttonsContainer) return;

                    // Check if buttons already exist, if not create them
                    let existingButtons = buttonsContainer.querySelectorAll('.sbus-source-btn');

                    if (existingButtons.length === 0 || existingButtons.length !== data.sources.length + 1) {
                        // Create buttons only if they don't exist or count changed
                        buttonsContainer.innerHTML = '';

                        // Create AUTO button first
                        const autoBtn = document.createElement('button');
                        autoBtn.className = 'sbus-source-btn sbus-auto-btn';
                        autoBtn.setAttribute('data-source', 'auto');
                        autoBtn.onclick = () => this.setSbusMode(0); // 0 = AUTO
                        autoBtn.style.cssText = 'padding: 8px 16px; color: white; border: none; border-radius: 4px; cursor: pointer; transition: all 0.3s ease;';
                        autoBtn.textContent = 'AUTO';
                        buttonsContainer.appendChild(autoBtn);

                        // Create source buttons
                        data.sources.forEach(src => {
                            const btn = document.createElement('button');
                            btn.className = 'sbus-source-btn';
                            btn.setAttribute('data-source', src.id);
                            btn.setAttribute('data-device', src.deviceId);
                            btn.onclick = () => this.setSbusSource(src.id);
                            btn.style.cssText = 'padding: 8px 16px; color: white; border: none; border-radius: 4px; cursor: pointer; transition: all 0.3s ease;';

                            // Set initial text
                            btn.textContent = src.name || `Source ${src.id}`;

                            buttonsContainer.appendChild(btn);
                        });
                    }

                    // Update AUTO button style based on mode
                    const autoBtn = buttonsContainer.querySelector('.sbus-auto-btn');
                    if (autoBtn) {
                        const isAuto = data.mode === 0; // 0 = AUTO, 1 = MANUAL
                        if (isAuto) {
                            // AUTO mode active - button disabled (grayed out)
                            autoBtn.style.cssText = `
                                padding: 8px 16px;
                                background: #6c757d;
                                color: white;
                                border: 2px solid transparent;
                                border-radius: 4px;
                                cursor: not-allowed;
                                opacity: 0.6;
                                min-width: 100px;
                                text-shadow: 1px 1px 2px rgba(0,0,0,0.5);
                            `;
                            autoBtn.disabled = true;
                        } else {
                            // MANUAL mode - AUTO button clickable
                            autoBtn.style.cssText = `
                                padding: 8px 16px;
                                background: #28a745;
                                color: white;
                                border: 2px solid #1e7e34;
                                border-radius: 4px;
                                cursor: pointer;
                                min-width: 100px;
                                text-shadow: 1px 1px 2px rgba(0,0,0,0.5);
                            `;
                            autoBtn.disabled = false;
                        }
                    }

                    // Update existing buttons with current status
                    data.sources.forEach(src => {
                        const btn = buttonsContainer.querySelector(`[data-source="${src.id}"]`);
                        if (!btn) return;

                            // Calculate quality percentage (0-100)
                            let quality = 0;
                            if (src.hasData) {
                                // Simple quality: valid=100%, has data but not valid=50%, no data=0%
                                quality = src.valid ? 100 : 50;
                                // Reduce quality if failsafe
                                if (src.hasFailsafe) quality = Math.max(30, quality - 30);
                            }

                            // Determine colors based on active state and quality
                            let bgColor, borderColor;
                            if (src.id === data.activeSource) {
                                // Active source - green tones
                                bgColor = quality > 70 ? '#28a745' : quality > 30 ? '#ffc107' : '#dc3545';
                                borderColor = '#1e7e34';
                            } else {
                                // Inactive sources - blue/gray tones
                                bgColor = quality > 70 ? '#007bff' : quality > 30 ? '#6c757d' : '#495057';
                                borderColor = 'transparent';
                            }

                            // Create gradient background (progress bar effect)
                            const gradientBg = `linear-gradient(90deg, ${bgColor} ${quality}%, rgba(108, 117, 125, 0.3) ${quality}%)`;

                            // Apply styles with gradient
                            btn.style.cssText = `
                                padding: 8px 16px;
                                background: ${gradientBg};
                                color: white;
                                border: 2px solid ${borderColor};
                                border-radius: 4px;
                                cursor: pointer;
                                font-weight: ${src.id === data.activeSource ? 'bold' : 'normal'};
                                position: relative;
                                min-width: 150px;
                                text-shadow: 1px 1px 2px rgba(0,0,0,0.5);
                            `;

                            // Button text with quality percentage
                            let btnText = src.name || 'Source ' + src.id;
                            if (quality > 0) {
                                btnText += ` (${quality}%)`;
                            }
                            if (src.framesReceived > 0 && src.framesReceived < 1000) {
                                btnText += ` ${src.framesReceived}fr`;
                            } else if (src.framesReceived >= 1000) {
                                btnText += ` ${Math.floor(src.framesReceived/1000)}k`;
                            }

                            btn.textContent = btnText;
                            buttonsContainer.appendChild(btn);
                    });

                    // Update status text
                    const activeSource = data.sources.find(s => s.id === data.activeSource);
                    const statusSpan = document.getElementById('activeSourceName');
                    if (statusSpan && activeSource) {
                        statusSpan.textContent = activeSource.name || 'Source ' + activeSource.id;
                    }
                }
            })
            .catch(error => {
                console.error('Failed to fetch SBUS status:', error);
            });
    },


    setSbusSource(source) {
        fetch(`/sbus/set_source?source=${source}`)
            .then(response => response.json())
            .then(data => {
                if (data.status === 'ok') {
                    // Update button states immediately
                    this.updateSbusSourceStatus();

                    // Show feedback
                    const btn = document.querySelector(`[data-source="${source}"]`);
                    if (btn) {
                        const originalText = btn.textContent;
                        btn.textContent = 'Switched!';
                        setTimeout(() => {
                            btn.textContent = originalText;
                        }, 1000);
                    }
                }
            })
            .catch(error => {
                console.error('Failed to set SBUS source:', error);
                alert('Failed to switch SBUS source');
            });
    },

    setSbusMode(mode) {
        fetch(`/sbus/set_mode?mode=${mode}`)
            .then(response => response.json())
            .then(data => {
                if (data.status === 'ok') {
                    // Update button states immediately
                    this.updateSbusSourceStatus();

                    // Show feedback
                    const autoBtn = document.querySelector('.sbus-auto-btn');
                    if (autoBtn) {
                        const originalText = autoBtn.textContent;
                        autoBtn.textContent = 'AUTO Enabled!';
                        setTimeout(() => {
                            autoBtn.textContent = originalText;
                        }, 1000);
                    }
                }
            })
            .catch(error => {
                console.error('Failed to set SBUS mode:', error);
                alert('Failed to switch SBUS mode');
            });
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

function toggleLogs() {
    Utils.rememberedToggle('logsBlock', 'logsArrow', 'collapse:logs');
}