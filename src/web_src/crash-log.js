// ESP32 UART Bridge - Crash Log Functions

// Toggle crash log visibility
function toggleCrashLog() {
    const content = document.getElementById('crashContent');
    const arrow = document.getElementById('crashArrow');
    
    if (content.style.display === 'none') {
        content.style.display = 'block';
        arrow.style.transform = 'rotate(180deg)';
        loadCrashLog();
    } else {
        content.style.display = 'none';
        arrow.style.transform = 'rotate(0deg)';
    }
}

// Load crash log from server
function loadCrashLog() {
    safeFetch('/crashlog_json', function(data) {
        const tbody = document.getElementById('crashTableBody');
        const badge = document.getElementById('crashBadge');
        const clearBtn = document.getElementById('crashClear');
        
        // Update badge
        const count = data.total || 0;
        badge.textContent = count;
        
        // Update badge color
        if (count === 0) {
            badge.className = 'badge';
            clearBtn.style.display = 'none';
        } else if (count <= 5) {
            badge.className = 'badge warning';
            clearBtn.style.display = 'inline-flex';
        } else {
            badge.className = 'badge danger';
            clearBtn.style.display = 'inline-flex';
        }
        
        // Update table
        if (data.entries && data.entries.length > 0) {
            tbody.innerHTML = '';
            data.entries.forEach(function(entry) {
                const heapClass = entry.heap < 15000 ? ' class="heap-critical"' : '';
                const typeClass = entry.reason === 'PANIC' ? 'crash-panic' : 'crash-wdt';
                
                const row = '<tr' + heapClass + '>' +
                    '<td>' + entry.num + '</td>' +
                    '<td class="' + typeClass + '">' + entry.reason + '</td>' +
                    '<td>' + formatUptime(entry.uptime) + '</td>' +
                    '<td>' + formatBytes(entry.heap) + '</td>' +
                    '<td>' + formatBytes(entry.min_heap) + '</td>' +
                    '</tr>';
                tbody.innerHTML += row;
            });
        } else {
            tbody.innerHTML = '<tr><td colspan="5" style="text-align: center;">No crashes recorded</td></tr>';
        }
    });
}

// Clear crash history
function clearCrashHistory() {
    if (confirm('Clear all crash history?\nThis action cannot be undone.')) {
        safeFetch('/clear_crashlog', function() {
            // Update UI
            document.getElementById('crashBadge').textContent = '0';
            document.getElementById('crashBadge').className = 'badge';
            document.getElementById('crashClear').style.display = 'none';
            
            // Reload table if open
            if (document.getElementById('crashContent').style.display !== 'none') {
                loadCrashLog();
            }
        });
    }
}

// Update crash count on page load
function updateCrashCount() {
    safeFetch('/crashlog_json', function(data) {
        const count = data.total || 0;
        const badge = document.getElementById('crashBadge');
        const clearBtn = document.getElementById('crashClear');
        
        badge.textContent = count;
        if (count === 0) {
            badge.className = 'badge';
            clearBtn.style.display = 'none';
        } else if (count <= 5) {
            badge.className = 'badge warning';
            clearBtn.style.display = 'inline-flex';
        } else {
            badge.className = 'badge danger';
            clearBtn.style.display = 'inline-flex';
        }
    });
}

// Format uptime from seconds
function formatUptime(seconds) {
    if (!seconds || seconds === 0) return '0s';
    if (seconds < 60) return seconds + 's';
    if (seconds < 3600) return Math.floor(seconds/60) + 'm';
    return Math.floor(seconds/3600) + 'h ' + Math.floor((seconds%3600)/60) + 'm';
}

// Format bytes to KB
function formatBytes(bytes) {
    if (!bytes || bytes === 0) return '0';
    return Math.floor(bytes/1024) + 'KB';
}