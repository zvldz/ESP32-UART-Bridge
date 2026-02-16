/**
 * ESP32 UART Bridge - Alpine.js Application
 *
 * Stores:
 * - app: static configuration, device roles, board info
 * - status: runtime data (uptime, stats, traffic) - polled every 5s
 * - sbus: SBUS routing state - polled every 5s if SBUS active
 */

// Default SBUS text output rate (Hz) - string for x-model compatibility
const DEFAULT_OUT_RATE = '50';

// Fields tracked for dirty checking (used in isDirty and _takeSnapshot)
const TRACKED_FIELDS = [
    'device1Role', 'device2Role', 'device3Role', 'device4Role', 'device5Role',
    'baudrate', 'databits', 'parity', 'stopbits', 'flowcontrol',
    'wifiMode', 'ssid', 'password', 'wifiApMode', 'wifiTxPower', 'wifiApChannel', 'mdnsHostname',
    'wifiNetwork0Ssid', 'wifiNetwork0Pass', 'wifiNetwork1Ssid', 'wifiNetwork1Pass',
    'wifiNetwork2Ssid', 'wifiNetwork2Pass', 'wifiNetwork3Ssid', 'wifiNetwork3Pass',
    'wifiNetwork4Ssid', 'wifiNetwork4Pass',
    'protocolOptimization', 'mavlinkRouting', 'sbusTimingKeeper',
    'device4TargetIP', 'device4TargetPort', 'device4SbusFormat',
    'device4AutoBroadcast', 'device4UdpTimeout', 'udpBatching',
    'device2OutRate', 'device3OutRate', 'device4OutRate', 'btSendRate',
    'logLevelWeb', 'logLevelUart', 'logLevelNetwork',
    'usbMode'
];

