#include "device4_task.h"
#include "config.h"
#include "logging.h"
#include "types.h"
#include "diagnostics.h"
#include "../wifi/wifi_manager.h"
#include "../protocols/protocol_pipeline.h"
#include "../protocols/udp_sender.h"

// External objects
extern Config config;
extern UartStats uartStats;
extern SystemState systemState;

// External UART1 interface for direct UDP → UART write
extern UartInterface* uartBridgeSerial;

// Device 4 log buffer
uint8_t device4LogBuffer[DEVICE4_LOG_BUFFER_SIZE];
int device4LogHead = 0;
int device4LogTail = 0;
SemaphoreHandle_t device4LogMutex = nullptr;

// Device 4 statistics
unsigned long globalDevice4TxBytes = 0;
unsigned long globalDevice4TxPackets = 0;
unsigned long globalDevice4RxBytes = 0;
unsigned long globalDevice4RxPackets = 0;

// Device 1 TX statistics for UDP→UART forwarding
unsigned long device1TxBytesFromDevice4 = 0;

// AsyncUDP instance
AsyncUDP* device4UDP = nullptr;

// REMOVED: Bridge buffers - now using Pipeline + UdpTxQueue

// Global Pipeline access from uartbridge.cpp
extern ProtocolPipeline* g_pipeline;

