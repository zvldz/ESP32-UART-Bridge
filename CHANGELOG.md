# CHANGELOG

## v2.15.7 (LED System Refactoring & Optimization) ✅ COMPLETED

### LED System Complete Refactoring ✅ COMPLETED
- **Code Size Reduction**: leds.cpp reduced from 600 to 462 lines (-138 lines, 23% reduction)
- **Architecture Unification**: Replaced 6 separate blinking systems with 1 universal system
  - **Old System**: Individual functions for WiFi, Ethernet, UART1-4 LED blinking
  - **New System**: Single `BlinkState` structure array with `BlinkType` enum
  - **State Management**: Centralized blinking logic in unified `updateLED()` function
- **Data Structure Optimization**: Reduced from 40+ individual variables to 5 structured elements
  - **BlinkState Structure**: `active`, `isOn`, `count`, `onTime`, `offTime`, `colorValue`, `nextTime`
  - **BlinkType Enum**: `BLINK_WIFI`, `BLINK_ETH`, `BLINK_UART1-4` for type identification
  - **State Array**: `blinkStates[6]` for all LED types with proper initialization

### Performance & Safety Improvements ✅ COMPLETED
- **Mutex Optimization**: Single mutex capture per update cycle instead of multiple captures
- **Time Handling**: Proper `millis()` overflow handling with safe time arithmetic
- **Code Cleanup**: Removed unused functions and duplicate logic
  - **Removed**: `setLED()` function (-9 lines)
  - **Eliminated**: Counter update duplication (-6 lines)
- **Volatile Warning Fix**: Replaced deprecated `volatile--` with explicit assignment

### Code Quality Enhancements ✅ COMPLETED
- **Maintainability**: Easy addition of new blink patterns through enum extension
- **Readability**: Clear separation of state management and LED control logic
- **Safety**: Proper volatile variable handling and thread-safe operations
- **Efficiency**: Optimized update loop with early returns and reduced function calls

### WiFi Manager Code Refactoring & Style Improvements ✅ COMPLETED
- **Code Size Reduction**: wifi_manager.cpp reduced from 586 to 538 lines (-48 lines, 8.2% reduction)
- **Function Naming**: Renamed all functions from snake_case to camelCase for consistency
  - **Old**: `wifi_manager_init()`, `wifi_manager_process()`, `wifi_manager_is_ready_for_data()`
  - **New**: `wifiInit()`, `wifiProcess()`, `wifiIsReady()`
  - **Updated**: 8 function declarations + implementations + 8 external call sites
- **Constants Extraction**: Replaced magic numbers with named constants
  - **WiFi Constants**: `WIFI_RECONNECT_DELAY_MS`, `WIFI_TX_POWER_LEVEL`, `WIFI_MIN_HEAP_BYTES`
  - **Buffer Sizes**: `WIFI_SSID_MAX_LEN`, `WIFI_PASSWORD_MAX_LEN`, `WIFI_CONNECT_TIMEOUT_MS`
  - **Utility**: `WIFI_MDNS_SERVICE_PORT`, `WIFI_MAC_SUFFIX_BUFFER_SIZE`

### Code Quality & Maintainability Improvements ✅ COMPLETED
- **Error Handling Unification**: Created helper functions for consistent error reporting
  - **ESP-IDF Errors**: `handleEspError()` function replaced 6 duplicate error handling blocks
  - **mDNS Errors**: `logMdnsError()` function unified mDNS error reporting
  - **Credentials Helper**: `setWifiCredentials()` function replaced 4 duplicate strncpy operations
- **Legacy Code Cleanup**: Removed unused and historical code elements
  - **Removed**: `wifiIsConnected()` legacy function (unused, 5 lines saved)
  - **Cleaned**: Historical comments about code migration and architecture changes
  - **Fixed**: Misleading constant name `WIFI_TX_POWER_DBM` → `WIFI_TX_POWER_LEVEL`
- **Variable Naming**: Improved variable names for better code readability

### Flow Control System Complete Refactoring ✅ COMPLETED
- **Architecture Simplification**: Replaced complex auto-detection system with simple ESP-IDF implementation
  - **Removed Files**: `flow_control.cpp` and `flow_control.h` (complex detection logic eliminated)
  - **Removed Globals**: `FlowControlStatus` structure and global variable from `main.cpp`
  - **Simplified Logic**: Direct ESP-IDF `UART_HW_FLOWCTRL_CTS_RTS` usage instead of custom detection
- **Object-Oriented Flow Control**: Integrated flow control status into UART interface
  - **New Method**: `getFlowControlStatus()` virtual method in `UartInterface` base class
  - **UART Implementation**: Flow control status reporting through `UartDMA` class
  - **Web Integration**: Updated `web_api.cpp` to use UART interface method instead of global function
- **Hardware Configuration**: Flow control enabled only for UART1 with proper GPIO configuration
  - **GPIO Pins**: RTS=GPIO6, CTS=GPIO7 as defined in hardware specification
  - **Threshold**: RX flow control threshold set to 100 bytes (~78% of 128-byte FIFO)
  - **User Control**: Flow control activated only when explicitly enabled via web interface
- **Build System Fixes**: Resolved compilation issues from previous refactoring
  - **Header Dependencies**: Added `#include <Arduino.h>` to `uart_interface.h` for `String` type
  - **Dead Code Removal**: Cleaned up remaining references to `globalTxBytesCounter` in protocol headers
  - **Incremental Build**: Fixed issues where header-only changes weren't properly recompiled
  - **Network Detection**: `foundNow` → `networkFoundNow`
  - **IP Address**: `ip_str` → `ipAddressStr` 
  - **WiFi Config**: `cfg` → `wifiConfig`
- **Micro-optimizations**: Small performance improvements
  - **Removed**: Duplicate extern declarations
  - **Optimized**: Cached hostname length in validation loop

### Web Interface Code Optimization & Cleanup ✅ COMPLETED
- **Code Size Reduction**: Combined optimization of web API and interface modules
  - **web_api.cpp**: Minor cleanup with constants for magic numbers and historical comment removal
  - **web_interface.cpp**: Reduced from 211 to 192 lines (-19 lines, 9% reduction)
- **HTTP Response Optimization**: Created helper function for gzipped static file serving
  - **Before**: 7 identical 3-line blocks for CSS/JS files (21 lines total)
  - **After**: Single `sendGzippedResponse()` helper + 7 one-line calls (13 lines total)
  - **Eliminated Duplication**: Response creation, gzip header, and sending logic unified
- **Magic Numbers → Named Constants**: Improved code readability
  - **HTTP_PORT (80)**: Web server port configuration
  - **UPLOAD_BUFFER_RESERVE (4096)**: File upload buffer pre-allocation size  
  - **ASCII_PRINTABLE_THRESHOLD (32)**: Character filtering threshold for uploads
- **Code Quality Improvements**: Removed historical references and unnecessary comments
  - **Cleaned**: Architecture change comments and implementation history notes
  - **Simplified**: Function documentation to focus on current behavior rather than past changes

### Task Scheduler Code Cleanup ✅ COMPLETED
- **Historical Comments Removal**: Cleaned up scheduler_tasks.cpp from legacy references
  - **Removed 10+ comments**: All "Statistics update tasks removed" and similar historical notes
  - **Focused Documentation**: Code comments now describe current functionality rather than past architecture changes
  - **Cleaner Codebase**: Eliminated references to removed statistics update tasks and deprecated functionality

## v2.15.6 (Types System Refactoring & Code Organization) ✅ COMPLETED

### Major Code Architecture Refactoring ✅ COMPLETED
- **Types.h Modularization**: Split large types.h file into logical modules
  - **File Reduction**: types.h reduced from ~400 to ~264 lines for faster compilation
  - **Logical Separation**: Device types, statistics, and core types properly separated
  - **Better Organization**: Clear modular structure by functionality
  - **Compilation Speed**: Changes to device statistics no longer recompile entire project

### New Modular Header Structure ✅ COMPLETED
- **device_stats.h**: Device statistics structure and management functions
  - **DeviceStatistics Structure**: Atomic counters for all 4 devices (Device1-4)
  - **Global Instance**: `extern DeviceStatistics g_deviceStats` declaration
  - **Management Functions**: `initDeviceStatistics()` and `resetDeviceStatistics()` 
- **device_stats.cpp**: Implementation of statistics functions and global instance
- **device_types.h**: Device roles, enums, and configuration structures
  - **Device Role Enums**: Device1Role, Device2Role, Device3Role, Device4Role
  - **Core Enums**: BridgeWiFiMode, UsbMode, LogLevel (moved from types.h)
  - **Config Structure**: Complete configuration structure with proper enum types
- **types.h Simplified**: Core system types only
  - **Essential Types**: BridgeMode, WiFiClientState, LedMode, SystemState
  - **BridgeContext**: Protocol pipeline context (maintained for compatibility)
  - **Include Integration**: Automatically includes new modular headers

### Cross-Core Communication Cleanup ✅ COMPLETED  
- **Removed Inter-Core Hacks**: Eliminated unnecessary cross-core synchronization
  - **Global Pipeline Pointer**: Removed `g_pipeline` global variable
  - **Memory Barriers**: Removed `__sync_synchronize()` calls
  - **External Declarations**: Cleaned up bridge_processing.h
- **Legacy Buffer Methods**: Removed unused cross-core communication methods
  - **writeWithSource()**: Removed from circular_buffer.h
  - **readWithSource()**: Removed from circular_buffer.h  
  - **writeInternal()**: Removed helper method
- **Architecture Simplification**: All components now run on Core 0, no sync needed

### Dead Code Elimination ✅ COMPLETED
- **Statistics Update Functions**: Removed empty update functions
  - **Diagnostics Cleanup**: Removed `updateMainStats()`, `updateDevice3Stats()`, `updateDevice4Stats()`
  - **Scheduler Tasks**: Removed unused statistics update tasks from TaskScheduler
  - **Task References**: Cleaned up all enable/disable/addTask calls for removed tasks

### Configuration Management Refactoring ✅ COMPLETED
- **Code Quality Improvements**: Enhanced maintainability and reduced duplication in config.cpp
- **Constants Extraction**: Replaced magic numbers with named constants
  - **Network**: `DEFAULT_BAUDRATE` (115200), `DEFAULT_UDP_PORT` (14560)
  - **Processing**: `JSON_PREVIEW_SIZE` (200), `IP_BUFFER_SIZE` (15)
- **Helper Functions**: Created utility functions to eliminate code duplication
  - **Migration Helper**: `finalizeMigration()` replaced 7 duplicate log+version blocks (-14 lines)
  - **Device Defaults**: `setDeviceDefaults()` replaced 2 duplicate device role assignments (-6 lines)
