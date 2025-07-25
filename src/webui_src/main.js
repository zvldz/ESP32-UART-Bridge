// ESP32 UART Bridge - Main Module

// Global configuration from embedded JSON
const config = JSON.parse(document.getElementById('config').textContent);

// Main initialization function
function initializeUI() {
    // Initialize all modules with config
    DeviceConfig.init(config);
    FormUtils.init(config);
    StatusUpdates.init(config);
    CrashLog.init();
    
    // Start periodic updates
    startPeriodicUpdates();
    
    // Log initialization complete
    console.log('ESP32 UART Bridge UI initialized');
}

// Start periodic updates for dynamic content
function startPeriodicUpdates() {
    // Combined update interval for better performance
    setInterval(() => {
        StatusUpdates.updateAll();
        CrashLog.updateCount();
    }, 5000);
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', initializeUI);

// Export config for debugging if needed
window.bridgeConfig = config;