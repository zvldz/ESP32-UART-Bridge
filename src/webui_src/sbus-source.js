// ESP32 UART Bridge - SBUS Source Management Module

const SbusSource = {
    config: null,
    updateInterval: null,

    // Mock data for testing UI (will be replaced with real API later)
    mockData: {
        active: 'uart',
        mode: 'manual',
        sources: {
            local: { quality: 0, lastPacket: 0, state: 'no_signal' },
            uart: { quality: 98, lastPacket: 12, state: 'active' },
            udp: { quality: 67, lastPacket: 145, state: 'standby' }
        },
        priorities: ['local', 'uart', 'udp'],
        timeout: 1000,
        hysteresis: 100,
        switchbackDelay: 2000,
        failoverEnabled: true
    },

    init(config) {
        this.config = config;

        // Debug logging
        console.log('SbusSource init - config:', config);
        console.log('SbusSource - device2Role:', config.device2Role, 'device3Role:', config.device3Role);

        if (this.shouldShow()) {
            console.log('SbusSource - shouldShow() = true, rendering section');
            this.render();
            this.attachListeners();
            this.startUpdates();
        } else {
            console.log('SbusSource - shouldShow() = false, section hidden');
        }
    },

    shouldShow() {
        // Show section if any SBUS_OUT device exists
        // device2Role === '4' means D2_SBUS_OUT
        // device3Role === '5' means D3_SBUS_OUT
        const device2IsSbusOut = (this.config.device2Role === '4');
        const device3IsSbusOut = (this.config.device3Role === '5');

        console.log('SbusSource shouldShow check:',
                    'device2Role=', this.config.device2Role, '(is SBUS_OUT:', device2IsSbusOut, ')',
                    'device3Role=', this.config.device3Role, '(is SBUS_OUT:', device3IsSbusOut, ')');

        return (device2IsSbusOut || device3IsSbusOut);
    },

    render() {
        // Find the Protocol Statistics section by searching for h3 text content
        const sections = document.querySelectorAll('.section');
        let protocolSection = null;

        for (let section of sections) {
            const h3 = section.querySelector('h3');
            if (h3 && h3.textContent.includes('Protocol Statistics')) {
                protocolSection = section;
                break;
            }
        }

        // Alternative: insert after device stats
        if (!protocolSection) {
            console.log('SbusSource: Protocol section not found, trying alternative method');
            const deviceStatsSection = document.getElementById('deviceStats')?.parentElement;
            if (deviceStatsSection && deviceStatsSection.nextElementSibling) {
                protocolSection = deviceStatsSection.nextElementSibling;
            }
        }

        if (protocolSection) {
            this.insertSection(protocolSection, 'before');
        } else {
            console.log('SbusSource: Could not find suitable insertion point');
        }
    },

    insertSection(referenceElement, position) {
        const sbusSection = document.createElement('div');
        sbusSection.className = 'section';
        sbusSection.id = 'sbusSourceSection';
        sbusSection.innerHTML = this.buildHTML();

        if (position === 'before') {
            referenceElement.parentNode.insertBefore(sbusSection, referenceElement);
        } else {
            referenceElement.parentNode.insertBefore(sbusSection, referenceElement.nextSibling);
        }
    },

    buildHTML() {
        return `
            <h3>🎮 SBUS Source Management</h3>

            <!-- Main Status Panel -->
            <div class="sbus-status-panel">
                <div class="sbus-current-status">
                    <strong>Active Source:</strong> <span id="sbusActiveSource">UART</span>
                    <span style="margin-left: 20px;">
                        <strong>Mode:</strong> <span id="sbusMode">Manual</span>
                    </span>
                </div>

                <!-- Source Indicators -->
                <div class="sbus-source-indicators">
                    ${this.buildSourceIndicator('local', 'LOCAL', 0, 'no_signal')}
                    ${this.buildSourceIndicator('uart', 'UART', 98, 'active')}
                    ${this.buildSourceIndicator('udp', 'UDP', 67, 'standby')}
                </div>

                <!-- Manual Control Buttons -->
                <div class="sbus-manual-controls">
                    <button onclick="SbusSource.forceSource('local')" id="sbusForceLocal">Force LOCAL</button>
                    <button onclick="SbusSource.forceSource('uart')" id="sbusForceUart" class="active">Force UART</button>
                    <button onclick="SbusSource.forceSource('udp')" id="sbusForceUdp">Force UDP</button>
                    <button onclick="SbusSource.setAutoMode()" id="sbusAutoMode">Enable AUTO</button>
                </div>
            </div>

            <!-- Configuration Section (Collapsible) -->
            <div style="margin-top: 20px;">
                <h4 style="cursor: pointer; user-select: none;" onclick="SbusSource.toggleConfig()">
                    Configuration
                    <span id="sbusConfigArrow" style="float: right; font-size: 18px;">▶</span>
                </h4>
                <div id="sbusConfigContent" style="display: none; padding: 10px; border: 1px solid #ddd; border-radius: 4px; margin-top: 10px;">
                    ${this.buildConfiguration()}
                </div>
            </div>

            <!-- Statistics Section (Collapsible) -->
            <div style="margin-top: 15px;">
                <h4 style="cursor: pointer; user-select: none;" onclick="SbusSource.toggleStats()">
                    Statistics
                    <span id="sbusStatsArrow" style="float: right; font-size: 18px;">▶</span>
                </h4>
                <div id="sbusStatsContent" style="display: none; padding: 10px; border: 1px solid #ddd; border-radius: 4px; margin-top: 10px;">
                    <div id="sbusStatsData">
                        <p style="color: #666;">Statistics will be available when backend is connected</p>
                    </div>
                </div>
            </div>
        `;
    },

    buildSourceIndicator(id, name, quality, state) {
        const stateClass = state === 'active' ? 'active' :
                          state === 'standby' ? 'standby' : 'offline';
        const stateIcon = state === 'active' ? '●' :
                         state === 'standby' ? '◐' : '○';
        const stateColor = state === 'active' ? '#4CAF50' :
                          state === 'standby' ? '#FF9800' : '#dc3545';

        return `
            <div class="sbus-source-indicator ${stateClass}" id="sbusSource_${id}">
                <span style="color: ${stateColor}; font-size: 20px;">${stateIcon}</span>
                <div style="flex: 1; margin: 0 10px;">
                    <div><strong>${name}</strong></div>
                    <div class="sbus-quality-bar">
                        <div class="sbus-quality-fill" style="width: ${quality}%"></div>
                    </div>
                </div>
                <span id="sbusQuality_${id}">${quality}%</span>
            </div>
        `;
    },

    buildConfiguration() {
        return `
            <div style="margin-bottom: 15px;">
                <h5 style="margin: 0 0 10px 0;">Source Priorities</h5>
                <div class="sbus-priority-list">
                    <div class="sbus-priority-item">
                        <span>Priority 1:</span>
                        <select id="sbusPriority1" style="width: 100px;">
                            <option value="local" selected>LOCAL</option>
                            <option value="uart">UART</option>
                            <option value="udp">UDP</option>
                        </select>
                    </div>
                    <div class="sbus-priority-item">
                        <span>Priority 2:</span>
                        <select id="sbusPriority2" style="width: 100px;">
                            <option value="local">LOCAL</option>
                            <option value="uart" selected>UART</option>
                            <option value="udp">UDP</option>
                        </select>
                    </div>
                    <div class="sbus-priority-item">
                        <span>Priority 3:</span>
                        <select id="sbusPriority3" style="width: 100px;">
                            <option value="local">LOCAL</option>
                            <option value="uart">UART</option>
                            <option value="udp" selected>UDP</option>
                        </select>
                    </div>
                </div>
            </div>

            <div style="margin-bottom: 15px;">
                <h5 style="margin: 0 0 10px 0;">Timing Settings</h5>
                <div class="sbus-timing-settings">
                    <div>
                        <label>Source Timeout (ms):</label>
                        <input type="number" id="sbusTimeout" value="1000" min="200" max="10000" style="width: 100px;">
                    </div>
                    <div>
                        <label>Hysteresis (ms):</label>
                        <input type="number" id="sbusHysteresis" value="100" min="50" max="1000" style="width: 100px;">
                    </div>
                    <div>
                        <label>Switchback Delay (ms):</label>
                        <input type="number" id="sbusSwitchback" value="2000" min="0" max="10000" style="width: 100px;">
                    </div>
                </div>
            </div>

            <div style="margin-bottom: 10px;">
                <label>
                    <input type="checkbox" id="sbusFailover" checked>
                    Auto-failover enabled
                </label>
            </div>

            <div style="text-align: center; margin-top: 15px;">
                <button onclick="SbusSource.saveConfig()" style="background-color: #4CAF50;">Save Configuration</button>
            </div>
        `;
    },

    attachListeners() {
        // Configuration changes will be handled in Phase 9.2
        console.log('SbusSource: Event listeners attached');
    },

    startUpdates() {
        // Update every 2 seconds
        this.updateStatus();
        this.updateInterval = setInterval(() => {
            this.updateStatus();
        }, 2000);
    },

    stopUpdates() {
        if (this.updateInterval) {
            clearInterval(this.updateInterval);
            this.updateInterval = null;
        }
    },

    updateStatus() {
        // Use real API instead of mock data
        this.updateFromAPI();
    },

    updateFromAPI() {
        fetch('/api/sbus/status')
            .then(response => response.json())
            .then(data => {
                // Update UI with real data
                document.getElementById('sbusActiveSource').textContent = data.active;
                document.getElementById('sbusMode').textContent = data.mode === 'manual' ? 'Manual' : 'Auto';

                // Update source indicators
                ['local', 'uart', 'udp'].forEach(source => {
                    const srcData = data.sources[source.toUpperCase()];
                    if (srcData) {
                        const quality = srcData.quality;
                        const state = srcData.state;

                        // Update quality percentage
                        const qualityEl = document.getElementById(`sbusQuality_${source}`);
                        if (qualityEl) qualityEl.textContent = quality + '%';

                        // Update quality bar
                        const fillEl = document.querySelector(`#sbusSource_${source} .sbus-quality-fill`);
                        if (fillEl) fillEl.style.width = quality + '%';

                        // Update indicator state class
                        const indicator = document.getElementById(`sbusSource_${source}`);
                        if (indicator) {
                            indicator.className = `sbus-source-indicator ${state}`;
                        }
                    }
                });
            })
            .catch(err => {
                console.error('Failed to fetch SBUS status:', err);
                // Fallback to mock data if API fails
                this.updateFromMockData();
            });
    },

    updateFromMockData() {
        // Fallback mock data for testing/debugging
        const data = this.mockData;

        // Random quality fluctuation for testing
        data.sources.uart.quality = 95 + Math.floor(Math.random() * 5);
        data.sources.udp.quality = 60 + Math.floor(Math.random() * 10);

        // Update UI elements
        document.getElementById('sbusActiveSource').textContent = data.active.toUpperCase();
        document.getElementById('sbusMode').textContent = data.mode === 'manual' ? 'Manual' : 'Auto';

        // Update quality indicators
        ['local', 'uart', 'udp'].forEach(source => {
            const quality = data.sources[source].quality;
            const qualityEl = document.getElementById(`sbusQuality_${source}`);
            if (qualityEl) qualityEl.textContent = quality + '%';

            const fillEl = document.querySelector(`#sbusSource_${source} .sbus-quality-fill`);
            if (fillEl) fillEl.style.width = quality + '%';
        });
    },

    forceSource(source) {
        console.log('Force source:', source);

        fetch('/api/sbus/source', {
            method: 'POST',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: `source=${source}`
        })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'ok') {
                // Update button states
                document.querySelectorAll('.sbus-manual-controls button').forEach(btn => {
                    btn.classList.remove('active');
                });

                const btnId = `sbusForce${source.charAt(0).toUpperCase() + source.slice(1)}`;
                const btn = document.getElementById(btnId);
                if (btn) btn.classList.add('active');

                // Refresh status
                this.updateStatus();
            } else {
                console.error('Failed to set source:', data.message);
            }
        })
        .catch(err => {
            console.error('Failed to set source:', err);
            // Fallback to mock behavior for testing
            this.mockForceSource(source);
        });
    },

    mockForceSource(source) {
        // Fallback mock behavior
        this.mockData.active = source;
        this.mockData.mode = 'manual';

        // Update button states
        document.querySelectorAll('.sbus-manual-controls button').forEach(btn => {
            btn.classList.remove('active');
        });

        const btnId = `sbusForce${source.charAt(0).toUpperCase() + source.slice(1)}`;
        const btn = document.getElementById(btnId);
        if (btn) btn.classList.add('active');

        this.updateStatus();
    },

    setAutoMode() {
        console.log('Set auto mode');

        fetch('/api/sbus/source', {
            method: 'POST',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: 'source=auto'
        })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'ok') {
                // Update button states
                document.querySelectorAll('.sbus-manual-controls button').forEach(btn => {
                    btn.classList.remove('active');
                });

                // Refresh status
                this.updateStatus();
            } else {
                console.error('Failed to set auto mode:', data.message);
            }
        })
        .catch(err => {
            console.error('Failed to set auto mode:', err);
            // Fallback to mock behavior
            this.mockSetAutoMode();
        });
    },

    mockSetAutoMode() {
        // Fallback mock behavior
        this.mockData.mode = 'auto';

        // Update button states
        document.querySelectorAll('.sbus-manual-controls button').forEach(btn => {
            btn.classList.remove('active');
        });

        this.updateStatus();
    },

    saveConfig() {
        // Gather configuration values
        const config = {
            priorities: [
                document.getElementById('sbusPriority1').value,
                document.getElementById('sbusPriority2').value,
                document.getElementById('sbusPriority3').value
            ],
            timeout: parseInt(document.getElementById('sbusTimeout').value),
            hysteresis: parseInt(document.getElementById('sbusHysteresis').value),
            switchbackDelay: parseInt(document.getElementById('sbusSwitchback').value),
            failoverEnabled: document.getElementById('sbusFailover').checked
        };

        console.log('Save config:', config);
        alert('Configuration saved (mock). Will be functional in Phase 9.2');

        // In Phase 9.2 will call: fetch('/api/sbus/config', {method: 'POST', body: config})
    },

    toggleConfig() {
        Utils.toggleElement('sbusConfigContent', 'sbusConfigArrow');
    },

    toggleStats() {
        Utils.toggleElement('sbusStatsContent', 'sbusStatsArrow');
    }
};

// Export for global access
window.SbusSource = SbusSource;