- **Code Cleanup**: Removed historical comments and improved consistency
  - **Removed**: "NEW" markers from UDP batching implementation
  - **Enhanced**: Protocol constant usage (`0` → `PROTOCOL_NONE` for better readability)
- **Scheduler Task Cleanup**: Removed empty if-blocks and historical comments from scheduler_tasks.cpp
  - **Eliminated**: 4 empty Device3/Device4 if-blocks (legacy after refactoring)
  - **Cleaned**: 10+ historical comments about removed statistics functions

### Crash Logging System Refactoring ✅ COMPLETED
- **Code Quality Improvements**: Enhanced maintainability and reduced duplication in crashlog.cpp
- **Helper Functions**: Created utility functions to eliminate code duplication
  - **Empty JSON Structure**: `createEmptyLog()` helper replaced 5 duplicate initialization blocks
  - **Fallback JSON**: `getFallbackJson()` helper replaced 3 duplicate error return strings
  - **File Writing**: `writeJsonToFile()` helper replaced 3 duplicate file write operations
- **Constants**: Added `CRASHLOG_MAX_FILE_SIZE` constant for better code readability
- **Code Cleanup**: Improved consistency and maintainability throughout crash logging system

### Legacy Code and Comment Cleanup ✅ COMPLETED
- **Historical Comments Removal**: Cleaned up outdated comments throughout codebase
- **Files Updated**: adaptive_buffer.h, bridge_processing.h, circular_buffer.h, uartbridge.cpp/h
- **Legacy Code Removal**: Removed unused enum DataSource from circular_buffer.h
- **Comment Consistency**: Simplified technical comments while preserving TODO markers and diagnostics

### Protocol Pipeline API Cleanup ✅ COMPLETED
- **Legacy Parameter Removal**: Eliminated unused txCounter parameter from all PacketSender constructors
- **Dead Code Elimination**: Removed updateTxCounter() helper functions and globalTxBytesCounter field
- **API Simplification**: Simplified constructor calls by removing nullptr parameters across pipeline
- **Constants Extraction**: Replaced magic numbers with named constants for buffer sizes
  - **DEFAULT_MAX_PACKETS** (20), **DEFAULT_MAX_BYTES** (8192) for UART/UDP senders
  - **USB_MAX_PACKETS** (128), **USB_MAX_BYTES** (24576) for USB sender optimized buffers
- **Code Quality**: Improved maintainability and self-documenting code through consistent naming
- **Project Structure**: Moved device_init files to src root
  - **Simplified Paths**: `devices/device_init.h` → `device_init.h` 
  - **Include Updates**: Fixed all include paths throughout codebase
  - **Directory Cleanup**: Removed empty devices/ directory

### Forward Declaration Conflicts Resolution ✅ COMPLETED
- **Enum Type Conflicts**: Fixed forward declaration vs full definition mismatches
  - **BridgeWiFiMode, UsbMode, LogLevel**: Moved complete definitions to device_types.h
  - **Removed Conflicts**: Eliminated forward declarations with base types
  - **Proper Dependencies**: Maintained correct include order through types.h
- **Include Path Optimization**: Updated all files requiring new headers
  - **main.cpp**: Added device_stats.h include, updated initialization
  - **web_api.cpp**: Added device_stats.h include for statistics access
  - **uartbridge.cpp**: Removed duplicate statistics initialization

### USB Host Code Style Refactoring ✅ COMPLETED
- **Class Structure**: Created proper usb_host.h header file with class declarations
- **Method Decomposition**: Split large `handleDeviceConnection()` into focused methods
  - **openDevice()**: Device opening and validation (8 lines)
  - **getDeviceInfo()**: Device information retrieval (17 lines)
  - **claimInterface()**: Interface claiming with error handling (12 lines)
  - **setupTransfers()**: USB transfer setup and initialization (23 lines)
- **CDC Interface Refactoring**: Improved `findCDCInterface()` method structure
  - **findBulkEndpoints()**: Separated endpoint discovery logic (44 lines)
  - **Better Error Handling**: Early returns instead of deep nesting
  - **Enhanced Logging**: Detailed debug information for USB operations
- **Naming Consistency**: Updated all methods to match project camelCase style
  - **Task Methods**: `usb_host_task` → `usbHostTask`
  - **Callbacks**: `client_event_callback` → `clientEventCallback`
  - **Variables**: `bulk_in_ep` → `bulk_in_endpoint`
- **Documentation**: Added comprehensive method comments and parameter descriptions

### Technical Benefits 🚀
- **Faster Compilation**: Modular headers reduce interdependencies
- **Cleaner Architecture**: Clear separation of concerns across modules  
- **Better Maintainability**: Smaller focused files easier to understand and modify
- **Reduced Memory Usage**: Eliminated unused tasks, variables, and functions
- **Improved Code Quality**: Consistent style and better error handling throughout
- **Lock-free Performance**: Removed unnecessary synchronization primitives

## v2.15.5 (Statistics & LED System Refactoring) ✅ COMPLETED

### Major Architecture Refactoring ✅ COMPLETED
- **Unified Device Statistics**: Complete migration to atomic-based statistics system
  - **Atomic Operations**: Replaced critical sections with `std::atomic<unsigned long>` and `memory_order_relaxed`
  - **Global Statistics**: Centralized `DeviceStatistics g_deviceStats` for all 4 devices (Device1-4)
  - **Thread Safety**: Eliminated spinlocks and critical sections for lock-free statistics
  - **Memory Reduction**: Removed task-local counters and UartStats structure
  - **Simplified Architecture**: Direct atomic increments throughout the codebase

### Device Statistics Centralization ✅ COMPLETED
- **DeviceCounter Structure**: Standardized RX/TX byte counters for all devices
  - **Device1**: UART1 primary interface statistics
  - **Device2**: USB/UART2 secondary interface statistics  
  - **Device3**: UART3 mirror/bridge interface statistics
  - **Device4**: UDP network interface statistics
- **System-wide Counters**: Added global activity tracking and system start time
- **Web Interface Integration**: All statistics accessible via atomic loads
- **Reset Functionality**: Unified statistics reset across all devices

### LED Monitor Task Implementation ✅ COMPLETED
- **Centralized LED Management**: Replaced scattered LED calls with unified monitor task
  - **TaskScheduler Integration**: 50ms interval LED monitoring task
  - **Snapshot Comparison**: Efficient LED state updates using LedSnapshot structure
  - **Activity Detection**: Smart LED updates only when device activity changes
  - **Bridge Mode Awareness**: LED task enabled/disabled based on bridge mode
- **Performance Optimization**: Eliminated redundant LED calls throughout codebase
- **Code Simplification**: Removed LED timing variables from bridge processing

### Protocol Pipeline Integration ✅ COMPLETED
- **Sender Statistics Cleanup**: Removed static counters from UartSender and UdpSender
  - **Direct Statistics**: Senders now update global g_deviceStats directly
  - **Memory Optimization**: Eliminated duplicate statistics tracking
  - **Unified Interface**: All statistics accessible through single global structure
- **Pipeline Statistics**: Protocol processing statistics integrated with global system

### Web Interface Statistics Overhaul ✅ COMPLETED
- **Atomic Statistics Access**: All web API endpoints use atomic loads
  - **Lock-free Operations**: Replaced critical sections with `load(std::memory_order_relaxed)`
  - **Real-time Updates**: Direct access to current statistics without blocking
  - **JSON Field Compatibility**: Maintained existing field names for frontend compatibility
- **Statistics Reset**: Simplified reset functionality using atomic stores
- **Diagnostic Integration**: Enhanced statistics reporting in web interface

### Code Architecture Cleanup ✅ COMPLETED
- **Legacy Statistics Removal**: Eliminated UartStats structure and related functions
  - **Types.h Cleanup**: Removed UartStats, statsMux, and old statistics functions
  - **Bridge Context**: Simplified BridgeContext initialization signature
  - **Function Signatures**: Updated all initialization functions to remove UartStats parameters
- **Critical Section Elimination**: Removed all portMUX-based statistics protection
- **Memory Efficiency**: Reduced memory footprint through unified statistics approach

### Technical Benefits 🚀
- **Lock-free Performance**: Atomic operations eliminate contention and improve throughput
- **Simplified Maintenance**: Single statistics system across entire codebase
- **Better Reliability**: No deadlocks or race conditions in statistics collection
- **Reduced Memory Usage**: Eliminated duplicate counters and synchronization primitives
- **Cleaner Architecture**: Clear separation between statistics collection and LED management

## v2.15.4 (UDP Sender Refactoring & Protocol-Aware Batching) 🚀

### Major Architecture Simplification ✅ COMPLETED
- **SPSC Queue Removal**: Eliminated complex inter-core Single Producer Single Consumer queue system
  - **Removed Components**: device4Task completely deleted, UdpTxQueue removed from UdpSender
  - **Memory Reduction**: ~6KB saved (4×1500 byte queue slots + task overhead)
  - **Latency Improvement**: Direct UDP transmission without inter-core queuing delays
  - **Code Simplification**: 300+ lines of complex synchronization code removed

### Direct UDP Integration ✅ COMPLETED  
- **Direct AsyncUDP**: UdpSender now uses AsyncUDP transport directly (no task intermediary)
- **UART→UDP Path**: processDevice1Input() → CircularBuffer → Pipeline → UdpSender → AsyncUDP (direct)
- **UDP→UART Path**: AsyncUDP onPacket → uartBridgeSerial->write() (direct bypass of Pipeline)
- **Statistics Integration**: UDP TX/RX stats moved to static UdpSender members for thread-safe access
- **Transport Creation**: UDP transport created in main.cpp for both Bridge and Logger modes

### Buffer Architecture Separation ✅ COMPLETED
- **Dual Buffer System**: Separate circular buffers for different operational modes
  - **Telemetry Buffer**: For UART→USB/UDP telemetry processing (Bridge mode)
  - **Log Buffer**: For line-based log processing (Logger mode, 1024 bytes)
- **Memory Optimization**: Only allocate needed buffer per mode (Logger saves ~1KB+ adaptive buffer)
- **Buffer Manager**: New buffer_manager.h/cpp for centralized buffer allocation/deallocation
- **Context Separation**: BridgeContext.buffers struct replaces old adaptive.circBuf reference

### UDP Logger Mode Enhancements ✅ COMPLETED
- **Buffer Initialization**: UDP log buffer properly zeroed to prevent garbage data transmission
- **WiFi Connection Checks**: Logger task clears buffers when WiFi disconnected
- **Line Parser Improvements**: Enhanced LineBasedParser supports \\r\\n, \\r, \\n line endings
- **Long Line Handling**: Graceful handling of lines without endings (skip with warning)
- **Logger-Bridge Separation**: UART telemetry ignored in Logger-only mode (prevents buffer conflicts)

