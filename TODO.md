# TODO / Roadmap

## Priority 1 - USB Host Mode Support

- [ ] **USB Host Mode Support**
  - Add USB Host mode for modem connection
  - Device/Host/Auto mode selection
  - Web interface for USB mode configuration
  - Hybrid Arduino + ESP-IDF implementation
  - Auto-detection of connected device type
  - Minimal changes to existing UART bridge logic

## Priority 2 - Code Optimization & Diagnostics Refactoring

- [ ] **UART Performance Optimization & Diagnostics System**
  - Implement configurable diagnostics levels (compile-time flag vs web interface option)
  - Optimize UART task for production use while maintaining debugging capabilities
  - Balance between minimal overhead and useful diagnostics
  - Consider web-configurable diagnostic levels instead of compile flags
  - Profile and optimize critical data path for high-speed operation

- [ ] **Code Refactoring & Size Reduction**
  - Reduce size of main.cpp, uartbridge.cpp, and html_main_page.h
  - Consider splitting into logical modules vs combining small related files
  - Extract button handling logic from main.cpp
  - Separate JavaScript from HTML templates
  - Clean up redundant code and optimize memory usage

- [ ] **Button Handler Module**
  - Extract button logic from main.cpp (~100 lines)
  - Support for additional patterns:
    - Double-click actions
    - Different hold durations
    - Combination patterns

- [ ] **Code Cleanup - Old TODOs**
  - Search for TODO comments previously located in defines.h
  - Review and address or remove outdated TODO items
  - Consolidate all TODOs in this file

## Priority 3 - Enhanced Features

- [ ] **High-Speed UART Optimization** *(For 921600+ baud rates)*
  - Increase adaptive buffer size to 512 bytes
  - Consider DMA implementation (requires ESP-IDF)
  - Profile and optimize critical path
  - Note: Current implementation works well up to 460800 baud

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
  - Required when expanding beyond ESP32-S3-Zero
  - Pin validation to prevent conflicts

## Priority 4 - Future Features

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
  - Store preference in config.json
  - CSS variables for easy theme switching