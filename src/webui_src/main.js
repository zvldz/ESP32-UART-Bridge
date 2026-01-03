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


        // Restore collapsible section states from localStorage
        Utils.restoreToggle('systemStatusBlock', 'systemStatusToggle', 'collapse:systemStatus');
        Utils.restoreToggle('protocolStats', 'protocolStatsToggle', 'collapse:protocolStats');
        Utils.restoreToggle('logsBlock', 'logsArrow', 'collapse:logs');
        Utils.restoreToggle('advancedConfigContent', 'advancedConfigArrow', 'collapse:advancedConfig');
        CrashLog.restoreState();

        // Update auto broadcast state after all sections restored
        if (typeof DeviceConfig !== 'undefined') {
            DeviceConfig.updateAutoBroadcastState();
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

        // Only update crash count if section is visible
        const crashContent = document.getElementById('crashContent');
        if (!(crashContent && crashContent.style.display === 'none')) {
            CrashLog.updateCount();
        }
    }, 5000);
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', initializeUI);

// Export config for debugging if needed
window.bridgeConfig = config;

// Toggle system status visibility
async function toggleSystemStatus() {
    const el = document.getElementById('systemStatusBlock');
    const isOpening = el && el.style.display === 'none';

    // Load fragment first if opening
    if (isOpening && el && el.dataset.fragment) {
        await Utils.loadFragment(el);
    }

    Utils.rememberedToggle('systemStatusBlock', 'systemStatusToggle', 'collapse:systemStatus');

    // Trigger immediate update after fragment loads
    if (isOpening) {
        StatusUpdates.updateAll();
    }
}

// Toggle protocol statistics visibility
async function toggleProtocolStats() {
    const el = document.getElementById('protocolStats');
    const isOpening = el && el.style.display === 'none';

    // Load fragment first if opening
    if (isOpening && el && el.dataset.fragment) {
        await Utils.loadFragment(el);
    }

    Utils.rememberedToggle('protocolStats', 'protocolStatsToggle', 'collapse:protocolStats');

    // Trigger immediate update after fragment loads
    if (isOpening) {
        StatusUpdates.updateAll();
    }
}

// Toggle advanced configuration visibility
function toggleAdvancedConfig() {
    Utils.rememberedToggle('advancedConfigContent', 'advancedConfigArrow', 'collapse:advancedConfig');
    // Update auto broadcast state when section becomes visible
    if (typeof DeviceConfig !== 'undefined') {
        DeviceConfig.updateAutoBroadcastState();
    }
}