### Protocol Pipeline Architecture ✅ COMPLETED
- **Mode-Specific Initialization**: Pipeline uses correct buffer based on Device4 role
  - **Logger Mode**: Uses logBuffer + LineBasedParser
  - **Bridge Mode**: Uses telemetryBuffer + RawParser/MavlinkParser
- **Buffer Validation**: Proper null-pointer checks for buffer availability
- **Error Handling**: Clear error messages for buffer allocation failures

### Compilation & Integration Fixes ✅ COMPLETED
- **Static Variable Definitions**: UDP statistics properly defined in udp_sender.cpp
- **Include Dependencies**: Added buffer_manager.h includes to uart/uartbridge.cpp
- **Context Access**: scheduler_tasks.cpp uses getBridgeContext() for buffer access
- **Bridge Processing**: Added Logger mode checks to prevent telemetry buffer usage

### Architecture Benefits 🚀
- **Simplified Design**: Removed 3 complex architectural layers (SPSC, Device4Task, injection)
- **Better Performance**: Direct UDP transmission reduces latency and CPU overhead
- **Memory Efficient**: Mode-specific buffer allocation saves memory
- **Maintainable Code**: Cleaner separation between Logger and Bridge functionality
- **Robust Logging**: UDP Logger mode with proper WiFi handling and line parsing

### Final Architecture Stabilization ✅ COMPLETED
- **Fixed Sender Indices**: Implemented stable IDX_USB=0, IDX_UART3=1, IDX_UDP=2 sender routing
  - **Eliminated Array Shifting**: Fixed sender indices prevent routing instability 
  - **PacketSource Integration**: Added SOURCE_TELEMETRY, SOURCE_LOGS, SOURCE_DEVICE4 packet tagging
  - **DataFlow Architecture**: Multi-stream processing with separate Telemetry and Logger flows
  - **Sender Mask Routing**: Precise packet routing with SENDER_USB, SENDER_UART3, SENDER_UDP masks

- **Bridge Processing Fix**: Resolved USB telemetry blocking in Logger mode
  - **Logger Mode Issue**: USB received no data when Logger was active (bridge_processing.h)
  - **Root Cause**: processDevice1Input() blocked UART→telemetryBuffer in D4_LOG_NETWORK mode
  - **Solution**: Removed blocking condition, USB always processes data through telemetryBuffer
  - **Result**: Logger mode with simultaneous USB telemetry and UDP log transmission

- **UDP Sender Universal Naming**: Renamed MAVLink-specific naming to universal atomic packet handling
  - **Renamed**: mavlink* → atomic* (atomicBatchBuffer, processAtomicPacket, etc.)
  - **Scope**: Universal batching for MAVLink telemetry, line-based logs, and future atomic packets
  - **Maintained**: All existing batching functionality and performance optimizations
  - **Prepared**: Framework for packet-type-specific batching strategies via PacketSource

- **WiFi Connectivity Universalization**: Created unified WiFi readiness checking
  - **New Function**: wifi_manager_is_ready_for_data() works for both AP and Client modes
  - **AP Mode**: Checks for connected clients before allowing data transmission
  - **Client Mode**: Checks connection status to target network
  - **Integration**: Used across UDP Logger, UDP Sender, and other network components
  - **Legacy**: Maintained wifi_manager_is_connected() for compatibility

- **UDP Batching Configuration Control**: Added configurable UDP packet batching for legacy GCS compatibility
  - **Configuration Field**: New udpBatchingEnabled setting (default: enabled) in config version 8
  - **Web Interface**: Added checkbox in Device 4 Network Configuration panel
  - **Backend Integration**: UdpSender.setBatchingEnabled() method for runtime configuration
  - **Automatic Migration**: Existing configurations upgraded to version 8 with batching enabled
  - **Performance Control**: Users can disable batching if GCS doesn't support batched UDP packets
  - **Logging**: Clear indication when UDP batching is enabled/disabled in system logs

### Architecture Evolution Within v2.15.3 📈
- **Phase 1**: Initial SPSC queue implementation for Device 4 Pipeline integration
- **Phase 2**: Real-world testing revealed complexity vs benefit imbalance  
- **Phase 3**: Architecture simplified - removed SPSC queue, implemented direct UDP + buffer separation
- **Phase 4**: Final stabilization - fixed routing, universal naming, WiFi handling improvements
- **Result**: Production-ready, maintainable, and performance-optimized architecture

## v2.15.2 (UDP Sender Refactoring & Protocol-Aware Batching) 🚀

### UDP Sender Architecture Overhaul ✅ COMPLETED
- **Protocol-Aware Batching**: Separate MAVLink and RAW packet batching strategies
  - **MAVLink Packets**: Optimized batching with keepWhole integrity (2-5 packets per batch)
  - **RAW Data**: Legacy batching for non-protocol data streams
  - **Smart Routing**: Automatic packet classification using keepWhole flag from parsers
  - **Adaptive Thresholds**: Different limits for normal (2 packets/600 bytes/5ms) vs bulk mode (5 packets/1200 bytes/20ms)

### Network Transmission Efficiency ✅ COMPLETED
- **MTU-Aware Batching**: Up to 1400 bytes per UDP datagram for optimal network utilization
- **Bulk Mode Integration**: Extended timeouts (20ms) for MAVFtp transfers, low latency (5ms) for telemetry
- **Timeout-Based Flushing**: Prevents packet stalling during low-traffic periods
- **Protocol Consistency**: Unified approach with USB sender optimizations

### Code Architecture Cleanup ✅ COMPLETED
- **TransmitHints Simplification**: Removed deprecated canBatch and urgentFlush flags
- **Parser Updates**: Cleaned up mavlink_parser.h and raw_parser.h hint assignments
- **Diagnostic Logging**: Added UDP-specific debug traces for batch operations and timeout events
- **Memory Safety**: Proper destructor with batch flushing before cleanup

## v2.15.1 (Task Priority Optimization & USB Batching Improvements) 🚀

### Task Priority Rebalancing ✅ COMPLETED
- **DMA Task Priority Increase**: Elevated DMA task priority above UartBridge task
  - **Problem Solved**: Eliminated UART FIFO overflow issues during high-throughput operations
  - **Result**: Stable data loading without packet loss
  - **Architecture**: Better real-time performance for DMA operations
  - **Impact**: More reliable MAVFtp and bulk transfer operations

### USB Batching Timeout Optimization ✅ COMPLETED
- **Adaptive Batch Timeouts**: Different timeouts for normal vs bulk modes
  - **Normal Mode**: 5ms timeout for low latency telemetry
  - **Bulk Mode**: 20ms timeout for optimal MAVFtp batching efficiency
  - **Smart Fallback**: Partial batch transmission on timeout to prevent data stalling
  - **Buffer Size Unification**: TX buffer increased to 2048 bytes across USB Device/Host modes
- **Complete Partial Write Elimination**: Full architectural cleanup
  - **Removed**: Pending buffer structure, flushPending() method
  - **Simplified**: Single packet and batch transmission without partial writes
  - **Enhanced**: Timeout-based partial batch transmission for stuck scenarios

### System Stability Improvements ✅ COMPLETED
- **USB Block Detection Timeout**: Increased from 500ms to 1000ms for better detection accuracy
- **Pool Exhausted Logging**: Rate limiting (1/second) and reduced severity (INFO vs WARNING)
- **Statistics Interface Fix**: Real MAVLink statistics instead of hardcoded zeros
  - **Backend**: getSender() methods for accessing real USB transmission counters
  - **Frontend**: Updated web interface with accurate packet counts and error statistics

## v2.15.0 (MAVLink Parser Refactoring to Byte-wise Parsing) 🚀

### Major MAVLink Parser Simplification
- **Byte-wise Parsing**: Migrated from complex state machine to standard pymavlink byte-wise parsing
  - **Removed Complexity**: Eliminated stash buffer, consume logic, prefix bytes tracking
  - **Standard Approach**: Using mavlink_parse_char() for each byte - industry standard
  - **Code Reduction**: Parser reduced from ~500 to ~300 lines
  - **No More Deadlocks**: Removed PacketAssembler and incomplete packet tracking

### BulkModeDetector Implementation
- **Smart Detection**: Decay counter mechanism for MAVFtp and parameter bulk transfers
  - **Hysteresis**: ON at 20 packets/sec, OFF at 5 packets/sec
  - **100ms Decay**: Counter decrements every 100ms for smooth transitions
  - **Targeted Detection**: Only counts FILE_TRANSFER_PROTOCOL and PARAM messages
  - **Adaptive Flushing**: Immediate flush in bulk mode, batched in normal mode

### USB Batching Architecture v2 ✅ COMPLETED
- **Complete Rewrite**: Full USB sender architecture overhaul for data integrity
  - **Pending Buffer**: Added protection against partial write data loss
  - **Helper Methods**: applyBackoff(), resetBackoff(), inBackoff(), updateTxCounter(), commitPackets(), flushPending(), sendSinglePacket()
  - **4-Step Algorithm**: Linear processing - flush pending → handle partials → single packets → bulk batching
  - **N/X/T Thresholds**: 4 packets OR 448 bytes OR 5ms timeout for batch flushing
  - **Bulk Mode Transitions**: Force flush on bulk mode exit to prevent latency
- **Memory Pool Enhancement**: Increased 128B pool from 20 to 60 blocks for MAVFtp transfers
- **Queue Architecture**: Migrated from std::queue to std::deque for direct indexing and batch planning
- **Compiler Fixes**: Resolved type deduction errors in min() calls with explicit std:: namespace

### USB Block Detection & Memory Pool Protection ✅ COMPLETED
- **Blocked USB Detection**: Smart detection when COM port closed on host side
  - **Timeout-based Logic**: 500ms timeout when availableForWrite() value unchanged
  - **Memory Pool Protection**: Auto-clear all queues to prevent pool exhaustion on startup
  - **Recovery Detection**: Keep 1 test packet for automatic unblock detection
  - **Diagnostic Logging**: `[USB-DIAG] USB blocked/unblocked` messages for debugging
- **Memory-Safe Operations**: Proper packet.free() calls in clearAllQueues()
- **Startup Protection**: No more `Pool exhausted for size X, using heap` messages when USB dead
- **Enhanced enqueue()**: Override in UsbSender to limit packets during blocked state

### MAVLink Statistics Web Interface Fix ✅ COMPLETED
- **Accurate Statistics Display**: Fixed web interface showing zeros for MAVLink stats
  - **Backend Changes**: Added getSender() methods to ProtocolPipeline for sender access
  - **Real Data Sources**: packetsSent/packetsDropped now from actual USB sender statistics
  - **Proper Error Counting**: Detection errors now counted from MAVLINK_FRAMING_BAD_CRC/BAD_SIGNATURE
  - **Field Renaming**: packetsDetected → packetsParsed for clarity