document.addEventListener('alpine:init', () => {

    // =========================================================================
    // APP STORE - Static configuration (loaded once on init)
    // =========================================================================
    Alpine.store('app', {
        // Loading state
        loading: true,
        error: null,

        // Board info (static)
        boardType: '',
        version: '',
        arduinoVersion: '',
        idfVersion: '',
        deviceName: '',
        usbHostSupported: false,
        uart2Available: true,
        sbusMavlinkEnabled: false,
        btSupported: false,
        bleSupported: false,

        // Device roles (from saved config)
        device1Role: '0',
        device2Role: '0',
        device3Role: '0',
        device4Role: '0',
        device5Role: '0',

        // Device 4 extended config
        device4TargetIP: '',
        device4TargetPort: '14550',
        device4SbusFormat: '0',
        device4AutoBroadcast: false,
        device4UdpTimeout: '1000',
        udpBatching: true,

        // USB mode (Device 2 USB role)
        usbMode: 'device',

        // UART config (strings for x-model select compatibility)
        baudrate: '115200',
        databits: '8',
        parity: 'N',
        stopbits: '1',
        flowcontrol: false,  // Boolean for checkbox
        _customBaudrate: false,  // User selected "Custom" in baudrate dropdown

        // WiFi config
        wifiMode: '0',
        ssid: '',
        password: '',
        wifiApMode: '',    // Loaded from API
        wifiTxPower: '',   // Loaded from API
        wifiApChannel: '',
        mdnsHostname: '',
        defaultHostname: '',  // MAC-based hostname (read-only, from API)

        // WiFi client networks (5 slots: primary + 4 additional)
        wifiNetwork0Ssid: '',
        wifiNetwork0Pass: '',
        wifiNetwork1Ssid: '',
        wifiNetwork1Pass: '',
        wifiNetwork2Ssid: '',
        wifiNetwork2Pass: '',
        wifiNetwork3Ssid: '',
        wifiNetwork3Pass: '',
        wifiNetwork4Ssid: '',
        wifiNetwork4Pass: '',

        // Protocol
        protocolOptimization: '0',
        mavlinkRouting: false,
        sbusTimingKeeper: false,

        // Log levels (-1=OFF, 0=ERROR, 1=WARNING, 2=INFO, 3=DEBUG)
        logLevelWeb: '1',
        logLevelUart: '1',
        logLevelNetwork: '-1',

        // Text output rates (Hz, 10-70)
        device2OutRate: DEFAULT_OUT_RATE,
        device3OutRate: DEFAULT_OUT_RATE,
        device4OutRate: DEFAULT_OUT_RATE,
        btSendRate: DEFAULT_OUT_RATE,

        // UI state
        logDisplayCount: 30,

        // Original values snapshot for dirty checking
        _original: null,

        // Computed: is form dirty (has unsaved changes)
        get isDirty() {
            if (!this._original) return false;
            return TRACKED_FIELDS.some(f => this[f] !== this._original[f]);
        },

        // Computed: is any SBUS role active on D1/D2/D3 only
        // Used for Device 4/5 options to avoid circular dependency
        get _isSbusActiveOnD1D2D3() {
            return this.device1Role === '1' ||           // D1_SBUS_IN
                   this.device2Role === '3' ||           // D2_SBUS_IN
                   this.device2Role === '4' ||           // D2_SBUS_OUT
                   this.device2Role === '5' ||           // D2_USB_SBUS_TEXT
                   this.device3Role === '4' ||           // D3_SBUS_IN
                   this.device3Role?.startsWith('5');    // D3_SBUS_OUT (5, 5_0, 5_1)
        },

        // Computed: is any SBUS role active (all devices)
        get isSbusActive() {
            return this._isSbusActiveOnD1D2D3 ||
                   this.device4Role?.startsWith('3') ||  // D4_SBUS_UDP_TX
                   this.device4Role === '4' ||           // D4_SBUS_UDP_RX
                   this.device5Role === '2';             // D5_BT_SBUS_TEXT
        },

        // Computed: is CRSF/ELRS active on Device 1
        get isCrsfActive() {
            return this.device1Role === '2';  // D1_CRSF_IN
        },

        // Computed: has SBUS input
        get hasSbusInput() {
            return this.device1Role === '1' ||           // D1_SBUS_IN
                   this.device2Role === '3' ||           // D2_SBUS_IN
                   this.device3Role === '4' ||           // D3_SBUS_IN
                   this.device4Role === '4';             // D4_SBUS_UDP_RX
        },

        // Computed: has SBUS output
        get hasSbusOutput() {
            return this.device2Role === '4' ||           // D2_SBUS_OUT
                   this.device2Role === '5' ||           // D2_USB_SBUS_TEXT
                   this.device3Role?.startsWith('5') ||  // D3_SBUS_OUT
                   this.device4Role?.startsWith('3') ||  // D4_SBUS_UDP_TX
                   this.device5Role === '2';             // D5_BT_SBUS_TEXT
        },

        // Computed: show SBUS config warning
        get showSbusWarning() {
            return this.hasSbusOutput && !this.hasSbusInput;
        },

        // Computed: has UART devices (for UART config visibility)
        get hasUartDevices() {
            // D1: UART is role 0
            // D2: UART2 is role 1
            // D3: UART3 is role 0 or 1
            return this.device1Role === '0' ||
                   this.device2Role === '1' ||
                   this.device3Role === '0' ||
                   this.device3Role === '1';
        },

        // Computed: is current baudrate a custom (non-standard) value
        get baudrateIsCustom() {
            return this._customBaudrate ||
                !['9600','19200','38400','57600','115200','230400','460800','921600'].includes(String(this.baudrate));
        },

        // Computed: show Device 4 network config
        get showDevice4Network() {
            return this.device4Role !== '0';  // Any role except disabled
        },

        // Computed: is MiniKit board (any variant)
        get isMiniKit() {
            return this.boardType === 'minikit' || this.boardType === 'minikit_bt';
        },

        // Computed: show Device 5 row (BT Classic or BLE enabled)
        get showDevice5() {
            return this.btSupported || this.bleSupported;
        },

        // Computed: show Auto Broadcast checkbox (Client mode + TX role)
        get showAutoBroadcast() {
            const baseRole = this.device4Role?.split('_')[0];
            return this.wifiMode === '1' && (baseRole === '1' || baseRole === '3');
        },

        // Computed: show USB Mode block (Device 2 is USB)
        get showUsbModeBlock() {
            return this.device2Role === '2';
        },

        // Computed: show SBUS Timing Keeper (Device 4 is SBUS UDP RX)
        get showSbusTimingKeeper() {
            return this.device4Role === '4';
        },

        // Computed: show MAVLink Routing section
        get showMavlinkRouting() {
            return this.protocolOptimization === '1' && !this.isSbusActive;
        },

        // XIAO GPIO to D-pin mapping
        _xiaoGpioMap: {
            1: 'D0', 2: 'D1', 3: 'D2', 4: 'D3', 5: 'D4', 6: 'D5', 7: 'D6',
            8: 'D8', 9: 'D9', 10: 'D10', 43: 'D6', 44: 'D7'
        },

        // Format GPIO pin with D-pin name for XIAO
        _formatGpio(gpioNum) {
            if (this.boardType === 'xiao' && this._xiaoGpioMap[gpioNum]) {
                return `GPIO ${gpioNum} (${this._xiaoGpioMap[gpioNum]})`;
            }
            return `GPIO ${gpioNum}`;
        },

        // Format GPIO pin pair
        _formatGpioPair(gpio1, gpio2) {
            if (this.boardType === 'xiao' && this._xiaoGpioMap[gpio1] && this._xiaoGpioMap[gpio2]) {
                return `GPIO ${gpio1}/${gpio2} (${this._xiaoGpioMap[gpio1]}/${this._xiaoGpioMap[gpio2]})`;
            }
            return `GPIO ${gpio1}/${gpio2}`;
        },

        // Computed: Device 1 pins text
        get device1PinsText() {
            if (this.device1Role === '0') {
                return this._formatGpioPair(4, 5);
            }
            return this._formatGpio(4) + ' (RX)';  // SBUS IN / CRSF IN
        },

        // Computed: Device 2 pins text
        get device2PinsText() {
            const role = this.device2Role;
            if (role === '1') return this._formatGpioPair(8, 9);  // UART2
            if (role === '2' || role === '5' || role === '6' || role === '7' || role === '8') return 'USB';
            if (role === '3') return this._formatGpio(8) + ' (RX)'; // SBUS IN
            if (role === '4') return this._formatGpio(9) + ' (TX)'; // SBUS OUT
            return 'N/A';
        },

        // Computed: Device 3 pins text
        get device3PinsText() {
            const role = this.device3Role;
            const isMiniKit = this.boardType === 'minikit' || this.boardType === 'minikit_bt';
            const isXiao = this.boardType === 'xiao';

            if (role === '1' || role === '2') {  // UART3 Mirror/Bridge
                if (isMiniKit) return 'GPIO 16/17';
                return isXiao ? this._formatGpioPair(43, 44) : 'GPIO 11/12';
            }
            if (role === '3') {  // UART3 Logger
                if (isMiniKit) return 'GPIO 17 (TX only)';
                return isXiao ? this._formatGpio(43) + ' (TX only)' : 'GPIO 12 (TX only)';
            }
            if (role === '4') {  // SBUS IN
                if (isMiniKit) return 'GPIO 16 (RX)';
                return isXiao ? this._formatGpio(44) + ' (RX)' : 'GPIO 11 (RX)';
            }
            if (role === '5' || role?.startsWith('5')) {  // SBUS OUT
                if (isMiniKit) return 'GPIO 17 (TX)';
                return isXiao ? this._formatGpio(43) + ' (TX)' : 'GPIO 12 (TX)';
            }
            if (role === '6') {  // CRSF Bridge (TX only for Phase 2)
                if (isMiniKit) return 'GPIO 17 (TX)';
                return isXiao ? this._formatGpio(43) + ' (TX)' : 'GPIO 12 (TX)';
            }
            return 'N/A';
        },

        // Computed: Device 4 pins text
        get device4PinsText() {
            const baseRole = this.device4Role?.split('_')[0];
            if (baseRole === '1' || baseRole === '2' || baseRole === '3' || baseRole === '4' || baseRole === '5') {
                return 'Network';
            }
            return 'N/A';
        },

        // Computed: Device 4 options based on SBUS/CRSF context
        get device4Options() {
            const noSbusIn = !this.hasSbusInput;
            const crsf = this.isCrsfActive;
            const inputMode = this.isSbusActive || crsf;
            return [
                { value: '0', label: 'Disabled', disabled: false },
                { value: '1', label: 'Bridge', disabled: inputMode },
                { value: '2', label: 'UDP Logger', disabled: false },
                { value: '3_0', label: 'SBUS Output', disabled: noSbusIn },
                { value: '3_1', label: 'SBUS Text Output', disabled: noSbusIn },
                { value: '4', label: 'SBUS Input', disabled: noSbusIn },
                { value: '5', label: 'CRSF Text Output', disabled: !crsf }
            ];
        },

        // Computed: Device 5 options based on SBUS/CRSF context
        get device5Options() {
            const inputMode = this.isSbusActive || this.isCrsfActive;
            const noSbusIn = !this.hasSbusInput;
            return [
                { value: '0', label: 'Disabled', disabled: false },
                { value: '1', label: 'Bridge', disabled: inputMode },
                { value: '2', label: 'SBUS Text Output', disabled: noSbusIn },
                { value: '3', label: 'CRSF Text Output', disabled: !this.isCrsfActive }
            ];
        },

        // Computed: show Device 2 text rate selector (USB SBUS/CRSF Text mode)
        get showDevice2OutRate() {
            return this.device2Role === '5' || this.device2Role === '7';
        },

        // Computed: show Device 3 output rate selector (SBUS Text Output)
        get showDevice3OutRate() {
            return this.device3Role === '5_1';
        },

        // Computed: show Device 4 output rate selector (SBUS/CRSF Text Output)
        get showDevice4OutRate() {
            return this.device4Role === '3_1' || this.device4Role === '5';
        },

        // Computed: show Device 5 BT output rate selector (SBUS/CRSF Text)
        get showDevice5OutRate() {
            return this.device5Role === '2' || this.device5Role === '3';
        },

        // Computed: Device 2 role options (some disabled for MiniKit or SBUS context)
        get device2Options() {
            const isCrsf = this.isCrsfActive;
            const noSbusIn = !this.hasSbusInput;
            const d1IsInput = this.device1Role === '1' || isCrsf;  // D1 SBUS_IN or CRSF_IN
            return [
                { value: '0', label: 'Disabled', disabled: false },
                { value: '1', label: 'UART2', disabled: !this.uart2Available || d1IsInput },
                { value: '2', label: 'USB', disabled: d1IsInput },
                { value: '6', label: 'USB Logger', disabled: false },
                { value: '3', label: 'SBUS Input', disabled: !this.uart2Available || isCrsf },
                { value: '4', label: 'SBUS Output', disabled: !this.uart2Available || isCrsf || noSbusIn },
                { value: '5', label: 'USB SBUS Text Output', disabled: isCrsf || noSbusIn },
                { value: '7', label: 'USB CRSF Text Output', disabled: !isCrsf },
                { value: '8', label: 'USB CRSF Bridge', disabled: !isCrsf }
            ];
        },

        // Computed: Device 3 role options
        get device3Options() {
            const d1SbusOrCrsf = this.device1Role === '1' || this.device1Role === '2';
            const isCrsf = this.isCrsfActive;
            const noSbusIn = !this.hasSbusInput;
            return [
                { value: '0', label: 'Disabled', disabled: false },
                { value: '1', label: 'UART3 Mirror', disabled: d1SbusOrCrsf },
                { value: '2', label: 'UART3 Bridge', disabled: d1SbusOrCrsf },
                { value: '3', label: 'UART3 Logger', disabled: false },
                { value: '4', label: 'SBUS Input', disabled: isCrsf },
                { value: '5_0', label: 'SBUS Output', disabled: isCrsf || noSbusIn },
                { value: '5_1', label: 'SBUS Text Output', disabled: isCrsf || noSbusIn },
                { value: '6', label: 'CRSF Bridge', disabled: !isCrsf }
            ];
        },

        // Handle any SBUS role change - auto-set Device 1 to SBUS_IN
        // Uses setTimeout(0) to run after x-model updates the value
        _checkAutoSbusIn() {
            setTimeout(() => this._doCheckAutoSbusIn(), 0);
        },

        _doCheckAutoSbusIn() {
            // Reset SBUS output roles when no SBUS input source exists
            if (!this.hasSbusInput) {
                // Reset D2 SBUS output roles
                if (this.device2Role === '4' || this.device2Role === '5') {
                    this.device2Role = '0';
                }
                // Reset D3 SBUS output roles
                if (this.device3Role?.startsWith('5')) {
                    this.device3Role = '0';
                }
                // Reset D4 SBUS roles
                const d4Base = this.device4Role?.split('_')[0];
                if (d4Base === '3' || d4Base === '4') {
                    this.device4Role = '0';
                }
                // Reset D5 SBUS Text
                if (this.device5Role === '2') {
                    this.device5Role = '0';
                }
            }

            // Reset incompatible roles when D1 is in input mode (SBUS/CRSF)
            if (this.device1Role === '1' || this.isCrsfActive) {
                // Plain USB and UART2 have no data path without telemetry flow
                if (this.device2Role === '1' || this.device2Role === '2') {
                    this.device2Role = '0';
                }
                // Mirror and Bridge need telemetry flow from D1
                if (this.device3Role === '1' || this.device3Role === '2') {
                    this.device3Role = '0';
                }
            }

            // Auto-set protocol optimization based on active mode
            if (this.isCrsfActive && this.protocolOptimization !== '3') {
                this.protocolOptimization = '3';
            } else if (this.isSbusActive && this.protocolOptimization !== '2') {
                this.protocolOptimization = '2';
            } else if (!this.isSbusActive && !this.isCrsfActive &&
                       (this.protocolOptimization === '2' || this.protocolOptimization === '3')) {
                this.protocolOptimization = '0';
            }
        },

        // Handle Device 4 role change - update port defaults
        onDevice4RoleChange(newRole, oldRole) {
            const baseRole = newRole?.split('_')[0];
            const oldBase = oldRole?.split('_')[0];

            // Only update port when role type changes (strings for consistency)
            if (baseRole !== oldBase) {
                if (baseRole === '1') this.device4TargetPort = '14550';       // Bridge
                else if (baseRole === '2') this.device4TargetPort = '14560'; // Logger
                else if (baseRole === '3') this.device4TargetPort = '14551'; // SBUS TX
                else if (baseRole === '4') this.device4TargetPort = '14552'; // SBUS RX (listen port)
            }

            this._checkAutoSbusIn();
        },

        // Load config from API
        async init() {
            try {
                const response = await fetch('/api/config');
                if (!response.ok) throw new Error('Failed to load config');
                const data = await response.json();

                // Board info
                this.boardType = data.boardType || 's3zero';
                this.version = data.version || '';
                this.arduinoVersion = data.arduinoVersion || '';
                this.idfVersion = data.idfVersion || '';
                this.deviceName = data.deviceName || 'UART Bridge';
                this.usbHostSupported = data.usbHostSupported ?? true;
                this.uart2Available = data.uart2Available ?? true;
                this.sbusMavlinkEnabled = data.sbusMavlinkEnabled ?? false;
                this.btSupported = data.btSupported ?? false;
                this.bleSupported = data.bleSupported ?? false;

                // Device roles
                this.device1Role = String(data.device1Role ?? '0');
                this.device2Role = String(data.device2Role ?? '0');
                this.device3Role = String(data.device3Role ?? '0');
                this.device4Role = String(data.device4Role ?? '0');
                this.device5Role = String(data.device5Role ?? '0');

                // Device 4 config (strings for x-model number input compatibility)
                this.device4TargetIP = data.device4TargetIp || data.device4TargetIP || '192.168.4.2';
                this.device4TargetPort = String(data.device4Port ?? data.device4TargetPort ?? 14550);
                this.device4SbusFormat = String(data.device4SbusFormat ?? '0');
                this.device4AutoBroadcast = data.device4AutoBroadcast ?? false;
                this.device4UdpTimeout = String(data.device4UdpTimeout ?? 1000);
                this.udpBatching = data.udpBatchingEnabled ?? true;

                // USB mode
                this.usbMode = data.usbMode || 'device';

                // UART config (strings for x-model select compatibility)
                this.baudrate = String(data.baudrate ?? 115200);
                this.databits = String(data.databits ?? 8);
                this.parity = data.parity || 'N';
                this.stopbits = String(data.stopbits ?? 1);
                this.flowcontrol = Boolean(data.flowcontrol);  // Convert 0/1 to boolean

                // WiFi config (strings for x-model select compatibility)
                this.wifiMode = String(data.wifiMode ?? 0);
                this.ssid = data.ssid || '';
                this.password = data.password || '';
                this.wifiApMode = String(data.wifiApMode ?? 1);  // Fallback for migration from old config
                this.wifiTxPower = String(data.wifiTxPower);
                this.wifiApChannel = String(data.wifiApChannel);
                this.mdnsHostname = data.mdnsHostname || '';
                this.defaultHostname = data.defaultHostname || '';

                // WiFi client networks (from array to individual fields)
                const networks = data.wifiNetworks || [];
                this.wifiNetwork0Ssid = networks[0]?.ssid || '';
                this.wifiNetwork0Pass = networks[0]?.password || '';
                this.wifiNetwork1Ssid = networks[1]?.ssid || '';
                this.wifiNetwork1Pass = networks[1]?.password || '';
                this.wifiNetwork2Ssid = networks[2]?.ssid || '';
                this.wifiNetwork2Pass = networks[2]?.password || '';
                this.wifiNetwork3Ssid = networks[3]?.ssid || '';
                this.wifiNetwork3Pass = networks[3]?.password || '';
                this.wifiNetwork4Ssid = networks[4]?.ssid || '';
                this.wifiNetwork4Pass = networks[4]?.password || '';

                // Protocol (convert to string for x-model select compatibility)
                this.protocolOptimization = String(data.protocolOptimization ?? 0);
                this.mavlinkRouting = data.mavlinkRouting ?? false;
                this.sbusTimingKeeper = data.sbusTimingKeeper ?? false;

                // Log levels (convert to string for x-model select compatibility)
                this.logLevelWeb = String(data.logLevelWeb ?? 1);
                this.logLevelUart = String(data.logLevelUart ?? 1);
                this.logLevelNetwork = String(data.logLevelNetwork ?? -1);

                // Output rates (convert to string for x-model select compatibility)
                // Use || instead of ?? because 0 is invalid rate (valid range: 10-70)
                this.device2OutRate = String(data.device2OutRate || DEFAULT_OUT_RATE);
                this.device3OutRate = String(data.device3OutRate || DEFAULT_OUT_RATE);
                this.device4OutRate = String(data.device4OutRate || DEFAULT_OUT_RATE);
                this.btSendRate = String(data.btSendRate || DEFAULT_OUT_RATE);

                // UI
                this.logDisplayCount = data.logDisplayCount ?? 30;

                // Snapshot original values for dirty checking
                this._takeSnapshot();

                // Sync browser time to ESP (once per boot, fire-and-forget)
                fetch('/api/time?epoch=' + Math.floor(Date.now() / 1000)).catch(() => {});

                this.loading = false;
            } catch (err) {
                this.error = err.message;
                this.loading = false;
                console.error('Failed to load config:', err);
            }
        },

        // Take snapshot of current values for dirty checking
        _takeSnapshot() {
            this._original = {};
            TRACKED_FIELDS.forEach(f => this._original[f] = this[f]);
        },

        // Reset form to original values
        reset() {
            if (!this._original) return;
            Object.keys(this._original).forEach(key => {
                this[key] = this._original[key];
            });
            this._customBaudrate = false;
        },

        // Convert composite role value to base role and format
        // e.g. '5_1' -> { role: '5', format: '1' }
        _parseCompositeRole(value) {
            if (!value || typeof value !== 'string') return { role: String(value || '0'), format: '0' };
            const parts = value.split('_');
            return {
                role: parts[0],
                format: parts[1] || '0'
            };
        },

        // Build JSON object for save
        _buildJsonData() {
            // Device 3: composite role (5_0 -> role=5, format=0)
            const d3 = this._parseCompositeRole(this.device3Role);
            // Device 4: composite role (3_0 -> role=3, format=0)
            const d4 = this._parseCompositeRole(this.device4Role);

            return {
                // Device roles
                device1_role: parseInt(this.device1Role),
                device2_role: parseInt(this.device2Role),
                device2_out_rate: parseInt(this.device2OutRate),
                device3_role: parseInt(d3.role),
                device3_sbus_format: parseInt(d3.format),
                device3_out_rate: parseInt(this.device3OutRate),
                device4_role: parseInt(d4.role),
                device4_sbus_format: parseInt(d4.format),
                device4_out_rate: parseInt(this.device4OutRate),
                device5_role: parseInt(this.device5Role),
                bt_send_rate: parseInt(this.btSendRate),

                // UART config
                baudrate: parseInt(this.baudrate),
                databits: parseInt(this.databits),
                parity: this.parity,
                stopbits: parseInt(this.stopbits),
                flowcontrol: this.flowcontrol ? 1 : 0,  // Convert boolean to 0/1

                // WiFi config
                wifi_mode: parseInt(this.wifiMode),
                ssid: this.mdnsHostname,  // AP SSID = hostname (unified naming)
                password: this.password,
                wifi_ap_mode: parseInt(this.wifiApMode),
                wifi_tx_power: parseInt(this.wifiTxPower),
                wifi_ap_channel: parseInt(this.wifiApChannel),
                mdns_hostname: this.mdnsHostname,

                // WiFi networks (client mode)
                wifi_networks: [
                    { ssid: this.wifiNetwork0Ssid, password: this.wifiNetwork0Pass },
                    { ssid: this.wifiNetwork1Ssid, password: this.wifiNetwork1Pass },
                    { ssid: this.wifiNetwork2Ssid, password: this.wifiNetwork2Pass },
                    { ssid: this.wifiNetwork3Ssid, password: this.wifiNetwork3Pass },
                    { ssid: this.wifiNetwork4Ssid, password: this.wifiNetwork4Pass }
                ],

                // Protocol
                protocol_optimization: parseInt(this.protocolOptimization),
                mavlink_routing: this.mavlinkRouting,
                udp_batching: this.udpBatching,

                // Log levels
                log_level_web: parseInt(this.logLevelWeb),
                log_level_uart: parseInt(this.logLevelUart),
                log_level_network: parseInt(this.logLevelNetwork),

                // Device 4 network config
                device4_target_ip: this.device4TargetIP,
                device4_port: parseInt(this.device4TargetPort),
                device4_auto_broadcast: this.device4AutoBroadcast,
                device4_udp_timeout: parseInt(this.device4UdpTimeout),

                // USB mode
                usbmode: this.usbMode
            };
        },

        // Validate form before submission
        // Returns null if valid, error message string if invalid
        validate() {
            // AP mode validation (AP SSID = mdnsHostname)
            if (this.wifiMode === '0') {
                if (!this.mdnsHostname || this.mdnsHostname.trim() === '') {
                    return 'Device name (hostname) required for AP mode';
                }
                if (this.password && this.password.length > 0 && this.password.length < 8) {
                    return 'AP password: min 8 chars';
                }
            }
            // Client mode validation
            else if (this.wifiMode === '1') {
                // Check primary network
                if (!this.wifiNetwork0Ssid || this.wifiNetwork0Ssid.trim() === '') {
                    return 'Primary SSID required';
                }
                // Validate all network passwords
                const passwords = [
                    this.wifiNetwork0Pass,
                    this.wifiNetwork1Pass,
                    this.wifiNetwork2Pass,
                    this.wifiNetwork3Pass,
                    this.wifiNetwork4Pass
                ];
                for (let i = 0; i < passwords.length; i++) {
                    const pass = passwords[i];
                    if (pass && pass.length > 0 && pass.length < 8) {
                        return `Network ${i + 1} password: min 8 chars`;
                    }
                }
            }

            // Device 4 IP validation (only for TX roles when visible and not auto broadcast)
            if (this.showDevice4Network) {
                const baseRole = this.device4Role?.split('_')[0];
                // TX roles need valid IP (unless auto broadcast is enabled)
                if (baseRole !== '4' && !this.device4AutoBroadcast) {
                    const ip = this.device4TargetIP?.trim();
                    if (!ip) {
                        return 'Target IP required';
                    }
                    // Validate IP format (single IP or comma-separated)
                    const ipPattern = /^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$/;
                    const ips = ip.split(',').map(s => s.trim());
                    for (const singleIp of ips) {
                        if (!ipPattern.test(singleIp)) {
                            return `Invalid IP: ${singleIp}`;
                        }
                    }
                }
            }

            return null;  // No errors
        },

        // Save configuration to server
        async save() {
            // Validate before saving
            const validationError = this.validate();
            if (validationError) {
                throw new Error(validationError);
            }

            try {
                const data = this._buildJsonData();
                const response = await fetch('/save', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(data)
                });

                if (!response.ok) {
                    // Try to get error message from server
                    const errorData = await response.json().catch(() => ({}));
                    const errorMsg = errorData.message || `Save failed: ${response.status}`;
                    console.error('Server error:', errorData);
                    throw new Error(errorMsg);
                }

                // Server will reboot, update snapshot for clean state
                this._takeSnapshot();
                return { success: true };
            } catch (err) {
                console.error('Save error:', err);
                throw err;
            }
        }
    });

    // =========================================================================
    // STATUS STORE - Runtime data (polled every 5s)
    // =========================================================================
    Alpine.store('status', {
        // Polling
        pollInterval: null,

        // System info
        uptime: 0,
        freeRam: 0,

        // WiFi status
        wifiClientConnected: false,
        connectedSSID: '',
        ipAddress: '',
        rssiPercent: 0,
        tempNetworkMode: false,

        // Bluetooth (MiniKit)
        btInitialized: false,
        btConnected: false,

        // UART info
        uartConfig: '',
        flowControl: '',

        // Device role names (from status)
        device1RoleName: '-',
        device2RoleName: '-',
        device3RoleName: '-',
        device4RoleName: '-',
        device5RoleName: '-',

        // Device roles (numeric, from status)
        device2Role: 0,
        device3Role: 0,
        device4Role: 0,
        device5Role: 0,

        // Device statistics
        device1Rx: 0,
        device1Tx: 0,
        device2Rx: 0,
        device2Tx: 0,
        device3Rx: 0,
        device3Tx: 0,

        // Previous values for activity indicators
        _prev: {
            device1Rx: 0, device1Tx: 0,
            device2Rx: 0, device2Tx: 0,
            device3Rx: 0, device3Tx: 0,
            device4RxBytes: 0, device4TxBytes: 0,
            device5RxBytes: 0, device5TxBytes: 0
        },

        // Device 4 statistics
        device4TxBytes: 0,
        device4TxPackets: 0,
        device4RxBytes: 0,
        device4RxPackets: 0,

        // Device 5 statistics (Bluetooth)
        device5TxBytes: 0,
        device5RxBytes: 0,

        // Total
        totalTraffic: 0,
        lastActivity: 'Never',

        // Protocol stats (raw data from API)
        protocolStats: null,

        // Protocol type (computed)
        get protocolType() {
            return this.protocolStats?.protocolType ?? 0;
        },

        // Protocol name (computed)
        get protocolName() {
            const NAMES = { 0: 'RAW Protocol', 1: 'MAVLink Protocol', 2: 'SBUS Protocol', 3: 'CRSF Protocol' };
            return NAMES[this.protocolType] || 'Unknown Protocol';
        },

        // SBUS success rate (computed)
        get sbusSuccessRate() {
            const p = this.protocolStats?.parser;
            if (!p) return '0.0';
            const total = (p.validFrames || 0) + (p.invalidFrames || 0);
            const lastActivityMs = p.lastActivityMs || 0;
            if (lastActivityMs >= 0 && lastActivityMs < 5000 && total > 0) {
                return ((p.validFrames || 0) / total * 100).toFixed(1);
            }
            return '0.0';
        },

        // SBUS success rate CSS class (computed)
        get sbusSuccessRateClass() {
            const rate = parseFloat(this.sbusSuccessRate);
            if (rate >= 95) return 'sbus-rate-excellent';
            if (rate >= 80) return 'sbus-rate-good';
            if (rate >= 60) return 'sbus-rate-fair';
            return 'sbus-rate-poor';
        },

        // SBUS activity CSS class (computed)
        get sbusActivityClass() {
            const lastMs = this.protocolStats?.parser?.lastActivityMs ?? -1;
            return lastMs >= 0 && lastMs < 5000 ? 'text-success' : 'text-danger';
        },

        // WiFi signal bars text (computed)
        get wifiSignalBars() {
            const p = this.rssiPercent;
            if (p >= 75) return '●●●●';
            if (p >= 50) return '●●●○';
            if (p >= 25) return '●●○○';
            return '●○○○';
        },

        // WiFi signal CSS class (computed)
        get wifiSignalClass() {
            const p = this.rssiPercent;
            if (p >= 75) return 'wifi-signal-excellent';
            if (p >= 50) return 'wifi-signal-good';
            if (p >= 25) return 'wifi-signal-fair';
            return 'wifi-signal-weak';
        },

        // UDP batching
        udpBatchingEnabled: false,
        udpBatchingStats: null,  // { avgPacketsPerBatch, maxPacketsInBatch, batchEfficiency }

        // Logs
        logs: [],
        logsLoaded: false,
        logsPollInterval: null,

        // =========================================================================
        // UTILITY FUNCTIONS
        // =========================================================================

        // Format bytes to human readable
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

        // Format last activity time (public for use in templates)
        formatLastActivity(ms) {
            if (ms === undefined || ms === null || ms < 0) return 'Never';
            if (ms < 1000) return `${ms}ms`;
            if (ms < 60000) return `${(ms / 1000).toFixed(1)}s`;
            return `${Math.floor(ms / 60000)}m ${Math.floor((ms % 60000) / 1000)}s`;
        },

        // =========================================================================
        // COMPUTED PROPERTIES
        // =========================================================================

        // Computed: formatted uptime
        get uptimeFormatted() {
            return this.formatUptime(this.uptime);
        },

        // Computed: formatted RAM
        get freeRamFormatted() {
            return this.formatBytes(this.freeRam);
        },

        // Computed: formatted total traffic
        get totalTrafficFormatted() {
            return this.formatBytes(this.totalTraffic);
        },

        // Computed: board name from app store
        get boardName() {
            const boardType = Alpine.store('app').boardType || 's3zero';
            const names = {
                'xiao': 'XIAO ESP32-S3',
                's3supermini': 'ESP32-S3 Super Mini',
                'minikit': 'ESP32 MiniKit',
                'minikit_bt': 'ESP32 MiniKit BT',
                'minikit_ble': 'ESP32 MiniKit BLE',
                's3zero': 'ESP32-S3-Zero',
                's3zero_ble': 'ESP32-S3-Zero BLE'
            };
            return names[boardType] || 'ESP32-S3-Zero';
        },

        // Computed: WiFi mode text
        get wifiModeText() {
            const app = Alpine.store('app');
            const tempIcon = this.tempNetworkMode ? ' ⏳' : '';
            // Client mode
            if (app.wifiMode === '1') {
                if (this.wifiClientConnected) {
                    return `Client (${this.connectedSSID || 'Unknown'})${tempIcon}`;
                }
                const primaryNetwork = app.wifiNetwork0Ssid || 'N/A';
                return `Client (Searching: ${primaryNetwork})${tempIcon}`;
            }
            // AP mode (SSID = hostname)
            if (this.tempNetworkMode) {
                return `Temporary AP (${app.mdnsHostname}) ⏳`;
            }
            return `Access Point (${app.mdnsHostname})`;
        },

        // Computed: show IP row (client mode connected)
        get showIpAddress() {
            return Alpine.store('app').wifiMode === '1' && this.wifiClientConnected;
        },

        // Activity indicators
        get device1RxActive() { return this.device1Rx > this._prev.device1Rx; },
        get device1TxActive() { return this.device1Tx > this._prev.device1Tx; },
        get device2RxActive() { return this.device2Rx > this._prev.device2Rx; },
        get device2TxActive() { return this.device2Tx > this._prev.device2Tx; },
        get device3RxActive() { return this.device3Rx > this._prev.device3Rx; },
        get device3TxActive() { return this.device3Tx > this._prev.device3Tx; },
        get device4RxActive() { return this.device4RxBytes > this._prev.device4RxBytes; },
        get device4TxActive() { return this.device4TxBytes > this._prev.device4TxBytes; },
        get device5RxActive() { return this.device5RxBytes > this._prev.device5RxBytes; },
        get device5TxActive() { return this.device5TxBytes > this._prev.device5TxBytes; },

        // Start polling
        startPolling() {
            this.fetchStatus();
            this.pollInterval = setInterval(() => this.fetchStatus(), 5000);
        },

        // Stop polling
        stopPolling() {
            if (this.pollInterval) {
                clearInterval(this.pollInterval);
                this.pollInterval = null;
            }
        },

        // Fetch status from API
        async fetchStatus() {
            try {
                const response = await fetch('/api/status');
                if (!response.ok) return;
                const data = await response.json();

                // Save previous values for activity indicators
                this._prev.device1Rx = this.device1Rx;
                this._prev.device1Tx = this.device1Tx;
                this._prev.device2Rx = this.device2Rx;
                this._prev.device2Tx = this.device2Tx;
                this._prev.device3Rx = this.device3Rx;
                this._prev.device3Tx = this.device3Tx;
                this._prev.device4RxBytes = this.device4RxBytes;
                this._prev.device4TxBytes = this.device4TxBytes;
                this._prev.device5RxBytes = this.device5RxBytes;
                this._prev.device5TxBytes = this.device5TxBytes;

                this.uptime = data.uptime ?? 0;
                this.freeRam = data.freeRam ?? 0;

                // WiFi
                this.wifiClientConnected = data.wifiClientConnected ?? false;
                this.connectedSSID = data.connectedSSID || '';
                this.ipAddress = data.ipAddress || '';
                this.rssiPercent = data.rssiPercent ?? 0;
                this.tempNetworkMode = data.tempNetworkMode ?? false;

                // BT
                this.btInitialized = data.btInitialized ?? false;
                this.btConnected = data.btConnected ?? false;

                // UART
                this.uartConfig = data.uartConfig || '';
                this.flowControl = data.flowControl || '';

                // Device role names
                this.device1RoleName = data.device1RoleName || '-';
                this.device2RoleName = data.device2RoleName || '-';
                this.device3RoleName = data.device3RoleName || '-';
                this.device4RoleName = data.device4RoleName || '-';
                this.device5RoleName = data.device5RoleName || '-';

                // Device roles (numeric)
                this.device2Role = parseInt(data.device2Role) || 0;
                this.device3Role = parseInt(data.device3Role) || 0;
                this.device4Role = parseInt(data.device4Role) || 0;
                this.device5Role = parseInt(data.device5Role) || 0;

                // Device stats
                this.device1Rx = data.device1Rx ?? 0;
                this.device1Tx = data.device1Tx ?? 0;
                this.device2Rx = data.device2Rx ?? 0;
                this.device2Tx = data.device2Tx ?? 0;
                this.device3Rx = data.device3Rx ?? 0;
                this.device3Tx = data.device3Tx ?? 0;

                // Device 4
                this.device4TxBytes = data.device4TxBytes ?? 0;
                this.device4TxPackets = data.device4TxPackets ?? 0;
                this.device4RxBytes = data.device4RxBytes ?? 0;
                this.device4RxPackets = data.device4RxPackets ?? 0;

                // Device 5 (Bluetooth)
                this.device5TxBytes = data.device5TxBytes ?? 0;
                this.device5RxBytes = data.device5RxBytes ?? 0;

                // Total
                this.totalTraffic = data.totalTraffic ?? 0;
                this.lastActivity = data.lastActivity || 'Never';

                // UDP batching
                this.udpBatchingEnabled = data.udpBatchingEnabled ?? false;

                // Protocol stats (if present)
                if (data.protocolStats) {
                    this.protocolStats = data.protocolStats;
                    // Extract UDP batching stats
                    if (data.protocolStats.udpBatching) {
                        this.udpBatchingStats = data.protocolStats.udpBatching;
                    }
                }
            } catch (err) {
                console.error('Failed to fetch status:', err);
            }
        },

        // Reset statistics
        async resetStatistics() {
            try {
                const response = await fetch('/reset_stats');
                if (!response.ok) throw new Error('Reset failed');
                const data = await response.json();
                if (data.status !== 'ok') throw new Error(data.message || 'Reset failed');

                // Reset local values
                this.device1Rx = 0;
                this.device1Tx = 0;
                this.device2Rx = 0;
                this.device2Tx = 0;
                this.device3Rx = 0;
                this.device3Tx = 0;
                this.device4RxBytes = 0;
                this.device4TxBytes = 0;
                this.device4RxPackets = 0;
                this.device4TxPackets = 0;
                this.device5RxBytes = 0;
                this.device5TxBytes = 0;
                this.totalTraffic = 0;
                this.lastActivity = 'Never';

                // Clear logs
                this.logs = ['Statistics and logs cleared'];

                // Force immediate update
                setTimeout(() => this.fetchStatus(), 100);
                return true;
            } catch (err) {
                console.error('Reset error:', err);
                throw err;
            }
        },

        // =========================================================================
        // LOGS METHODS
        // =========================================================================

        // Fetch logs from API
        async fetchLogs() {
            try {
                const response = await fetch('/logs');
                if (!response.ok) return;
                const data = await response.json();
                this.logs = data.logs || [];
                this.logsLoaded = true;
            } catch (err) {
                console.error('Failed to fetch logs:', err);
            }
        },

        // Start logs polling (when section opened)
        startLogsPolling() {
            if (this.logsPollInterval) return;  // Already polling
            this.fetchLogs();
            this.logsPollInterval = setInterval(() => this.fetchLogs(), 5000);
        },

        // Stop logs polling (when section closed)
        stopLogsPolling() {
            if (this.logsPollInterval) {
                clearInterval(this.logsPollInterval);
                this.logsPollInterval = null;
            }
        },

        // Copy logs to clipboard
        copyLogs() {
            if (!this.logs || this.logs.length === 0) return;
            const text = this.logs.join('\n');
            if (!text || text === 'Loading logs...' || text === 'No logs available') return;

            // Use modern clipboard API if available
            if (navigator.clipboard && navigator.clipboard.writeText) {
                navigator.clipboard.writeText(text).catch(err => {
                    console.error('Clipboard write failed:', err);
                    this._fallbackCopy(text);
                });
            } else {
                this._fallbackCopy(text);
            }
        },

        // Fallback copy method for older browsers
        _fallbackCopy(text) {
            const textArea = document.createElement('textarea');
            textArea.value = text;
            textArea.style.position = 'fixed';
            textArea.style.left = '-9999px';
            document.body.appendChild(textArea);
            textArea.select();
            try {
                document.execCommand('copy');
            } catch (err) {
                console.error('Fallback copy failed:', err);
            }
            document.body.removeChild(textArea);
        }
    });

    // =========================================================================
    // SBUS STORE - SBUS routing state (polled if SBUS active)
    // =========================================================================
    Alpine.store('sbus', {
        // Polling
        pollInterval: null,

        // State
        mode: 0,            // 0=AUTO, 1=MANUAL
        state: 0,           // 0=OK, 1=HOLD, 2=FAILSAFE
        activeSource: 0,
        sources: [],
        framesRouted: 0,
        repeatedFrames: 0,

        // Computed: mode text
        get modeText() {
            return this.mode === 0 ? 'AUTO' : 'MANUAL';
        },

        // Computed: state text
        get stateText() {
            return ['OK', 'HOLD', 'FAILSAFE'][this.state] || 'UNKNOWN';
        },

        // Computed: state CSS class
        get stateClass() {
            return ['sbus-ok', 'sbus-hold', 'sbus-failsafe'][this.state] || '';
        },

        // Computed: configured sources
        get configuredSources() {
            return this.sources.filter(s => s.configured);
        },

        // Computed: show source selection (2+ inputs)
        get showSourceSelection() {
            return this.configuredSources.length >= 2;
        },

        // Start polling
        startPolling() {
            this.fetchStatus();
            this.pollInterval = setInterval(() => this.fetchStatus(), 5000);
        },

        // Stop polling
        stopPolling() {
            if (this.pollInterval) {
                clearInterval(this.pollInterval);
                this.pollInterval = null;
            }
        },

        // Fetch SBUS status
        async fetchStatus() {
            try {
                const response = await fetch('/sbus/status');
                if (!response.ok) return;
                const data = await response.json();

                this.mode = data.mode ?? 0;
                this.state = data.state ?? 0;
                this.activeSource = data.activeSource ?? 0;
                this.sources = data.sources || [];
                this.framesRouted = data.framesRouted ?? 0;
                this.repeatedFrames = data.repeatedFrames ?? 0;
            } catch (err) {
                console.error('Failed to fetch SBUS status:', err);
            }
        },

        // Set SBUS source
        async setSource(sourceId) {
            try {
                const response = await fetch(`/sbus/set_source?source=${sourceId}`);
                if (response.ok) {
                    this.fetchStatus();
                }
            } catch (err) {
                console.error('Failed to set SBUS source:', err);
            }
        },

        // Set SBUS mode (AUTO/MANUAL)
        async setMode(newMode) {
            try {
                const response = await fetch(`/sbus/set_mode?mode=${newMode}`);
                if (response.ok) {
                    this.mode = newMode;
                    this.fetchStatus();
                }
            } catch (err) {
                console.error('Failed to set SBUS mode:', err);
            }
        },

        // Toggle mode
        toggleMode() {
            this.setMode(this.mode === 0 ? 1 : 0);
        }
    });

    // =========================================================================
    // CRASH STORE - Crash log state
    // =========================================================================
    Alpine.store('crash', {
        count: 0,
        entries: [],
        loaded: false,
        expandedIndex: null,

        // Computed: badge color
        get badgeColor() {
            if (this.count === 0) return '#4CAF50';  // Green
            if (this.count <= 5) return '#ff9800';   // Orange
            return '#f44336';                         // Red
        },

        // Computed: show clear button
        get showClearButton() {
            return this.count > 0;
        },

        // Fetch crash count only (for badge)
        async fetchCount() {
            try {
                const response = await fetch('/crashlog_json');
                if (!response.ok) return;
                const data = await response.json();
                this.count = data.total || 0;
            } catch (err) {
                console.log('Could not load crash count');
            }
        },

        // Fetch full crash data (when section opened)
        async fetchAll() {
            try {
                const response = await fetch('/crashlog_json');
                if (!response.ok) return;
                const data = await response.json();
                this.count = data.total || 0;
                this.entries = data.entries || [];
                this.loaded = true;
            } catch (err) {
                console.error('Failed to fetch crash log:', err);
            }
        },

        // Clear crash history
        async clear() {
            try {
                const response = await fetch('/clear_crashlog');
                if (!response.ok) throw new Error('Clear failed');
                const data = await response.json();
                if (data.status !== 'ok') throw new Error('Clear operation failed');
                this.count = 0;
                this.entries = [];
                this.expandedIndex = null;
                return true;
            } catch (err) {
                console.error('Clear crash log error:', err);
                throw err;
            }
        },

        // Toggle expanded panic details for a row
        toggleDetails(index) {
            this.expandedIndex = this.expandedIndex === index ? null : index;
        },

        // CSS class for crash type
        typeClass(reason) {
            if (reason === 'PANIC') return 'crash-panic';
            if (reason.includes('WDT')) return 'crash-wdt';
            return 'crash-other';
        },

        // Format crash reason for display
        formatReason(reason) {
            const map = {
                'PANIC': 'Panic', 'TASK_WDT': 'Task WDT', 'INT_WDT': 'Int WDT',
                'SW_RESET': 'SW Reset', 'POWERON': 'Power On',
                'BROWNOUT': 'Brownout', 'DEEPSLEEP': 'Deep Sleep'
            };
            return map[reason] || reason;
        },

        // Format epoch timestamp
        formatTime(epoch) {
            if (!epoch || epoch === 0) return '\u2014';
            const d = new Date(epoch * 1000);
            const pad = n => String(n).padStart(2, '0');
            return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}`;
        },

        // ESP32 exception cause description
        getExcauseDescription(excause) {
            const causes = {
                '0x00000000': '(IllegalInstruction)',
                '0x00000001': '(Syscall)',
                '0x00000002': '(InstructionFetchError)',
                '0x00000003': '(LoadStoreError)',
                '0x00000004': '(Level1Interrupt)',
                '0x00000005': '(Alloca)',
                '0x00000006': '(IntegerDivideByZero)',
                '0x00000009': '(LoadStoreAlignmentCause)',
                '0x0000000c': '(InstrPIFDataError)',
                '0x0000000d': '(LoadStorePIFDataError)',
                '0x0000000e': '(InstrPIFAddrError)',
                '0x0000000f': '(LoadStorePIFAddrError)',
                '0x00000014': '(InstrTLBMiss)',
                '0x00000018': '(LoadStoreTLBMiss)',
                '0x0000001c': '(LoadProhibited)',
                '0x0000001d': '(StoreProhibited)'
            };
            return causes[excause] || '';
        },

        // Generate panic details HTML (complex block with backtrace, copy buttons)
        panicDetailsHtml(entry) {
            const panic = entry.panic;
            if (!panic || !panic.pc) return '';

            let html = '<div class="panic-details">';
            html += '<div style="font-weight: bold; margin-bottom: 8px;">\u26A0\uFE0F Exception Details:</div>';

            let exceptionText = '\u26A0\uFE0F Exception Details:\n\n';

            if (entry.version) {
                html += '<div style="margin-bottom: 8px; color: #666; font-size: 11px;">';
                html += `Firmware Version: <strong>${entry.version}</strong>`;
                html += '</div>';
                exceptionText += `Firmware Version: ${entry.version}\n`;
            }

            html += '<div style="margin-bottom: 8px; font-family: monospace; font-size: 11px;">';
            html += `<div>PC (Program Counter): <strong>${panic.pc}</strong></div>`;
            exceptionText += `PC (Program Counter): ${panic.pc}\n`;

            if (panic.task) {
                html += `<div>Task: <strong>${panic.task}</strong></div>`;
                exceptionText += `Task: ${panic.task}\n`;
            }
            if (panic.excause) {
                const causeDesc = this.getExcauseDescription(panic.excause);
                html += `<div>Exception Cause: <strong>${panic.excause}</strong> ${causeDesc}</div>`;
                exceptionText += `Exception Cause: ${panic.excause} ${causeDesc}\n`;
            }
            if (panic.excvaddr) {
                html += `<div>Exception Address: <strong>${panic.excvaddr}</strong></div>`;
                exceptionText += `Exception Address: ${panic.excvaddr}\n`;
            }
            html += '</div>';

            if (panic.backtrace && panic.backtrace.length > 0) {
                html += '<div style="margin-top: 10px;">';
                html += `<div style="font-weight: bold; margin-bottom: 5px;">Backtrace (${panic.backtrace.length} addresses):</div>`;
                html += '<div style="font-family: monospace; font-size: 11px; background: white; padding: 8px; border-radius: 4px; word-break: break-all; line-height: 1.6;">';

                exceptionText += `\nBacktrace (${panic.backtrace.length} addresses):\n`;

                for (let i = 0; i < panic.backtrace.length; i += 4) {
                    const group = panic.backtrace.slice(i, i + 4);
                    html += group.join(' ') + '<br>';
                    exceptionText += group.join(' ') + '\n';
                }

                html += '</div></div>';

                if (panic.truncated) {
                    html += '<div style="margin-top: 10px; padding: 8px; background: #fff3cd; border-left: 3px solid #ff9800; font-size: 11px; color: #856404;">';
                    html += '\u26A0\uFE0F <strong>Warning:</strong> Backtrace was truncated at 16 addresses (ESP-IDF limit). Actual call stack may be deeper.';
                    html += '</div>';
                }
                if (panic.corrupted) {
                    html += '<div style="margin-top: 10px; padding: 8px; background: #f8d7da; border-left: 3px solid #f44336; font-size: 11px; color: #721c24;">';
                    html += '\u26A0\uFE0F <strong>Warning:</strong> Backtrace data is corrupted. Results may be unreliable.';
                    html += '</div>';
                }

                const addresses = panic.backtrace.join(' ');
                const exceptionTextEsc = exceptionText.replace(/'/g, "\\'").replace(/\n/g, '\\n');
                html += '<div style="margin-top: 10px; display: flex; gap: 8px; flex-wrap: wrap;">';
                html += `<button data-exception="${exceptionTextEsc}" onclick="Alpine.store('crash').copyException(this.dataset.exception)" style="padding: 6px 12px; font-size: 12px; cursor: pointer; border: 1px solid #2196F3; background: white; color: #1976D2; border-radius: 4px;">&#128203; Copy Exception</button>`;
                html += `<button data-addresses="${addresses}" onclick="Alpine.store('crash').copyCommand(this.dataset.addresses)" style="padding: 6px 12px; font-size: 12px; cursor: pointer; border: 1px solid #4CAF50; background: white; color: #2E7D32; border-radius: 4px;">&#128203; Copy addr2line</button>`;
                html += '</div>';
            }

            html += '</div>';
            return html;
        },

        // Copy exception details to clipboard
        copyException(text) {
            const decoded = text.replace(/\\n/g, '\n').replace(/\\'/g, "'");
            this._copyToClipboard(decoded, 'Exception details copied to clipboard!');
        },

        // Copy addr2line command to clipboard
        copyCommand(addresses) {
            const command = `xtensa-esp32s3-elf-addr2line -pfiaC -e firmware.elf ${addresses}`;
            this._copyToClipboard(command, 'addr2line command copied! Replace firmware.elf with your .elf file path');
        },

        _copyToClipboard(text, message) {
            if (navigator.clipboard && navigator.clipboard.writeText) {
                navigator.clipboard.writeText(text).then(() => {
                    this._showCopyFeedback(message);
                }).catch(() => {
                    this._fallbackCopy(text, message);
                });
            } else {
                this._fallbackCopy(text, message);
            }
        },

        _fallbackCopy(text, message) {
            const textarea = document.createElement('textarea');
            textarea.value = text;
            textarea.style.position = 'fixed';
            textarea.style.opacity = '0';
            document.body.appendChild(textarea);
            textarea.select();
            try {
                document.execCommand('copy');
                this._showCopyFeedback(message);
            } catch (err) {
                alert('Copy failed. Please copy manually.');
            }
            document.body.removeChild(textarea);
        },

        _showCopyFeedback(message) {
            const feedback = document.createElement('div');
            feedback.textContent = message;
            feedback.style.cssText = 'position: fixed; top: 20px; right: 20px; background: #4CAF50; color: white; padding: 12px 20px; border-radius: 4px; z-index: 10000; font-size: 14px; box-shadow: 0 2px 8px rgba(0,0,0,0.2);';
            document.body.appendChild(feedback);
            setTimeout(() => {
                feedback.style.opacity = '0';
                feedback.style.transition = 'opacity 0.3s';
                setTimeout(() => document.body.removeChild(feedback), 300);
            }, 2000);
        }
    });

    // =========================================================================
    // UI STORE - Collapsible sections state (persisted to localStorage)
    // =========================================================================
    Alpine.store('ui', {
        // Collapsible section states (true = open, false = closed)
        // Manual localStorage sync (alpine-persist works differently in stores)
        systemStatus: JSON.parse(localStorage.getItem('ui:systemStatus')) || false,
        protocolStats: JSON.parse(localStorage.getItem('ui:protocolStats')) || false,
        logs: JSON.parse(localStorage.getItem('ui:logs')) || false,
        crash: JSON.parse(localStorage.getItem('ui:crash')) || false,
        wifiConfig: JSON.parse(localStorage.getItem('ui:wifiConfig')) || false,
        advancedConfig: JSON.parse(localStorage.getItem('ui:advancedConfig')) || false,
        configBackup: JSON.parse(localStorage.getItem('ui:configBackup')) || false,
        firmwareUpdate: JSON.parse(localStorage.getItem('ui:firmwareUpdate')) || false,
        additionalNetworks: JSON.parse(localStorage.getItem('ui:additionalNetworks')) || false,

        // Toggle section and trigger side effects
        toggle(section) {
            this[section] = !this[section];

            // Persist to localStorage
            localStorage.setItem('ui:' + section, JSON.stringify(this[section]));

            // Side effects on open
            if (this[section]) {
                if (section === 'logs') {
                    Alpine.store('status').startLogsPolling();
                } else if (section === 'crash') {
                    Alpine.store('crash').fetchAll();
                } else if (section === 'systemStatus' || section === 'protocolStats') {
                    Alpine.store('status').fetchStatus();
                }
            } else {
                // Side effects on close
                if (section === 'logs') {
                    Alpine.store('status').stopLogsPolling();
                }
            }
        }
    });

    // =========================================================================
    // FIRMWARE STORE - Firmware update progress
    // =========================================================================
    Alpine.store('firmware', {
        progress: 0,
        status: '',
        statusColor: 'black',
        showProgress: false,
        buttonDisabled: false,
        buttonText: 'Update Firmware',

        start() {
            this.showProgress = true;
            this.progress = 0;
            this.status = 'Uploading firmware...';
            this.statusColor = 'black';
            this.buttonDisabled = true;
            this.buttonText = 'Updating...';
        },

        setProgress(percent) {
            this.progress = percent;
            this.status = `Uploading firmware... ${Math.round(percent)}%`;
        },

        setSuccess(message) {
            this.progress = 100;
            this.status = message;
            this.statusColor = 'green';
            this.buttonText = 'Rebooting...';
        },

        setError(message) {
            this.progress = 0;
            this.status = message;
            this.statusColor = 'red';
            this.buttonDisabled = false;
            this.buttonText = 'Update Firmware';
        },

        reset() {
            this.buttonDisabled = false;
            this.buttonText = 'Update Firmware';
        },

        // Upload firmware file (XHR for progress tracking)
        upload(file) {
            if (!file) return;
            this.start();

            const formData = new FormData();
            formData.append('update', file);

            const xhr = new XMLHttpRequest();

            xhr.upload.addEventListener('progress', (event) => {
                if (event.lengthComputable) {
                    this.setProgress((event.loaded / event.total) * 100);
                }
            });

            xhr.addEventListener('load', () => {
                const contentType = xhr.getResponseHeader('content-type');
                const isJson = contentType && contentType.includes('application/json');

                if (!isJson || xhr.responseText.startsWith('<')) {
                    this.setSuccess('Firmware uploaded! Device is rebooting...');
                    this._reconnect();
                    return;
                }

                try {
                    const response = JSON.parse(xhr.responseText);
                    if (xhr.status === 200 && response.status === 'ok') {
                        this.setSuccess(response.message);
                        this._reconnect();
                    } else {
                        throw new Error(response.message || 'Update failed');
                    }
                } catch (error) {
                    this.setError('Update failed: ' + error.message);
                }
            });

            xhr.addEventListener('error', () => {
                this.setError('Update failed: Network error');
            });

            xhr.open('POST', '/update');
            xhr.send(formData);
        },

        _reconnect() {
            let countdown = 12;
            const countdownInterval = setInterval(() => {
                this.status = `Device rebooting... ${countdown}s`;
                countdown--;
                if (countdown < 0) {
                    clearInterval(countdownInterval);
                    this._tryReconnect(0, 30);
                }
            }, 1000);
        },

        _tryReconnect(attempt, maxAttempts) {
            if (attempt >= maxAttempts) {
                this.status = 'Please refresh page manually';
                this.statusColor = '#ff9800';
                return;
            }

            this.status = `Reconnecting... (${attempt + 1}/${maxAttempts})`;

            fetch('/api/status', { cache: 'no-store' })
                .then(r => {
                    if (!r.ok) throw new Error('Not ready');
                    this.status = 'Connected! Reloading...';
                    this.statusColor = 'green';
                    setTimeout(() => window.location.reload(), 500);
                })
                .catch(() => {
                    setTimeout(() => this._tryReconnect(attempt + 1, maxAttempts), 1000);
                });
        }
    });

    // =========================================================================
    // BACKUP STORE - Config import/export and factory reset
    // =========================================================================
    Alpine.store('backup', {
        importStatus: '',
        importColor: '',
        showImportStatus: false,
        resetInProgress: false,

        importConfig(file) {
            if (!file) return;
            if (!file.name.toLowerCase().endsWith('.json')) {
                alert('Please select a JSON configuration file');
                return;
            }

            this.showImportStatus = true;
            this.importStatus = 'Importing...';
            this.importColor = 'black';

            const formData = new FormData();
            formData.append('config', file);

            fetch('/config/import', { method: 'POST', body: formData })
                .then(r => r.ok ? r.json() : Promise.reject(new Error('Import failed')))
                .then(data => {
                    if (data.status === 'ok') {
                        this.importStatus = data.message + ' Please refresh the page.';
                        this.importColor = 'green';
                    }
                })
                .catch(error => {
                    this.importStatus = 'Import failed: ' + error.message;
                    this.importColor = 'red';
                });
        },

        factoryReset() {
            if (!confirm('Reset all settings to factory defaults?\n\nThis will erase all configuration including WiFi credentials.\nDevice will reboot after reset.')) {
                return;
            }

            this.resetInProgress = true;

            const app = Alpine.store('app');
            const apName = app?.defaultHostname || app?.mdnsHostname || 'esp-bridge-XXXX';

            const onReset = () => {
                alert(`Factory reset complete. Device is rebooting...\n\nReconnect to WiFi: ${apName}`);
            };

            Utils.fetchWithReboot('/config/reset', { method: 'POST' })
                .then(data => { if (data.status === 'ok') onReset(); })
                .catch(error => {
                    if (error.isReboot) { onReset(); return; }
                    alert('Factory reset failed: ' + error.message);
                    this.resetInProgress = false;
                });
        }
    });

});

