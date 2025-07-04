# TODO / Roadmap

## Priority 1 - Core Stability

- [ ] **Watchdog Timer Protection**
  - Prevent system hang on Serial.write() blocking
  - Critical for high-speed operations (921600+)
  - Implement esp_task_wdt_add() for UART task
  - Auto-recovery from stuck states

## Priority 2 - Enhanced Features

- [ ] **WiFi Mode Management**
  - "Persistent WiFi mode" checkbox in web interface
  - Option to disable automatic WiFi timeout
  - Manual WiFi on/off control buttons
  - Save WiFi mode preference to config.json

- [ ] **MAVLink Protocol Parsing Mode** *(Optional Advanced Feature)*
  - Add protocol detection and parsing
  - Forward complete MAVLink packets without fragmentation
  - Only beneficial at speeds ≥460800 baud
  - Potential performance gain up to 20%
  - Note: Current adaptive buffering already achieves 95%+ efficiency

- [ ] **Configurable GPIO Pins**
  - Web interface for pin selection (TX/RX/CTS/RTS)
  - Support different ESP32 variants and development boards
  - Save pin configuration to config.json
  - Required when expanding beyond ESP32-C3 SuperMini
  - Pin validation to prevent conflicts

## Priority 3 - Future Features

- [ ] **Alternative Data Transmission Modes**
  - **UDP Forwarding** - UART data over WiFi UDP packets
    - Configure target IP and port
    - Useful for remote debugging
  - **TCP Server Mode** - Accept TCP connections for UART access
    - Multiple client support
    - Connection management
  - **WebSocket Streaming** - Real-time data in browser
    - JavaScript client example
    - Data visualization possibilities
  - Allow UART bridge to work simultaneously with WiFi

- [ ] **SBUS Protocol Support**
  - SBUS input mode (RC receiver → USB)
  - SBUS output mode (USB → RC servos)
  - Configuration: 100000 baud, 8E2, inverted
  - Auto-detection of SBUS data
  - Popular in RC/drone applications
  - Extends compatibility with flight controllers

- [ ] **Dark Theme for Web Interface**
  - Toggle button in web interface
  - Store preference