- **Frontend Updates**: Redesigned renderMavlinkStats() with relevant metrics
  - **Packet Statistics Panel**: Parsed, Sent, Dropped, Errors
  - **Packet Analysis Panel**: Average Size, Size Range, Last Activity  
  - **Removed Deprecated**: Resync Events, Total Bytes, Transmitted fields
- **Data Integrity**: All statistics now show real values instead of hardcoded zeros

### Protocol Pipeline Fix
- **Critical Logic Fix**: Fixed incorrect consume order in protocol_pipeline.h
  - **Before**: Only consumed bytes when packets found (data loss!)
  - **After**: ALWAYS consume bytes first, THEN distribute packets
  - **Universal Pattern**: Works for all parsers (MAVLink, RAW, future SBUS/CRSF)

### Cleanup and Maintenance
- **Priority System Removal**: Completely removed packet priority infrastructure
  - **Deleted**: enum PacketPriority, priority field from ParsedPacket
  - **Renamed**: dropLowestPriority() → dropOldestPacket()
  - **Simplified**: All packets treated equally in FIFO queue
- **Diagnostic Marking**: Unified format for temporary diagnostics
  - **Standard Format**: `=== DIAGNOSTIC START === (Remove after MAVLink stabilization)`
  - **Easy Removal**: All diagnostic code clearly marked for future cleanup
- **TODO Addition**: Added reminder for adaptive buffer minimum size analysis

## v2.14.1 (CircularBuffer Deadlock Fix & Priority System Rollback) ✅

### CircularBuffer Critical Fix
- **tail=511 Deadlock Resolution**: Fixed critical issue where CircularBuffer would deadlock at buffer boundaries
  - **Root Cause**: When tail near buffer end (e.g., tail=459 in 512-byte buffer), only 53 linear bytes available despite 511 total bytes
  - **Solution**: Data linearization using tempLinearBuffer[296] for transparent wrap handling
  - **Impact**: MAVLink parser now gets consistent contiguous view up to 296 bytes regardless of wrap position
- **Memory Optimization**: Removed obsolete shadow buffer system
  - **Heap Savings**: 296 bytes per buffer moved from heap to BSS (better for embedded)
  - **Cleaner Architecture**: Eliminated non-functional shadow update logic from write() method
- **Simplified Consume Logic**: Replaced complex boundary jump heuristics with clean consume pattern
  - **No More Boundary Jumps**: Parser always consumes exactly bytesProcessed amount
  - **Eliminated Stuck States**: No more aggressive consume or forced boundary jump workarounds

### Priority System Rollback
- **Architecture Simplification**: Rolled back from 4-priority queue system to single FIFO
  - **Removed Complexity**: Eliminated CRITICAL/HIGH/NORMAL/BULK priority queues
  - **Single Queue**: PacketSender now uses simple std::queue for all packets
  - **Simplified Enums**: protocol_types.h reduced to PRIORITY_NORMAL only
- **Code Cleanup**: Removed all priority-specific methods and logic
  - **Parser Simplification**: MavlinkParser.getPacketPriority() now returns PRIORITY_NORMAL for all
  - **Sender Cleanup**: Removed getDropped{Bulk,Normal,Critical} methods from UartSender/UdpSender
  - **TODO Comments**: Added documentation for potential future priority reintroduction

### Performance Improvements
- **MAVFtp Stability**: File transfer now works reliably without progress rollbacks
- **Packet Loss Reduction**: From 100+ losses per session to <1%
- **CRC Error Reduction**: From 100+ CRC errors to <10 per session
- **High Speed Support**: Stable operation at 921600 baud without packet corruption

## v2.14.0 (PyMAVLink Migration & Architecture Cleanup) ✅

### MAVLink Library Migration
- **PyMAVLink Integration**: Complete migration to PyMAVLink for better compatibility and maintainability
  - **Library Replacement**: Replaced previous MAVLink implementation with standard PyMAVLink
  - **Generated Headers**: Full pymavlink integration (common + ardupilotmega dialects)
  - **Configuration**: Updated build system with proper PyMAVLink compiler flags
  - **Improved Parsing**: Enhanced MAVLink processing efficiency and reliability

### Architecture Simplification
- **Protocol Detection Removal**: Eliminated complex protocol detection system
  - **Removed Files**: mavlink_detector.h/cpp, protocol_detector.h, protocol_factory.h/cpp
  - **Direct Parser Creation**: Protocol pipeline now creates parsers directly based on configuration
  - **Simplified Logic**: PROTOCOL_MAVLINK → MavlinkParser, PROTOCOL_NONE → RawParser
- **MAVLink Function Migration**: Moved MAVLink-specific logic from pipeline to parser
  - **MavlinkParser Enhancement**: Added non-MAVLink stream detection and warnings
  - **Smart Buffering**: Parser handles invalid data gracefully with progress warnings
  - **Error Recovery**: Automatic detection of non-MAVLink streams with user guidance

### Code Structure Cleanup
- **types.h Optimization**: Cleaned protocol structure from detection artifacts
  - **Removed Fields**: detector pointer, minBytesNeeded (detection-specific)
  - **Preserved Fields**: type, stats, enabled flag for runtime control
  - **Initialization**: Proper protocol structure initialization in initBridgeContext()
- **Include Cleanup**: Removed legacy library and detector includes across codebase
- **Factory Pattern Removal**: Eliminated initProtocolDetectionFactory() and cleanup functions

### Technical Improvements
- **Memory Efficiency**: Reduced memory footprint by removing detection overhead
- **Processing Speed**: Direct parser instantiation eliminates detection delays
- **Maintainability**: Simplified codebase with clear parser responsibilities
- **Future Ready**: Clean architecture for additional protocol support

## v2.13.2 (Protocol Statistics Web Interface) ✅

### Protocol Statistics Implementation
- **Unified Protocol Statistics**: Complete web interface implementation for all protocol types
  - **RAW Protocol Statistics**: Now shows detailed chunk processing, buffer usage, and output device metrics
  - **MAVLink Protocol Statistics**: Enhanced packet detection stats with priority-based drop analysis
  - **Extensible Architecture**: Ready for future SBUS and other protocol implementations

### Web Interface Enhancements
- **Protocol Pipeline Integration**: Centralized statistics through ProtocolPipeline.appendStatsToJson()
- **Dynamic Protocol Display**: 
  - RAW mode shows "Chunks Created", buffer utilization, timing-based processing metrics
  - MAVLink mode shows "Packets Detected", detection errors, resync events with precise packet analysis
  - Automatic protocol type detection and appropriate UI rendering
- **Sender Statistics Table**: Unified output device statistics with protocol-aware drop analysis
  - MAVLink: Priority breakdown (Bulk/Normal/Critical packet drops)
  - RAW: Percentage-based drop rate calculation
  - Queue depth monitoring and maximum queue tracking

### Backend Architecture
- **ProtocolPipeline Statistics**: Added appendStatsToJson() method for unified data export
- **JSON Structure Standardization**: Consistent protocol statistics format across all protocol types
- **Memory Efficient**: Uses existing pipeline structures without additional allocations
- **ArduinoJson Integration**: Proper JSON serialization for web interface consumption

### Code Modernization
- **Eliminated Legacy Stats**: Removed old protocol statistics code from web_api.cpp (~40 lines)
- **Modular JavaScript**: Added specialized render methods for different protocol types
- **Responsive Design**: Grid-based layout adapts to different screen sizes
- **Error Handling**: Graceful fallback for unknown or future protocol types

### Critical Fixes and Optimizations
- **Fixed Protocol Statistics Crashes**: Added comprehensive null pointer checks in appendStatsToJson()
  - **Problem**: Null pointer access causing packet losses and system instability
  - **Solution**: Added checks for !ctx, !ctx->system.config before accessing pointers
  - **Result**: Eliminated crashes during statistics export in web interface
- **Fixed RAW Parser Statistics**: RAW protocol now properly updates statistics in web interface
  - **Problem**: RAW parser showed zeros for all metrics despite active data processing
  - **Solution**: Used existing updatePacketSize() and stats->reset() methods instead of creating new fields
  - **Result**: Real-time statistics display for chunk processing, bytes transmitted, size ranges
- **Memory Pool Optimization**: RAW parser now uses standard pool sizes for efficiency
  - **Problem**: Arbitrary chunk sizes (98, 103, 115 bytes) caused "Pool exhausted" warnings
  - **Solution**: Round allocation sizes to pool standards (64, 128, 288, 512 bytes) while preserving actual data size
  - **Result**: Eliminated pool warnings, improved memory efficiency, reduced heap fragmentation
- **Statistics Reset Fix**: "Reset Statistics" button now properly clears all protocol metrics
  - **Problem**: New protocol fields not reset when user clicked reset button
  - **Solution**: Enhanced reset() methods to clear all statistics fields including protocol-specific ones
  - **Result**: Complete statistics reset functionality across all protocols

## v2.13.1 (Statistics Synchronization Unification) ✅

### Statistics Architecture Refactoring
- **Unified Statistics Management**: Centralized all device statistics functions in diagnostics.cpp
  - **Problem Solved**: Eliminated duplicate statistics functions across multiple files
  - **Core 0/Core 1 Separation**: Clean separation by CPU cores for optimal performance
  - **Thread Safety**: All device statistics properly synchronized using critical sections

### Function Consolidation
- **Moved to diagnostics.cpp**:
  - `updateMainStats()` - Updates Device 1/2 statistics (Core 0 devices)
  - `updateDevice3Stats()` - Updates Device 3 statistics (Core 1 device)  
  - `updateDevice4Stats()` - Updates Device 4 statistics (Core 1 device)
- **Removed Duplicate Code**:
  - Deleted `updateMainStats()` from uartbridge.cpp
  - Deleted `updateDevice3Stats()` from device3_task.cpp
  - Removed static pointer variables no longer needed
  - Clean extern declarations in scheduler_tasks.cpp

### Architecture Benefits
- **Proper Core Distribution**:
  - Core 0: Device 1 (UART1) + Device 2 (USB/UART2) → unified in `updateMainStats()`
  - Core 1: Device 3 (UART3) + Device 4 (UDP) → separate functions for each
- **Reduced Race Conditions**: Each device's statistics updated in single location
- **Better Maintainability**: All statistics logic centralized in diagnostics module
- **Performance**: Minimal blocking between cores, optimized update intervals

### Device 4 Statistics Integration
- **BridgeContext Integration**: Added Device 4 fields to main bridge context
- **Pipeline Statistics**: UdpSender properly updates TX byte counters
- **Scheduler Tasks**: Device 4 statistics task properly restored and configured
- **Global Variables**: Maintained existing globalDevice4* variables for task communication