void device4Task(void* parameter) {
    log_msg(LOG_INFO, "Device 4 task started on core %d", xPortGetCoreID());
    
    // Wait for UDP TX queue initialization
    log_msg(LOG_INFO, "Device4: Waiting for UDP TX queue...");
    while (UdpSender::getTxQueue() == nullptr) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    __sync_synchronize();  // Acquire semantics
    
    UdpTxQueue* txQueue = UdpSender::getTxQueue();
    log_msg(LOG_INFO, "Device4: TX queue ready");
    
    // Wait for Pipeline
    log_msg(LOG_INFO, "Device4: Waiting for Pipeline...");
    while (g_pipeline == nullptr) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    __sync_synchronize();
    log_msg(LOG_INFO, "Device4: Pipeline ready");
    
    // Wait for network mode to be active first
    const uint32_t maxNetworkWaitTime = 3000;  // 3 seconds
    uint32_t networkWaited = 0;
    
    while (!systemState.networkActive && networkWaited < maxNetworkWaitTime) {
        vTaskDelay(pdMS_TO_TICKS(100));
        networkWaited += 100;
    }
    
    if (!systemState.networkActive) {
        log_msg(LOG_ERROR, "Device 4: Network mode not active after 3s, exiting");
        vTaskDelete(NULL);
        return;
    }
    
    log_msg(LOG_INFO, "Device 4: Network mode active, waiting for WiFi connection...");
    
    // Wait for actual WiFi connection (AP mode or Client connected)
    EventBits_t bits;
    if (config.wifi_mode == BRIDGE_WIFI_MODE_CLIENT) {
        // In client mode, wait for connection
        log_msg(LOG_INFO, "Device 4: Waiting for WiFi client connection...");
        bits = xEventGroupWaitBits(networkEventGroup, 
                                  NETWORK_CONNECTED_BIT, 
                                  pdFALSE, pdTRUE, 
                                  pdMS_TO_TICKS(30000));  // 30 second timeout
        
        if (!(bits & NETWORK_CONNECTED_BIT)) {
            log_msg(LOG_ERROR, "Device 4: WiFi client connection timeout after 30s, exiting");
            vTaskDelete(NULL);
            return;
        }
        
        log_msg(LOG_INFO, "Device 4: WiFi client connected successfully");
    } else {
        // In AP mode, WiFi is immediately ready
        log_msg(LOG_INFO, "Device 4: WiFi AP mode active");
    }
    
    // Additional delay for WiFi stack stabilization
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    log_msg(LOG_INFO, "Device 4: Network ready, initializing AsyncUDP");
    
    // Create AsyncUDP instance
    device4UDP = new AsyncUDP();
    if (!device4UDP) {
        log_msg(LOG_ERROR, "Device 4: Failed to create AsyncUDP");
        vTaskDelete(NULL);
        return;
    }
    
    // REMOVED: Bridge mutex - no longer needed with Pipeline architecture
    
    // Determine broadcast or unicast
    bool isBroadcast = (strcmp(config.device4_config.target_ip, "192.168.4.255") == 0) ||
                       (strstr(config.device4_config.target_ip, ".255") != nullptr);
    
    // Setup listener for Bridge mode - NEW ARCHITECTURE
    if (config.device4.role == D4_NETWORK_BRIDGE) {
        if (!device4UDP->listen(config.device4_config.port)) {
            log_msg(LOG_ERROR, "Device 4: Failed to listen on port %d", config.device4_config.port);
        } else {
            log_msg(LOG_INFO, "Device 4: Listening on port %d", config.device4_config.port);
            
            device4UDP->onPacket([](AsyncUDPPacket packet) {
                if (config.device4.role == D4_NETWORK_BRIDGE && uartBridgeSerial) {
                    size_t len = packet.length();
                    uint8_t* data = packet.data();
                    
                    // Direct write to UART1 interface
                    uartBridgeSerial->write(data, len);
                    
                    // Update RX statistics
                    __sync_fetch_and_add(&globalDevice4RxBytes, len);
                    __sync_fetch_and_add(&globalDevice4RxPackets, 1);
                    
                    // Update UART1 TX statistics (thread-safe)
                    __sync_fetch_and_add(&device1TxBytesFromDevice4, len);
                    
                    // Log for verification
                    static int count = 0;
                    if (++count % 10 == 0) {
                        log_msg(LOG_INFO, "[Device4] Forwarded %d UDP packets to UART1", count);
                    }
                }
            });
        }
    }
    
    // Parse target IP for UDP transmission (remove duplicate isBroadcast)
    IPAddress targetIP;
    if (!isBroadcast) {
        targetIP.fromString(config.device4_config.target_ip);
    }
    
    // Main loop - NEW ARCHITECTURE
    while (1) {
        // Check if WiFi client mode is still connected
        if (config.wifi_mode == BRIDGE_WIFI_MODE_CLIENT) {
            if (!wifi_manager_is_connected()) {
                log_msg(LOG_WARNING, "Device 4: WiFi disconnected, dropping queue...");
                
                // FIXED: Clear queue when WiFi is down to prevent stale data accumulation
                uint8_t dumpPacket[1500];
                int droppedCount = 0;
                while (txQueue->dequeue(dumpPacket, sizeof(dumpPacket)) > 0) {
                    droppedCount++;
                }
                if (droppedCount > 0) {
                    log_msg(LOG_INFO, "Device 4: Dropped %d stale packets", droppedCount);
                }
                
                // Wait for reconnection (reduced timeout from 10s to 1s)
                EventBits_t bits = xEventGroupWaitBits(networkEventGroup, 
                                          NETWORK_CONNECTED_BIT, 
                                          pdFALSE, pdTRUE, 
                                          pdMS_TO_TICKS(1000));  // 1 second timeout
                
                if (!(bits & NETWORK_CONNECTED_BIT)) {
                    // Not reconnected - continue loop (will drop new packets)
                    vTaskDelay(pdMS_TO_TICKS(100));  // Small delay
                    continue;  // Start loop over
                } else {
                    log_msg(LOG_INFO, "Device 4: WiFi reconnected");
                }
            }
        }
        
        // === NEW: Pipeline → UDP transmission ===
        uint8_t packet[1500];
        size_t size;
        int packetsProcessed = 0;  // Count processed packets for adaptive delay
        
        // Drain queue efficiently
        while ((size = txQueue->dequeue(packet, sizeof(packet))) > 0) {
            packetsProcessed++;  // Count processed packets
            
            // Send via AsyncUDP
            if (isBroadcast) {
                device4UDP->broadcastTo(packet, size, config.device4_config.port);
            } else {
                device4UDP->writeTo(packet, size, targetIP, config.device4_config.port);
            }
            
            // Update packet counter (bytes already counted in UdpSender)
            __sync_fetch_and_add(&globalDevice4TxPackets, 1);
            
            // Optional: yield to other tasks if sending many packets
            if (packetsProcessed % 10 == 0) {
                taskYIELD();
            }
        }
        
        // Logger mode: Check log buffer
        if (config.device4.role == D4_LOG_NETWORK && device4LogMutex) {
            if (xSemaphoreTake(device4LogMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                if (device4LogHead != device4LogTail) {
                    // Collect data from buffer
                    uint8_t tempBuffer[512];
                    int count = 0;
                    
                    while (device4LogHead != device4LogTail && count < sizeof(tempBuffer)) {
                        tempBuffer[count++] = device4LogBuffer[device4LogTail];
                        device4LogTail = (device4LogTail + 1) % DEVICE4_LOG_BUFFER_SIZE;
                    }
                    
                    xSemaphoreGive(device4LogMutex);
                    
                    // Send collected data
                    if (count > 0) {
                        size_t sent = 0;
                        
                        if (isBroadcast) {
                            sent = device4UDP->broadcastTo(tempBuffer, count, 
                                                          config.device4_config.port);
                        } else {
                            IPAddress targetIP;
                            if (targetIP.fromString(config.device4_config.target_ip)) {
                                sent = device4UDP->writeTo(tempBuffer, count, targetIP, 
                                                         config.device4_config.port);
                            }
                        }
                        
                        if (sent == count) {
                            enterStatsCritical();
                            globalDevice4TxBytes += count;
                            globalDevice4TxPackets++;
                            exitStatsCritical();
                        }
                    }
                } else {
                    xSemaphoreGive(device4LogMutex);
                }
            }
        }
        
        // REMOVED: Bridge mode TX buffer handling - now using UdpTxQueue via Pipeline
        
        // FIXED: Adaptive delay based on actual packets processed
        if (packetsProcessed > 0) {
            // Had packets - minimal delay to keep processing efficiently
            taskYIELD();  // Give other tasks a chance but return quickly
        } else {
            // Queue was empty - longer sleep to reduce CPU usage
            vTaskDelay(pdMS_TO_TICKS(5));   // 5ms when idle
        }
    }
}

