// ESP32 UART Bridge - Main Module

// Global configuration - will be loaded via AJAX
let config = null;

// Main initialization function
async function initializeUI() {
    try {
        // Load initial configuration from server
        const response = await fetch('/status');
        config = await response.json();
        
        // Initialize all modules with config
        DeviceConfig.init(config);
        FormUtils.init(config);
        StatusUpdates.init(config);
        CrashLog.init();

        // Initialize SBUS Source Management (NEW)
        if (typeof SbusSource !== 'undefined') {
            SbusSource.init(config);
        }

        // Start periodic updates
        startPeriodicUpdates();
    } catch (error) {
        console.error('Failed to load configuration:', error);
        // Show error message to user
        document.body.innerHTML = '<div style="padding: 20px; text-align: center; color: red;"><h2>Failed to load configuration</h2><p>Please refresh the page</p></div>';
    }
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

// Toggle protocol statistics visibility
function toggleProtocolStats() {
    const statsDiv = document.getElementById('protocolStats');
    const toggleIcon = document.getElementById('protocolStatsToggle');
    
    if (statsDiv && toggleIcon) {
        if (statsDiv.style.display === 'none') {
            statsDiv.style.display = 'block';
            toggleIcon.textContent = '▼';
        } else {
            statsDiv.style.display = 'none';
            toggleIcon.textContent = '▶';
        }
    }
}