## v2.13.0 (Protocol Architecture Refactoring) 🚀

### Major Architecture Overhaul: Parser + Sender Pattern
- **Complete Protocol Architecture Redesign**: Implemented separation of packet parsing and transmission
  - **New Data Flow**: UART RX → CircularBuffer → Parser → PacketQueue → Sender(s) → Device(s)  
  - **Problem Solved**: Eliminated coupling between protocol detection and transmission logic
  - **Benefits**: Clean architecture, better maintainability, protocol-agnostic design

### Memory Pool Implementation
- **Slab Allocator**: Thread-safe memory pool for packet management
  - **Implementation**: `packet_memory_pool.h` with Meyers Singleton pattern
  - **Pool Sizes**: 64B (control packets), 288B (MAVLink), 512B (RAW chunks)
  - **Fallback**: Automatic heap allocation when pools exhausted
  - **Thread Safety**: C++11+ guaranteed initialization safety

### Protocol Pipeline Components
- **Parser Framework**: Base `ProtocolParser` class with specialized implementations
  - **RawParser**: Adaptive buffering without protocol parsing (default mode)
  - **MAVLinkParser**: MAVLink packet boundary detection and parsing
  - **Future Ready**: Architecture supports SBUS, CRSF protocol additions

- **Sender Framework**: Priority-based packet transmission system
  - **UsbSender**: Exponential backoff for congestion handling
  - **UartSender**: Inter-packet gap support for timing-sensitive protocols
  - **UdpSender**: Batching capabilities for network efficiency
  - **Priority Support**: CRITICAL > NORMAL > BULK packet prioritization

### Transmission Improvements
- **Backpressure Handling**: Intelligent packet dropping based on priority levels
- **Partial Send Support**: USB/UART senders handle incomplete transmissions
- **Queue Management**: Per-sender packet queues with size limits
- **Flow Control**: Ready state checking prevents buffer overflow

### Integration & Cleanup
- **Pipeline Coordinator**: `ProtocolPipeline` class manages all parsers and senders
- **Bridge Integration**: Seamless integration with existing UART bridge task
- **Legacy Removal**: Cleaned up old protocol detection functions
- **Code Quality**: All comments converted to English, consistent style

### Performance Benefits
- **Zero Copy**: Direct buffer access for parsing without data copying
- **Memory Efficient**: Pool allocation prevents heap fragmentation
- **Multi-core Ready**: Thread-safe design for ESP32 dual-core operation
- **Scalable**: Easy addition of new protocols and output devices

## v2.12.1 (Memory Optimization & Final Code Cleanup) ✅

### Memory Optimization Phase 
- **Legacy Buffer Removal**: Complete elimination of old buffer architecture
  - **Memory Saved**: 384-2048 bytes of RAM (removed duplicate buffer allocation)
  - **Code Cleanup**: Removed unused `adaptiveBuffer` allocation in `uartbridge.cpp:95-98`
  - **Architecture**: Now uses only CircularBuffer implementation
  - **Validation**: Ensured no remaining references to legacy buffer system

### bufferIndex Removal Optimization
- **Structure Cleanup**: Removed `bufferIndex` from `AdaptiveBufferContext` structure
  - **Memory Saved**: Additional 8 bytes (pointer + int) per bridge context
  - **Code Paths**: Eliminated all bufferIndex initialization and update code
  - **Critical Fix**: Updated `isBufferCritical()` in `protocol_pipeline.h:182-188`
    - **Before**: Used legacy `bufferIndex` for buffer fullness detection
    - **After**: Direct CircularBuffer availability check: `circBuf->available() >= bufferSize - 64`
  - **Validation**: Comprehensive search confirmed no remaining bufferIndex references

### Production Code Cleanup
- **Debug Code Removal**: All temporary diagnostic logging removed for production
  - **Files Cleaned**: `circular_buffer.h`, `main.cpp`, `adaptive_buffer.h`
  - **Log Reduction**: Removed verbose buffer initialization and operation logs
  - **Performance**: Cleaner log output focused on critical events only
  - **Maintenance**: Easy identification and removal of TEMPORARY DEBUG blocks

### Final Architecture State
- **Single Buffer**: CircularBuffer as the only data buffer implementation
- **Zero Waste**: No duplicate memory allocations or unused legacy code
- **Clean Codebase**: Production-ready with minimal diagnostic output
- **Memory Efficient**: Optimized RAM usage through complete legacy removal

## v2.12.0 (Circular Buffer Implementation & Adaptive Thresholds) 🚀

### Major Architecture Changes
- **Circular Buffer Migration**: Replaced legacy linear buffer with modern circular buffer
  - **Problem**: Old buffer used inefficient memmove operations causing data corruption
  - **Solution**: Zero-copy circular buffer with scatter-gather I/O
  - **Implementation**:
    - New `CircularBuffer` class in `src/circular_buffer.h`
    - Power-of-2 sizing for bitwise masking (256-2048 bytes)
    - Shadow buffer (296 bytes) for protocol parsing continuity
    - Thread-safe with FreeRTOS portMUX locks
  - **Critical Fix**: Added missing `initAdaptiveBuffer()` call in `uartbridge.cpp`
    - BridgeContext now properly initializes CircularBuffer
    - Fixed nullptr issue preventing data transmission

### Adaptive Traffic Detection
- **Gap-based Traffic Detector**: Automatic mode switching based on data patterns
  - **Normal Mode**: Standard telemetry (2ms timeout, 10% buffer threshold)
  - **Burst Mode**: Parameter downloads (5ms timeout, 20% buffer threshold)
  - **Detection Logic**:
    - Fast data: <1ms gaps between bytes
    - Burst trigger: 100+ consecutive fast bytes
    - Burst end: >5ms pause in data stream
  - **Benefits**: Optimizes for both low-latency telemetry and bulk transfers

### USB Bottleneck Handling
- **Smart Drop Strategy**: Prevents complete system hangs
  - **Thresholds** (percentage-based, scales with buffer size):
    - Warning: 85% (small), 70% (medium), 60% (large)
    - Critical: 90% (small), 75% (medium), 65% (large)
  - **Drop Policy**:
    - Warning level: Drop 12.5% after 5 retry attempts
    - Critical level: Drop 33% immediately
  - **Improvement**: Better than old system which could hang indefinitely

### Performance Optimizations
- **Transmission Thresholds**:
  - Normal: 128 bytes max (low latency priority)
  - Burst: 200 bytes max (optimized for single MAVLink packet)
- **Buffer Sizes** (auto-calculated based on baudrate):
  - 115200: 288 bytes (384 with protocol)
  - 230400: 768 bytes
  - 460800: 1024 bytes
  - 921600: 2048 bytes

### Diagnostic Enhancements
- **Comprehensive Debug Logging** (all marked TEMPORARY for easy removal):
  - Buffer initialization and allocation
  - Write operations and availability
  - Transmission triggers with mode indication
  - USB status and bottleneck events
  - Segment information for scatter-gather
- **Traffic Statistics**:
  - Total bursts detected
  - Maximum burst size
  - Drop events and bytes

### TODO.md Updates
- Added Circular Buffer Optimizations (v2.6.x)
- Added Protocol-aware Optimizations (v2.7.0)
- Added USB Optimizations (v2.7.x)

### Testing Recommendations
1. **Low Traffic Test**: Normal telemetry should show "NORMAL mode"
2. **High Traffic Test**: Parameter downloads should trigger "BURST mode"
3. **USB Bottleneck Test**: Monitor for controlled drops vs hangs
4. **Success Metrics**:
   - Packet loss <5% (from 32+ events)
   - CRC errors <20 (from 56)
   - No transmission freezes
   - Successful MAVLink parameter downloads

## v2.11.0 (Project Restructuring & MAVLink Fix) ✅
- **Project Structure Reorganization**: Improved code organization with dedicated folders
  - **Created subfolder structure**: Organized related modules into logical directories
    - `src/web/` - Web interface modules (web_api, web_interface, web_ota)
    - `src/usb/` - USB implementation files (usb_device, usb_host, interfaces)
    - `src/devices/` - Device tasks and initialization (device3_task, device4_task, device_init)
    - `src/wifi/` - WiFi management (wifi_manager)
    - `src/uart/` - UART and flow control (uart_dma, uartbridge, flow_control)
  - **Updated all include paths**: Fixed 30+ files with new relative paths
    - Files within folders use local includes (`"file.h"`)
    - Cross-folder includes use relative paths (`"../folder/file.h"`)
    - Main files use folder paths (`"folder/file.h"`)
  - **Benefits**:
    - Better code navigation and maintainability
    - Clearer module boundaries and dependencies
    - Easier to locate related functionality
    - Reduced clutter in src/ root directory

- **MAVLink Parser Critical Fix**: Resolved packet loss and CRC errors
  - **Root Cause**: Parser state corruption from buffer operations
  - **Solution**: Independent frame buffer approach with improved isolation
  - **Implementation**: Enhanced parser with stable buffer management and simplified error handling
  - **Results**:
    - **Before**: 547 packet losses, 349 CRC errors, 13314 detection errors
    - **After**: <1% packet loss, <10 CRC errors, <100 detection errors
    - MAVFtp file transfers now work reliably
    - Eliminated configuration-related parsing errors
  - **Code Cleanup**:
    - Removed `#include <map>` and all std::map usage
    - Deleted methods: `updateDetailedStats()`, `checkSequenceLoss()`, `logPacketInfo()`
    - Simplified to basic error counters for diagnostics
    - Reduced memory footprint and complexity

## v2.10.2 (USB Backpressure Reversion) ✅
- **USB Backpressure Simplification**: Reverted complex behavioral detection to simple approach
  - **Removed Features**: Complex behavioral port state detection system
    - **Behavioral detection logic** removed from `src/usb_device.cpp`
    - **Dynamic thresholds** (ASSUME_CLOSED_THRESHOLD, FIRST_ATTEMPT_THRESHOLD) eliminated
    - **Port state tracking** and related statistics removed
    - **Automatic protocol detector reset** on port changes disabled
  - **Root Cause**: Stability issues and false positives on different systems
    - **False port closure detection** causing unnecessary data drops
    - **System-dependent behavior** making solution unreliable
    - **Over-complexity** for marginal benefit in real-world usage
  - **Current Implementation**: Simple Arduino Serial backpressure
    - **Basic `Serial.availableForWrite()`** check retained
    - **Buffer overflow prevention** still functional
    - **Improved stability** across different USB host systems
    - **Simpler debugging** without complex state machine
  - **Decision Rationale**:
    - Simple solution proved more reliable than complex behavioral detection
    - Arduino Serial backpressure sufficient for most use cases
    - Eliminated source of platform-specific issues
  - **Future Consideration**: May revisit with simpler heuristics if specific need arises

