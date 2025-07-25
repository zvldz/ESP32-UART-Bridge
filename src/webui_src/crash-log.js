// ESP32 UART Bridge - Crash Log Module

const CrashLog = {
    elements: null,
    
    init() {
        this.elements = this.cacheElements();
        this.updateCount();
    },
    
    cacheElements() {
        return {
            content: document.getElementById('crashContent'),
            arrow: document.getElementById('crashArrow'),
            badge: document.getElementById('crashBadge'),
            clearBtn: document.getElementById('crashClear'),
            tbody: document.getElementById('crashTableBody')
        };
    },
    
    toggle() {
        if (!this.elements.content || !this.elements.arrow) return;
        
        const isOpening = this.elements.content.style.display === 'none';
        
        if (isOpening) {
            this.elements.content.style.display = 'block';
            this.elements.arrow.style.transform = 'rotate(180deg)';
            this.load();
        } else {
            this.elements.content.style.display = 'none';
            this.elements.arrow.style.transform = 'rotate(0deg)';
        }
    },
    
    load() {
        Utils.safeFetch('/crashlog_json', (data) => {
            this.updateBadge(data.total || 0);
            this.updateTable(data.entries || []);
        });
    },
    
    updateCount() {
        Utils.safeFetch('/crashlog_json', (data) => {
            this.updateBadge(data.total || 0);
        }, (error) => {
            // Silent fail - crash log is not critical
            console.log('Could not load crash count');
        });
    },
    
    updateBadge(count) {
        if (!this.elements.badge || !this.elements.clearBtn) return;
        
        this.elements.badge.textContent = count;
        
        // Update badge appearance based on count
        if (count === 0) {
            this.elements.badge.style.background = '#4CAF50';
            this.elements.clearBtn.style.display = 'none';
        } else if (count <= 5) {
            this.elements.badge.style.background = '#ff9800';
            this.elements.clearBtn.style.display = 'inline-flex';
        } else {
            this.elements.badge.style.background = '#f44336';
            this.elements.clearBtn.style.display = 'inline-flex';
        }
    },
    
    updateTable(entries) {
        if (!this.elements.tbody) return;
        
        if (entries.length > 0) {
            let html = '';
            
            entries.forEach(entry => {
                const heapClass = entry.heap < 15000 ? ' class="heap-critical"' : '';
                const typeClass = entry.reason === 'PANIC' ? 'crash-panic' : 
                                 entry.reason.includes('WDT') ? 'crash-wdt' : 'crash-other';
                
                html += `
                    <tr${heapClass}>
                        <td>${entry.num}</td>
                        <td class="${typeClass}">${this.formatReason(entry.reason)}</td>
                        <td>${Utils.formatUptime(entry.uptime)}</td>
                        <td>${Utils.formatBytes(entry.heap)}</td>
                        <td>${Utils.formatBytes(entry.min_heap)}</td>
                    </tr>
                `;
            });
            
            this.elements.tbody.innerHTML = html;
        } else {
            this.elements.tbody.innerHTML = '<tr><td colspan="5" style="text-align: center;">No crashes recorded</td></tr>';
        }
    },
    
    formatReason(reason) {
        // Make crash reasons more readable
        const reasonMap = {
            'PANIC': 'Panic',
            'TASK_WDT': 'Task WDT',
            'INT_WDT': 'Int WDT',
            'SW_RESET': 'SW Reset',
            'POWERON': 'Power On',
            'BROWNOUT': 'Brownout',
            'DEEPSLEEP': 'Deep Sleep'
        };
        
        return reasonMap[reason] || reason;
    },
    
    clear() {
        if (!confirm('Clear all crash history?\nThis action cannot be undone.')) {
            return;
        }
        
        fetch('/clear_crashlog')
            .then(response => {
                if (!response.ok) {
                    throw new Error('Clear failed');
                }
                return response.json(); // Parse JSON response
            })
            .then(data => {
                // Check if response indicates success
                if (data.status !== 'ok') {
                    throw new Error('Clear operation failed');
                }
                
                // Update UI
                this.updateBadge(0);
                
                // Update table if visible
                if (this.elements.content && this.elements.content.style.display !== 'none') {
                    this.updateTable([]);
                }
                
                // Show feedback
                const badge = this.elements.badge;
                if (badge) {
                    const originalText = badge.textContent;
                    badge.textContent = '✓';
                    setTimeout(() => {
                        badge.textContent = originalText;
                    }, 1500);
                }
            })
            .catch(error => {
                console.error('Clear crash log error:', error);
                alert('Failed to clear crash history: ' + error.message);
            });
    },
    
    // Auto-refresh crash count if section is open
    startAutoRefresh() {
        // This can be called from main.js if needed
        if (this.elements.content && this.elements.content.style.display !== 'none') {
            this.load();
        } else {
            this.updateCount();
        }
    }
};

// Global functions for onclick handlers
function toggleCrashLog() {
    CrashLog.toggle();
}

function clearCrashHistory() {
    CrashLog.clear();
}