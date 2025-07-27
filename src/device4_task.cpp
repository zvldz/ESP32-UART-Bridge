#include "device4_task.h"
#include "config.h"
#include "logging.h"
#include "types.h"
#include "diagnostics.h"

// External objects
extern Config config;
extern UartStats uartStats;
extern SystemState systemState;

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

// AsyncUDP instance
AsyncUDP* device4UDP = nullptr;

// Device 4 Bridge buffers (only if Bridge mode is used)
uint8_t device4BridgeTxBuffer[DEVICE4_BRIDGE_BUFFER_SIZE];
uint8_t device4BridgeRxBuffer[DEVICE4_BRIDGE_BUFFER_SIZE];
int device4BridgeTxHead = 0;
int device4BridgeTxTail = 0;
int device4BridgeRxHead = 0;
int device4BridgeRxTail = 0;
SemaphoreHandle_t device4BridgeMutex = nullptr;

void device4Task(void* parameter) {
    log_msg("Device 4 task started on core " + String(xPortGetCoreID()), LOG_INFO);
    
    // Wait for network ready with timeout
    const uint32_t maxWaitTime = 3000;  // 3 seconds
    uint32_t waited = 0;
    
    while (!systemState.networkActive && waited < maxWaitTime) {
        vTaskDelay(pdMS_TO_TICKS(100));
        waited += 100;
    }
    
    if (!systemState.networkActive) {
        log_msg("Device 4: Network not ready after 3s, exiting", LOG_ERROR);
        vTaskDelete(NULL);
        return;
    }
    
    // Additional delay for WiFi stack stabilization
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    log_msg("Device 4: Network ready, initializing AsyncUDP", LOG_INFO);
    
    // Create AsyncUDP instance
    device4UDP = new AsyncUDP();
    if (!device4UDP) {
        log_msg("Device 4: Failed to create AsyncUDP", LOG_ERROR);
        vTaskDelete(NULL);
        return;
    }
    
    // Create Bridge mutex if needed
    if (config.device4.role == D4_NETWORK_BRIDGE) {
        device4BridgeMutex = xSemaphoreCreateMutex();
        if (!device4BridgeMutex) {
            log_msg("Device 4: Failed to create bridge mutex", LOG_ERROR);
            delete device4UDP;
            vTaskDelete(NULL);
            return;
        }
    }
    
    // Determine broadcast or unicast
    bool isBroadcast = (strcmp(config.device4_config.target_ip, "192.168.4.255") == 0) ||
                       (strstr(config.device4_config.target_ip, ".255") != nullptr);
    
    // Setup listener for Bridge mode
    if (config.device4.role == D4_NETWORK_BRIDGE) {
        if (!device4UDP->listen(config.device4_config.port)) {
            log_msg("Device 4: Failed to listen on port " + 
                    String(config.device4_config.port), LOG_ERROR);
        } else {
            log_msg("Device 4: Listening on port " + 
                    String(config.device4_config.port), LOG_INFO);
            
            device4UDP->onPacket([](AsyncUDPPacket packet) {
                if (config.device4.role == D4_NETWORK_BRIDGE && device4BridgeMutex) {
                    if (xSemaphoreTake(device4BridgeMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        size_t len = packet.length();
                        uint8_t* data = packet.data();
                        
                        // Store incoming UDP data in Bridge RX buffer
                        for (size_t i = 0; i < len; i++) {
                            int nextHead = (device4BridgeRxHead + 1) % DEVICE4_BRIDGE_BUFFER_SIZE;
                            if (nextHead != device4BridgeRxTail) {  // Buffer not full
                                device4BridgeRxBuffer[device4BridgeRxHead] = data[i];
                                device4BridgeRxHead = nextHead;
                            } else {
                                // Buffer full, drop packet
                                log_msg("Device 4: Bridge RX buffer full, dropping packet", LOG_WARNING);
                                break;
                            }
                        }
                        
                        xSemaphoreGive(device4BridgeMutex);
                        
                        // Update statistics
                        enterStatsCritical();
                        globalDevice4RxBytes += len;
                        globalDevice4RxPackets++;
                        exitStatsCritical();
                    }
                }
            });
        }
    }
    
    // Main loop for log transmission and Bridge mode
    while (1) {
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
        
        // Bridge mode: Check Bridge TX buffer and send UDP->UART data
        if (config.device4.role == D4_NETWORK_BRIDGE && device4BridgeMutex) {
            if (xSemaphoreTake(device4BridgeMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                // Send Bridge TX data via UDP
                if (device4BridgeTxHead != device4BridgeTxTail) {
                    uint8_t tempBuffer[512];
                    int count = 0;
                    
                    while (device4BridgeTxHead != device4BridgeTxTail && count < sizeof(tempBuffer)) {
                        tempBuffer[count++] = device4BridgeTxBuffer[device4BridgeTxTail];
                        device4BridgeTxTail = (device4BridgeTxTail + 1) % DEVICE4_BRIDGE_BUFFER_SIZE;
                    }
                    
                    xSemaphoreGive(device4BridgeMutex);
                    
                    // Send collected data via UDP
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
                    xSemaphoreGive(device4BridgeMutex);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));  // 50ms for low latency
    }
}

void updateDevice4Stats() {
    enterStatsCritical();
    uartStats.device4TxBytes = globalDevice4TxBytes;
    uartStats.device4TxPackets = globalDevice4TxPackets;
    uartStats.device4RxBytes = globalDevice4RxBytes;
    uartStats.device4RxPackets = globalDevice4RxPackets;
    exitStatsCritical();
}