## v2.10.1 (Configuration Cleanup) ✅
- **USB Configuration Cleanup**: Removed unsupported USB Auto mode
  - **Code Cleanup**: Removed USB_MODE_AUTO enum value and all related handling code
  - **Web Interface**: Removed "Auto (not supported)" option from USB mode selection
  - **Configuration**: Simplified USB mode to only Device and Host options
  - **Implementation**: Deleted UsbAuto class and createUsbAuto factory function
  - **Benefits**: Cleaner codebase, no misleading configuration options

## v2.10.0 (USB Buffer Overflow Prevention) ✅
- **USB Buffer Overflow Prevention**: Behavioral port state detection to prevent buffer overflow
  - **Behavioral Port Detection** (`src/usb_device.cpp`): Smart detection of USB port state without relying on unavailable APIs
    - **Buffer monitoring**: Tracks write buffer availability patterns to detect port closure
    - **Adaptive thresholds**: ASSUME_CLOSED_THRESHOLD (20) and FIRST_ATTEMPT_THRESHOLD (5) for optimal response
    - **Honest API**: Returns 0 when unable to write, allowing caller to manage data freshness
    - **Automatic recovery**: Seamless transition when port opens/closes
  - **Early Data Dropping** (`src/adaptive_buffer.h`): Prevents accumulation of stale data
    - **Connection state tracking**: Monitors USB port state changes in real-time
    - **Protocol detector reset**: Automatic reset when port reopens to prevent packet fragmentation
    - **Buffer cleanup**: Immediate buffer clearing on port state transitions
    - **Diagnostic integration**: Proper statistics tracking for dropped data
  - **Improved Diagnostics**: Enhanced meaning of UART FIFO overflow messages
    - **Real issues only**: FIFO overflow now indicates actual USB performance problems when port is open
    - **Reduced log spam**: Eliminated false overflow warnings when port is closed
    - **Better troubleshooting**: Clear distinction between port closure and performance issues
  - **Real-time Performance**: Optimized for time-critical applications
    - **Fast detection**: ~20-200ms response time for port closure detection
    - **Fresh data guarantee**: Only current data transmitted after port reopens
    - **Zero stale data**: Prevents transmission of outdated information

## v2.9.5 (Critical Memory Safety & Logging Refactoring) ✅
- **Critical Memory Safety Fixes**: Eliminated heap corruption and segmentation faults
  - **Adaptive Buffer Protection** (`src/adaptive_buffer.h`): Fixed buffer underflow and bounds checking
    - **Underflow prevention**: Added validation for `totalConsumed > bufferIndex` before size_t arithmetic
    - **Bounds checking**: Comprehensive validation before all memmove operations
    - **Safe state recovery**: Automatic buffer reset on detection of invalid states
    - **Partial transmission safety**: Enhanced partial data transmission with proper boundary checks
  - **Protocol Pipeline Hardening** (`src/protocols/protocol_pipeline.h`): Fixed MAVLink detection crashes
    - **Pointer validation**: Added bounds checking before `detector->canDetect()` calls
    - **Search window protection**: Prevented buffer overruns during protocol packet search
    - **Packet size validation**: Limited packet sizes to reasonable bounds (max 512 bytes)
    - **Diagnostic logging**: Added periodic state dumps for debugging protocol issues
  - **MAVLink Detector Improvements** (`src/protocols/mavlink_detector.h/cpp`): Enhanced error recovery
    - **Increased search window**: Expanded from 64 to 300 bytes for better resync capability
    - **Improved resync logic**: More efficient recovery from garbage data streams
    - **Enhanced error handling**: Better progress through invalid data sequences
  - **Stability Impact**: Eliminated MAVFtp connection drops and system crashes
    - **Zero heap corruption**: Fixed root cause of ESP32 reboots during MAVLink processing
    - **Reliable protocol detection**: MAVLink packets now consistently detected and processed
    - **Stable network operations**: MAVFtp parameter uploads work without interruption

- **Logging System Refactoring**: Complete elimination of heap fragmentation
  - **Printf-Style Migration**: Migrated all ~306 log_msg calls across 21 files
    - Replaced String concatenation (`"Text " + String(var)`) with printf format (`"Text %d", var`)
    - Unified logging interface: `log_msg(LOG_LEVEL, "format", args)`
    - Added compile-time format string validation with `__attribute__((format(printf, 2, 3)))`
  - **Memory Optimization**: Eliminated heap allocation in logging hot paths
    - **Stack-based formatting**: All log messages use 256-byte stack buffers
    - **Zero heap fragmentation**: Removed String object creation in critical paths
    - **Hot path protection**: Adaptive buffer and protocol pipeline logging optimized
  - **Performance Impact**: Significant stability improvement for ESP32
    - **Reduced RAM usage**: Eliminated hundreds of String allocations per second
    - **Lower fragmentation**: ESP32 heap stays healthier during intensive operations
    - **Better reliability**: Reduced risk of memory-related crashes during MAVLink processing
  - **Code Cleanup**: Removed deprecated logging functions
    - Deleted old `log_msg(String, LogLevel)` and `log_msg(const char*, LogLevel)` functions
    - Single unified printf-style logging function for entire codebase
    - Improved code maintainability and consistency

## v2.9.0 (MAVLink Protocol Implementation) ✅
- **MAVLink Protocol Detector** - Phase 4.2 Complete
  - **Core MAVLink Implementation**: Full MAVLink v1/v2 packet detection
    - Created `src/protocols/mavlink_detector.h/cpp` with complete implementation
    - MAVLink packet boundary detection with header validation and payload size calculation
    - Support for MAVLink v1 (0xFE) and v2 (0xFD) protocol versions
    - Error handling with next start byte search for packet recovery
  - **Protocol Statistics System**: Comprehensive performance tracking
    - Created `src/protocols/protocol_stats.h` for real-time metrics
    - Tracks detected packets, transmitted packets, error count, resync events
    - Packet size statistics (average, min, max) and transmission rates
    - Integration with web interface for live protocol monitoring
  - **Configuration Management**: Seamless integration with existing config system
    - Added `protocolOptimization` field with version migration (v6→v7)
    - Protocol factory system with `src/protocols/protocol_factory.h/cpp`
    - Web interface dropdown for protocol selection (None/MAVLink)
    - Configuration persistence and loading across reboots
  - **Web Interface Enhancements**: User-friendly protocol management
    - Protocol Optimization dropdown in main interface (after UART Configuration)
    - Collapsible Protocol Statistics section with real-time updates
    - Statistics display: packets, errors, sizes, rates, last packet timing
    - Form integration with save/load functionality
  - **Performance Impact**: Dramatic latency reduction for MAVLink streams
    - **Eliminates UART FIFO overflows** at 115200 baud with high-speed MAVLink data
    - **Instant packet transmission** upon boundary detection (no timeout delays)
    - **Perfect packet preservation** - no data loss or fragmentation
    - Compatible with adaptive buffering fallback for non-MAVLink data
  - **HTML Minification Fix**: Improved web interface readability
    - Disabled aggressive HTML minification to preserve text spacing
    - Maintained gzip compression for optimal size (70-78% reduction)
    - Preserved JavaScript/CSS minification for performance

## v2.8.3 (Protocol Detection Framework) ✅
- **Protocol Detection Infrastructure** - Phase 4.1 Complete
  - **Framework Architecture**: Extensible protocol detection system
    - Created `src/protocols/` directory with base classes and interfaces
    - Implemented `ProtocolDetector` interface for protocol-specific implementations
    - Added comprehensive protocol pipeline with processing hooks
  - **Data Structure Extensions**: Enhanced core structures for protocol support
    - Added `ProtocolType` enum with support for MAVLink, Modbus RTU, NMEA, Text Line, SBUS
    - Extended `BridgeContext` with protocol detection state and error tracking
    - Added `protocolOptimization` configuration field with PROTOCOL_NONE default
  - **Integration Points**: Protocol-aware data processing
    - Integrated protocol hooks in `bridge_processing.h` for byte-level processing
    - Enhanced `adaptive_buffer.h` with protocol-specific transmission logic
    - Added protocol lifecycle management in `uartbridge.cpp`
  - **Stub Implementation**: Complete framework with zero functional impact
    - All protocol functions implemented as safe stubs returning default values
    - Zero performance overhead when disabled (PROTOCOL_NONE default)
    - Full backward compatibility with existing adaptive buffering
  - **Benefits**: Ready infrastructure for Phase 4.2+ protocol implementations
    - Easy addition of new protocols without architectural changes
    - Clear separation of concerns between framework and protocol logic
    - Prepared for future protocol-specific optimizations

## v2.8.2 (WiFi Manager ESP-IDF Migration) ✅
- **Complete WiFi Manager Migration** - Completed
  - **Full ESP-IDF Implementation**: Migrated from Arduino WiFi API to native ESP-IDF
    - Replaced WiFi.h with esp_wifi.h, esp_event.h, esp_netif.h
    - Event-driven architecture using ESP-IDF event handlers
    - Native WiFi configuration and state management
    - Eliminated temporary AP appearance in Client mode (main issue resolved)
  - **mDNS Service Implementation**: Device discovery by hostname in Client mode
    - Added mdns.h support with CONFIG_MDNS_ENABLED=y
    - Generated unique hostnames using device name + MAC suffix (2 bytes)
    - Safe mDNS initialization outside event handler context
    - HTTP service advertisement for web interface discovery
  - **DHCP Hostname Configuration**: Proper device identification in router
    - Implemented esp_netif_set_hostname() before WiFi start
    - Router now displays custom device name instead of "espresiff"
    - Hostname generation with MAC uniqueness for multiple devices
  - **Enhanced Error Handling**: Robust connection management and recovery
    - Bootloop protection with safe mode after 3 WiFi init failures
    - Proper memory checks before WiFi initialization
    - Connection timeout handling and automatic retry logic
    - Scan failure recovery with WiFi subsystem reset

## v2.8.1 (WiFi Client Mode Stability Fixes) ✅
- **WiFi Client Connection Logic** - Major stability improvements
  - **AP Mode Conflict Resolution**: Fixed dual AP+Client mode issue causing network conflicts
    - Added ESP-IDF level WiFi initialization with forced STA-only mode 
    - Implemented double AP disable: at client start and after successful connection
    - Eliminated unwanted AP broadcast during client connection process
  - **Intelligent Connection Management**: Enhanced retry logic with proper error distinction
    - **Initial Connection**: Scans every 15 seconds when network not found
    - **Connection Attempts**: Up to 5 attempts when target network is available  
    - **Authentication Failure**: Stops after 5 failed attempts to prevent router lockout
    - **Network Loss Recovery**: Unlimited reconnection attempts after successful initial connection
    - **Error State Handling**: Clear distinction between "wrong password" and "network not found"
  - **LED Indication Accuracy**: Corrected LED behavior for all connection states
    - Orange slow blink (2s): Searching for configured network
    - Orange solid: Successfully connected to WiFi network
    - Red fast blink (500ms): Authentication failed (wrong password) - restart required
    - Purple solid: AP mode active for direct configuration
  - **Performance Optimization**: Eliminated unnecessary scanning after successful connection
    - Fixed continuous scanning loop when already connected
    - Improved WiFi manager process state handling