// =========================================================================
// INITIALIZATION
// =========================================================================
document.addEventListener('DOMContentLoaded', () => {
    // Wait for Alpine to initialize stores
    document.addEventListener('alpine:initialized', () => {
        // Load config
        Alpine.store('app').init();

        // Start status polling
        Alpine.store('status').startPolling();

        // Fetch initial crash count for badge
        Alpine.store('crash').fetchCount();

        // Periodic crash count updates (every 5s)
        setInterval(() => Alpine.store('crash').fetchCount(), 5000);

        // SBUS polling: start/stop reactively when isSbusActive changes
        Alpine.effect(() => {
            const active = Alpine.store('app').isSbusActive;
            const sbus = Alpine.store('sbus');
            if (active && !sbus.pollInterval) {
                sbus.startPolling();
            } else if (!active && sbus.pollInterval) {
                sbus.stopPolling();
            }
        });

        // Restore side effects for persisted open sections
        // Use setTimeout to ensure Alpine x-collapse has finished rendering DOM
        setTimeout(() => {
            const ui = Alpine.store('ui');
            if (ui.logs) {
                Alpine.store('status').startLogsPolling();
            }
            if (ui.crash) {
                Alpine.store('crash').fetchAll();
            }
        }, 300);
    });
});

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    if (Alpine.store('status')) {
        Alpine.store('status').stopPolling();
        Alpine.store('status').stopLogsPolling();
    }
    if (Alpine.store('sbus')) {
        Alpine.store('sbus').stopPolling();
    }
});