- **Code Quality Improvements**
  - **ESP-IDF Integration**: Hybrid approach using ESP-IDF for initialization, Arduino for operations
- **Documentation Updates**
  - **README.md**: Comprehensive WiFi Client section with detailed connection logic
  - **Help Page**: Updated HTML help with key WiFi Client information and troubleshooting
  - **Troubleshooting**: Enhanced problem resolution guide with specific LED state meanings

## v2.8.0 (WiFi Client Mode Implementation) ✅
- **WiFi Client Mode** - Full implementation completed
  - **Dual WiFi Modes**: Support for both Access Point (AP) and Client (STA) modes
  - **WiFi Manager**: Complete state machine with scanning, connecting, and error handling
  - **Triple Click Logic**: Enhanced button logic for mode switching between Standalone/Client/AP
  - **LED Indication System**: 
    - Orange slow blink during network scanning (2s interval)
    - Solid orange when connected as WiFi client
    - Fast red blink for connection errors (500ms interval)
    - White blink for button click feedback in Client mode
  - **Device 4 Network Awareness**: Proper WiFi connection waiting using EventGroup synchronization
  - **Temporary/Permanent Modes**: Session-based temporary modes and persistent configuration
  - **Configuration**: Full web interface integration for WiFi Client credentials
  - **NVS Optimization**: Fixed preferences key length limitations (temp_net_mode)
  - **Benefits**:
    - Connect to existing WiFi networks while maintaining AP functionality
    - Seamless mode switching without configuration loss
    - Visual feedback for all WiFi states and user interactions
    - Reliable network operations with proper synchronization

## v2.7.3 (Configuration Import/Export + UI Improvements) ✅
- **Configuration Import/Export** - Completed
  - **Export Configuration**: Download current config as JSON file with unique filename
  - **Import Configuration**: Upload JSON config file with validation and auto-reboot  
  - **Web Interface**: Added "Configuration Backup" section with Export/Import buttons
  - **Async Reboot Fix**: Resolved reboot timing issues using TaskScheduler for delayed restart
  - **UI/UX Improvements**: Proper status messages, progress bars, and error handling
  - **Benefits**:
    - Quick device provisioning and configuration backup/restore
    - Share configurations between devices  
    - No need to reconfigure after firmware updates
    - Reliable status display without "Failed to fetch" errors

## v2.7.2 (Device 3/4 Code Refactoring) ✅
- **Device 3/4 Module Separation** - Completed
  - **File Structure**: Created independent device3_task.cpp/h and device4_task.cpp/h modules
  - **Code Reduction**: Reduced uartbridge.cpp from 600+ to 240 lines (360+ lines moved)
  - **Architecture Improvement**: Clear separation of device responsibilities for better maintainability
  - **Dependencies**: Updated all cross-file includes and extern declarations
  - **Linker Fixes**: Resolved device3Serial definition conflicts between modules
  - **Benefits**:
    - Easier maintenance and development
    - Reduced file complexity and improved readability
    - Clear module boundaries for future expansion
    - Preserved all existing functionality without changes

## v2.7.1 (Device 3/4 Statistics Unification) ✅
- **Statistics Thread Safety** - Completed
  - **Device 4 Protection**: Added critical sections for all TX/RX statistics updates
  - **Device 3 Migration**: Moved from local variables to global variables approach
  - **Race Condition Fixes**: Eliminated multi-threaded statistics conflicts
  - **Unified Architecture**: Consistent statistics handling across Device 3 and Device 4
  - **TaskScheduler Integration**: Implemented updateDevice3Stats() for periodic updates
  - **Benefits**:
    - Thread-safe statistics under high data load
    - Reliable counters without "jumping" values
    - Maintainable unified codebase
    - Stable operation in concurrent scenarios

## v2.7.0 (Device 4 Network Implementation) ✅
- **Device 4 Network Functionality** - Completed
  - **Network Logger Mode**: Send system logs via UDP (broadcast or unicast)
  - **Network Bridge Mode**: Bidirectional UART<->UDP communication
  - **Configuration**: Full web interface integration with IP/Port settings
  - **Statistics**: Real-time TX/RX bytes and packet counters
  - **Architecture**:
    - AsyncUDP library for network operations
    - Ring buffers for non-blocking data transfer
    - Task runs on Core 1 with web server
    - DMA-style approach for efficient data handling
  - **Integration**:
    - Device 4 configuration saved/loaded with config version 5
    - Network Log Level enabled when Device 4 is active
    - Full statistics in web interface status display
    - Works in both standalone and network modes
  - **Technical Details**:
    - 2KB buffers for logs and bridge data
    - 50ms task delay for low latency
    - Mutex protection for thread-safe buffer access
    - Forward declarations to resolve compilation order
  - **Fixed Issues**:
    - Web interface now correctly displays selected Device 4 role
    - Removed duplicate role name transmission in config JSON

## v2.6.0 (ESPAsyncWebServer Migration) ✅
- **Migrated to ESPAsyncWebServer** - Completed
  - **Libraries**: Updated to ESPAsyncWebServer v3.7.10 + AsyncTCP v3.4.5
  - **Template System**: Changed from custom `{{}}` to built-in `%PLACEHOLDER%` processor
  - **API Migration**: Converted all handlers to async (request parameter access, response sending)
  - **OTA Adaptation**: Redesigned file upload handling for async server
  - **Architecture**: Removed webServerTask - AsyncWebServer works without dedicated task
  - **Memory Benefits**: Better resource usage, no blocking operations
  - **Performance**: Non-blocking request handling, improved concurrent connections
  - **JavaScript Fixes**: Fixed Reset Statistics and Clear Crash History button handling
  - **Diagnostics**: Enhanced stack monitoring for WiFi/TCP tasks instead of web server task

## v2.5.8 (Permanent Network Mode) ✅
- **Permanent Network Mode Implementation** - Completed
  - Added `permanent_network_mode` configuration parameter in Config structure
  - Updated configuration version from 3 to 4 with automatic migration
  - Web interface: Added checkbox in WiFi Configuration section
  - Backend: Full configuration save/load support with proper validation
  - Boot logic: Automatic permanent network mode detection in `detectMode()`
  - Benefits:
    - Wi-Fi access point remains active until manually disabled
    - No timeout for permanent mode (only for triple-click temporary mode)
    - Configurable via web interface with clear user feedback
    - Backward compatible with existing configurations

- **Build System Enhancement** - Completed
  - Added `update_readme_version.py` script for automatic version synchronization
  - Script automatically updates README.md badge with version from defines.h
  - Integrated into PlatformIO build process as pre-build script
  - Prevents version mismatches between firmware and documentation
  - **Fixed**: Removed blocking `exit(0)` that prevented compilation and upload

## v2.5.7 (Device Init Refactoring) ✅
- **Refactor Device Initialization** - Completed
  - Migrated `uartbridge_init()` from `uartbridge.cpp` to `device_init.cpp`
  - Renamed `uartbridge_init()` to `initMainUART()` for consistency
  - Migrated `initDevices()` from `main.cpp` to `device_init.cpp`
  - Updated all function calls in `main.cpp` (2 occurrences)
  - Removed function declarations from original locations
  - Benefits:
    - Better code organization with all device init functions in one module
    - Consistent naming convention for initialization functions
    - Reduced file sizes: uartbridge.cpp (~55 lines), main.cpp (~30 lines)
  - Technical details:
    - Added necessary includes: `usb_interface.h`, `flow_control.h`, `freertos/semphr.h`
    - Made `g_usbInterface` global (was static) for cross-module access
    - Added `diagnostics.h` include for helper functions
    - Total lines moved: ~85 lines to device_init module

## v2.5.6 (Bridge Mode Renaming) ✅
- **Rename Device Modes to Bridge Modes** - Completed
  - Renamed `DeviceMode` enum to `BridgeMode`
  - Renamed `MODE_NORMAL` to `BRIDGE_STANDALONE`
  - Renamed `MODE_CONFIG` to `BRIDGE_NET`
  - Updated all references across 11 source files
  - Benefits:
    - Clear separation of bridge functionality vs network state
    - Future-ready for Device 4 permanent network modes
    - Better naming that accurately describes functionality
    - Flexible design supporting different network configurations

- **Update SystemState Structure** - Completed
  - Renamed `wifiAPActive` to `networkActive`
  - Renamed `wifiStartTime` to `networkStartTime`
  - Added `isTemporaryNetwork` flag for future Device 4 support
  - Allows differentiation between setup AP (temporary) and permanent network modes

- **Update All User-Visible Text** - Completed
  - Changed "normal mode" to "standalone mode" throughout
  - Changed "config mode" to "network mode" throughout
  - Changed "WiFi configuration" to "network setup" where appropriate
  - Simplified to just "Network Mode" (without "setup") for universal usage
  - Updated help documentation and web interface

- **Implementation Details** - Completed
  - Total changes: ~60 occurrences across 11 files
  - Added critical comment for `_TASK_INLINE` macro in scheduler_tasks.h
  - All mode checks updated to use new enum values
  - Function names updated (initNormalMode → initStandaloneMode, etc.)
  - TaskScheduler functions renamed (enableRuntimeTasks → enableStandaloneTasks)
  - Network timeout only active when `isTemporaryNetwork=true`

## v2.5.5 (Adaptive Buffer Optimization) ✅
- **Fix FIFO Overflow at 115200 baud** - Completed
  - Identified root cause: increased buffer size (512 bytes) causing USB bottleneck
  - Created graduated buffer sizing for smoother performance:
    - < 115200: 256 bytes (original size)
    - 115200: 288 bytes (optimal compromise)
    - 230400: 768 bytes
    - 460800: 1024 bytes
    - 921600+: 2048 bytes
  - Results at 115200 baud:
    - Reduced FIFO overflow from constant to rare (3-8 times per test)
    - Improved packet loss from 25% to 2-10%
    - Parameter download success rate: 83% (5 out of 6 attempts)
  - Determined 288 bytes as optimal for 115200 baud without flow control

- **Code Cleanup** - Completed
  - Fixed duplicated adaptive buffering code in `bridge_processing.h`
  - Now properly uses functions from `adaptive_buffer.h`
  - Removed ~55 lines of duplicated code
  - Added temporary diagnostics (marked for removal after testing)

## v2.5.4 (TaskScheduler Implementation) ✅
- **Implement TaskScheduler** - Completed
  - Added TaskScheduler library to replace manual timer checks
  - Created `scheduler_tasks.cpp/h` for centralized task management
  - Migrated all periodic tasks (diagnostics, statistics, LED updates)
  - Saved ~100 lines of timer management code
  - Benefits achieved:
    - Cleaner code structure
    - Easy to add/remove periodic tasks
    - Better timing accuracy
    - Reduced cognitive load
  - All tasks properly distributed in time to prevent simultaneous execution
  - Mode-specific task management (Runtime vs Setup modes)

## v2.5.3 (Phase 2 Code Refactoring - Hybrid Approach) ✅
- **Phase 2 Refactoring - Hybrid Approach** - Completed
  - Further refactored uartbridge.cpp from ~555 to ~250 lines
  - Created BridgeContext structure for clean parameter passing
  - Maintained performance with static inline functions
  
- **Extended types.h** - Completed
  - Added comprehensive BridgeContext structure
  - Groups all task-local variables logically
  - Added inline initBridgeContext() function
  - Forward declarations for UartInterface/UsbInterface
  
- **Enhanced Diagnostics Module** - Completed
  - Moved periodic diagnostics from uartbridge.cpp
  - Added logBridgeActivity(), logStackDiagnostics()
  - Added logDmaStatistics(), logDroppedDataStats()
  - Created updatePeriodicDiagnostics() main function
  - ~150 lines moved to diagnostics module
  
- **Created bridge_processing.h** - Completed
  - All data processing logic as inline functions
  - processDevice1Input(), processDevice2USB/UART()
  - processDevice3BridgeInput()
  - shouldYieldToWiFi() helper
  - ~200 lines of processing logic extracted
  
- **Created adaptive_buffer.h** - Completed
  - All adaptive buffering logic in one place
  - shouldTransmitBuffer(), transmitAdaptiveBuffer()
  - handleBufferTimeout()
  - calculateAdaptiveBufferSize() moved from device_init
  - ~120 lines of buffering logic extracted
  
- **Simplified Main Loop** - Completed
  - Clean, readable main loop with function calls
  - All complex logic in specialized modules
  - Performance preserved with inline functions
  - Ready for future extensions

## v2.5.2 (Phase 1 Code Refactoring) ✅
- **Refactor Large uartbridge.cpp File** - Phase 1 Complete
  - Original size: ~700 lines, reduced to ~555 lines
  - Created modular structure without breaking functionality
  
- **Flow Control Module** - Completed
  - Created `flow_control.h` and `flow_control.cpp`
  - Extracted `detectFlowControl()` and `getFlowControlStatus()` functions
  - ~50 lines of code moved to separate module
  - Clean separation of flow control logic

- **Device Initialization Module** - Completed
  - Created `device_init.h` and `device_init.cpp`
  - Extracted `initDevice2UART()` and `initDevice3()` functions
  - ~85 lines of code moved to separate module
  - Fixed linkage issues with proper extern declarations

- **Build System Verification** - Completed
  - Fixed static/extern conflicts for device2Serial
  - Added proper extern declarations in uartbridge.h
  - Project compiles successfully with new structure
  - All functionality preserved

## v2.5.1 (Web Interface Modularization) ✅
- **Split main.js into modules** - Completed
  - Created separate JS modules for better organization:
    - `utils.js` - Common utility functions
    - `device-config.js` - Device configuration UI
    - `form-utils.js` - Form handling and validation
    - `status-updates.js` - Status and statistics updates
    - `crash-log.js` - Crash log handling
    - `main.js` - Main initialization and coordination
  - Updated `web_interface.cpp` to serve all JS files
  - Updated `embed_html.py` to process all JS files
  - Added proper handlers in `web_interface.h`

- **Fix Reset Statistics button** - Completed
  - Removed confirmation dialog as requested
  - Fixed button state stuck on "Resetting..."
  - Button now properly returns to "Reset Statistics" after operation

- **OTA Update Safety** - Completed
  - Added Device 3 UART0 deinitialization before OTA update
  - Prevents conflicts during firmware update when Device 3 uses UART0
  - Made `device3Serial` accessible from `web_ota.cpp`
  - Ensures clean OTA update even with active Device 3

- **Code Cleanup** - Completed
  - Removed unused `lastWdtFeed` variable from `uartbridge.cpp`
  - Fixed legacy code remnants

## v2.5.0 (Complete ESP-IDF Migration + Performance) ✅
- **Complete ESP-IDF Migration** - Completed
  - Migrated Device 2 UART to ESP-IDF/DMA ✅
  - Migrated Device 3 UART to ESP-IDF/DMA ✅
  - Migrated UART logger to ESP-IDF/DMA ✅
  - Removed HardwareSerial dependencies completely ✅
  - Removed serial_config_decoder.h ✅

- **DMA Polling Mode Implementation** - Completed
  - Added polling mode support to UartDMA class ✅
  - Device 2/3 use polling mode (no event tasks) ✅
  - Added `pollEvents()` method for non-blocking operation ✅
  - Optimized for minimal resource usage ✅

- **Configuration Architecture Improvement** - Completed
  - Created `UartConfig` structure for UART parameters ✅
  - Separated DMA configuration from UART configuration ✅
  - Fixed scope conflicts with global config ✅
  - Clean parameter passing without global dependencies ✅

- **Dynamic Adaptive Buffer Sizing** - Completed
  - Buffer size now adjusts based on baud rate (256-2048 bytes) ✅
  - 256 bytes for ≤19200 baud
  - 512 bytes for ≤115200 baud
  - 1024 bytes for ≤460800 baud
  - 2048 bytes for >460800 baud
  - Prevents packet fragmentation at high speeds
  - Maintains low latency at all speeds

## v2.4.0 (ESP-IDF Migration) ✅
- **Remove Arduino Framework Dependencies for Device1** - Completed
  - Migrated Device 1 UART to ESP-IDF driver with DMA
  - Removed all conditional compilation (#ifdef USE_UART_DMA)
  - Deleted uart_arduino.cpp wrapper
  - Improved UART performance with hardware packet detection

- **Configuration System Update** - Completed
  - Updated Config struct to use ESP-IDF native types (uart_word_length_t, uart_parity_t, uart_stop_bits_t)
  - Increased config version to 3 with automatic migration
  - Added conversion functions between ESP-IDF enums and web interface strings

- **Code Cleanup and Optimization** - Completed
  - Created UsbBase class to eliminate code duplication between USB Host and Device
  - Simplified UartInterface with direct ESP-IDF configuration
  - Removed serial_config_decoder.h dependency for Device 1
  - Improved error handling and logging

- **Performance Improvements** - Completed
  - Hardware DMA for Device 1 UART with packet boundary detection
  - Zero-copy ring buffer implementation
  - Reduced CPU usage through event-driven architecture
  - Native ESP-IDF drivers for better performance
  - Increased DMA buffers and task priority for high-throughput scenarios

- **USB Implementation Decision** - Completed
  - Kept Arduino Serial for USB Device (stable and sufficient)
  - USB Host already uses ESP-IDF
  - Created common base class for code reuse
  - USB performance not critical compared to UART

## v2.3.3 (Performance Optimization) ✅
- **UART Bridge Performance Fix** - Completed
  - Fixed packet drops during MAVLink parameter downloads
  - Optimized device role checks with cached flags
  - Removed repeated config.deviceX.role comparisons in critical loop
  - Improved performance at 115200 baud and higher

- **Buffer Size Optimization** - Completed
  - Increased UART_BUFFER_SIZE from 256 to 1024 bytes
  - Added Serial.setTxBufferSize(1024) for USB interface
  - Removed baudrate condition - always use 1KB buffers
  - Reduced WiFi yield time from 5ms to 3ms

## v2.3.2 (Web Interface Migration) ✅
- **Web Interface Refactoring** - Completed
  - Migrated from embedded HTML strings to separate HTML/CSS/JS files
  - Created build-time processing with `embed_html.py` script
  - Reduced placeholders from 40+ to just {{CONFIG_JSON}}
  - Separated JavaScript into main.js and crash-log.js
  - HTML/CSS/JS files now editable with proper IDE support
  - C++ code reduced by ~3000 lines

## v2.3.1 ✅
- **Update Statistics System** - Completed
  - Replaced old `bytesUartToUsb`/`bytesUsbToUart` with per-device counters
  - Added `device1RxBytes`/`device1TxBytes`, `device2RxBytes`/`device2TxBytes`, `device3RxBytes`/`device3TxBytes`
  - Updated web interface to show statistics for all active devices
  - Shows device roles in statistics display

- **Code Cleanup** - Completed
  - Moved `updateSharedStats` and `resetStatistics` to diagnostics module
  - Removed legacy `if (currentMode == MODE_NORMAL || currentMode == MODE_CONFIG)` check
  - Fixed deprecated ArduinoJson `containsKey()` warning
  - Added helper functions `getDevice2RoleName()` and `getDevice3RoleName()`

- **Web Interface Updates** - Completed
  - Updated traffic statistics table to show per-device data
  - Dynamic show/hide of device rows based on active roles
  - Updated JavaScript to handle new JSON structure from `/status` endpoint
  - Fixed "Never" display for last activity

## v2.3.0 ✅
- **Remove DEBUG_MODE** - Completed
  - Removed all DEBUG_MODE checks from code
  - Bridge always active in all modes
  - Diagnostics converted to log levels
  
- **Remove CONFIG_FREERTOS_UNICORE** - Completed
  - Now only supports multi-core ESP32
  - UART task on core 0, Web task on core 1
  
- **Code Organization** - Completed
  - Extracted diagnostics to separate module (diagnostics.cpp/h)
  - Moved system utilities to system_utils.cpp/h
  - Moved RTC variables and crash update to crashlog.cpp/h
  - main.cpp reduced from ~600 to ~450 lines



## Current Status Summary

The project now uses a full ESP-IDF approach for all UART operations and modern AsyncWebServer:
- **Device 1 (Main UART)**: Full ESP-IDF with DMA and event task ✅
- **Device 2 (Secondary)**: ESP-IDF with DMA polling mode ✅
- **Device 3 (Mirror/Bridge/Log)**: ESP-IDF with DMA polling mode ✅
- **USB Device**: Arduino Serial (proven stable) ✅
- **USB Host**: ESP-IDF implementation ✅
- **UART Logger**: ESP-IDF with DMA polling mode ✅
- **Permanent Network Mode**: Fully implemented and configurable ✅
- **Web Server**: ESPAsyncWebServer for non-blocking operations ✅