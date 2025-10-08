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
            this.elements.arrow.textContent = '▼';
            this.load();
        } else {
            this.elements.content.style.display = 'none';
            this.elements.arrow.textContent = '▶';
        }

        // Save state to localStorage
        try {
            localStorage.setItem('collapse:crash', isOpening ? 'shown' : 'hidden');
        } catch(e) {
            console.warn('localStorage not available:', e);
        }
    },

    // Restore collapsed state from localStorage
    restoreState() {
        if (!this.elements.content || !this.elements.arrow) return;

        let v = null;
        try {
            v = localStorage.getItem('collapse:crash');
        } catch(e) {
            console.warn('localStorage not available:', e);
        }

        if (v === 'shown') {
            this.elements.content.style.display = 'block';
            this.elements.arrow.textContent = '▼';
            // Load data if block is open on page load
            this.load();
        } else {
            // Default: collapsed (including when v === null - no saved state)
            this.elements.content.style.display = 'none';
            this.elements.arrow.textContent = '▶';
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

            entries.forEach((entry, index) => {
                const heapClass = entry.heap < 15000 ? ' class="heap-critical"' : '';
                const typeClass = entry.reason === 'PANIC' ? 'crash-panic' :
                                 entry.reason.includes('WDT') ? 'crash-wdt' : 'crash-other';

                const hasPanic = entry.panic && entry.panic.pc;
                const detailsText = hasPanic
                    ? `🔍 PC:${entry.panic.pc} ${entry.panic.task || ''}`
                    : '—';

                const clickHandler = hasPanic ? `onclick="CrashLog.toggleDetails(${index})"` : '';
                const cursorStyle = hasPanic ? 'cursor: pointer;' : '';

                html += `
                    <tr${heapClass} ${clickHandler} style="${cursorStyle}" data-entry-index="${index}">
                        <td>${entry.num}</td>
                        <td class="${typeClass}">${this.formatReason(entry.reason)}</td>
                        <td>${Utils.formatUptime(entry.uptime)}</td>
                        <td>${Utils.formatBytes(entry.heap)}</td>
                        <td class="crash-min-heap">${Utils.formatBytes(entry.min_heap)}</td>
                        <td class="crash-details">${detailsText}</td>
                    </tr>
                `;

                // Add expandable details row if panic data exists
                if (hasPanic) {
                    html += `
                        <tr id="crash-details-${index}" class="crash-details-row" style="display: none;">
                            <td colspan="6" style="padding: 10px; background: #f5f5f5; border-left: 3px solid #f44336;">
                                ${this.renderPanicDetails(entry.panic, entry.version)}
                            </td>
                        </tr>
                    `;
                }
            });

            this.elements.tbody.innerHTML = html;

            // Attach event listeners to copy buttons
            document.querySelectorAll('.copy-exception-btn').forEach(btn => {
                btn.addEventListener('click', (e) => {
                    e.stopPropagation();
                    // Unescape the text
                    const text = btn.dataset.exception.replace(/\\n/g, '\n').replace(/\\'/g, "'");
                    this.copyException(text);
                });
            });

            document.querySelectorAll('.copy-command-btn').forEach(btn => {
                btn.addEventListener('click', (e) => {
                    e.stopPropagation();
                    this.copyCommand(btn.dataset.addresses);
                });
            });
        } else {
            this.elements.tbody.innerHTML = '<tr><td colspan="6" style="text-align: center;">No crashes recorded</td></tr>';
        }
    },

    renderPanicDetails(panic, version) {
        let html = '<div class="panic-details">';
        html += '<div style="font-weight: bold; margin-bottom: 8px;">⚠️ Exception Details:</div>';

        // Build exception text for copying
        let exceptionText = '⚠️ Exception Details:\n\n';

        // Show firmware version if available
        if (version) {
            html += '<div style="margin-bottom: 8px; color: #666; font-size: 11px;">';
            html += `Firmware Version: <strong>${version}</strong>`;
            html += '</div>';
            exceptionText += `Firmware Version: ${version}\n`;
        }

        // Main exception info
        html += '<div style="margin-bottom: 8px; font-family: monospace; font-size: 11px;">';
        html += `<div>PC (Program Counter): <strong>${panic.pc}</strong></div>`;
        exceptionText += `PC (Program Counter): ${panic.pc}\n`;

        if (panic.task) {
            html += `<div>Task: <strong>${panic.task}</strong></div>`;
            exceptionText += `Task: ${panic.task}\n`;
        }
        if (panic.excause) {
            const causeDesc = this.getExcauseDescription(panic.excause);
            html += `<div>Exception Cause: <strong>${panic.excause}</strong> ${causeDesc}</div>`;
            exceptionText += `Exception Cause: ${panic.excause} ${causeDesc}\n`;
        }
        if (panic.excvaddr) {
            html += `<div>Exception Address: <strong>${panic.excvaddr}</strong></div>`;
            exceptionText += `Exception Address: ${panic.excvaddr}\n`;
        }
        html += '</div>';

        // Backtrace
        if (panic.backtrace && panic.backtrace.length > 0) {
            html += '<div style="margin-top: 10px;">';
            html += `<div style="font-weight: bold; margin-bottom: 5px;">Backtrace (${panic.backtrace.length} addresses):</div>`;
            html += '<div style="font-family: monospace; font-size: 11px; background: white; padding: 8px; border-radius: 4px; word-break: break-all; line-height: 1.6;">';

            exceptionText += `\nBacktrace (${panic.backtrace.length} addresses):\n`;

            // Group addresses by 4 for better readability on mobile
            for (let i = 0; i < panic.backtrace.length; i += 4) {
                const group = panic.backtrace.slice(i, i + 4);
                html += group.join(' ') + '<br>';
                exceptionText += group.join(' ') + '\n';
            }

            html += '</div>';
            html += '</div>';

            // Show warning if backtrace was truncated or corrupted
            if (panic.truncated) {
                html += '<div style="margin-top: 10px; padding: 8px; background: #fff3cd; border-left: 3px solid #ff9800; font-size: 11px; color: #856404;">';
                html += '⚠️ <strong>Warning:</strong> Backtrace was truncated at 16 addresses (ESP-IDF limit). Actual call stack may be deeper.';
                html += '</div>';
            }
            if (panic.corrupted) {
                html += '<div style="margin-top: 10px; padding: 8px; background: #f8d7da; border-left: 3px solid #f44336; font-size: 11px; color: #721c24;">';
                html += '⚠️ <strong>Warning:</strong> Backtrace data is corrupted. Results may be unreliable.';
                html += '</div>';
            }

            // Copy buttons
            html += '<div style="margin-top: 10px; display: flex; gap: 8px; flex-wrap: wrap;">';

            const addresses = panic.backtrace.join(' ');
            const exceptionTextEsc = exceptionText.replace(/'/g, "\\'").replace(/\n/g, '\\n');
            html += `<button data-exception="${exceptionTextEsc}" class="copy-exception-btn" style="padding: 6px 12px; font-size: 12px; cursor: pointer; border: 1px solid #2196F3; background: white; color: #1976D2; border-radius: 4px;">&#128203; Copy Exception</button>`;
            html += `<button data-addresses="${addresses}" class="copy-command-btn" style="padding: 6px 12px; font-size: 12px; cursor: pointer; border: 1px solid #4CAF50; background: white; color: #2E7D32; border-radius: 4px;">&#128203; Copy addr2line</button>`;

            html += '</div>';
        }

        html += '</div>';
        return html;
    },

    getExcauseDescription(excause) {
        // Common ESP32 exception causes
        const causes = {
            '0x00000000': '(IllegalInstruction)',
            '0x00000001': '(Syscall)',
            '0x00000002': '(InstructionFetchError)',
            '0x00000003': '(LoadStoreError)',
            '0x00000004': '(Level1Interrupt)',
            '0x00000005': '(Alloca)',
            '0x00000006': '(IntegerDivideByZero)',
            '0x00000009': '(LoadStoreAlignmentCause)',
            '0x0000000c': '(InstrPIFDataError)',
            '0x0000000d': '(LoadStorePIFDataError)',
            '0x0000000e': '(InstrPIFAddrError)',
            '0x0000000f': '(LoadStorePIFAddrError)',
            '0x00000014': '(InstrTLBMiss)',
            '0x00000018': '(LoadStoreTLBMiss)',
            '0x0000001c': '(LoadProhibited)',
            '0x0000001d': '(StoreProhibited)'
        };
        return causes[excause] || '';
    },

    toggleDetails(index) {
        const detailsRow = document.getElementById(`crash-details-${index}`);
        if (detailsRow) {
            const isVisible = detailsRow.style.display !== 'none';
            detailsRow.style.display = isVisible ? 'none' : 'table-row';
        }
    },

    copyException(text) {
        this.copyToClipboard(text, 'Exception details copied to clipboard!');
    },

    copyCommand(addresses) {
        // Generate addr2line command
        // User will need to replace firmware.elf with their actual path
        const command = `xtensa-esp32s3-elf-addr2line -pfiaC -e firmware.elf ${addresses}`;
        this.copyToClipboard(command, 'addr2line command copied! Replace firmware.elf with your .elf file path');
    },

    copyToClipboard(text, message) {
        if (navigator.clipboard && navigator.clipboard.writeText) {
            navigator.clipboard.writeText(text).then(() => {
                this.showCopyFeedback(message);
            }).catch(err => {
                console.error('Copy failed:', err);
                this.fallbackCopy(text, message);
            });
        } else {
            this.fallbackCopy(text, message);
        }
    },

    fallbackCopy(text, message) {
        // Fallback for older browsers
        const textarea = document.createElement('textarea');
        textarea.value = text;
        textarea.style.position = 'fixed';
        textarea.style.opacity = '0';
        document.body.appendChild(textarea);
        textarea.select();
        try {
            document.execCommand('copy');
            this.showCopyFeedback(message);
        } catch (err) {
            console.error('Fallback copy failed:', err);
            alert('Copy failed. Please copy manually.');
        }
        document.body.removeChild(textarea);
    },

    showCopyFeedback(message) {
        // Show temporary feedback message
        const feedback = document.createElement('div');
        feedback.textContent = message;
        feedback.style.cssText = 'position: fixed; top: 20px; right: 20px; background: #4CAF50; color: white; padding: 12px 20px; border-radius: 4px; z-index: 10000; font-size: 14px; box-shadow: 0 2px 8px rgba(0,0,0,0.2);';
        document.body.appendChild(feedback);

        setTimeout(() => {
            feedback.style.opacity = '0';
            feedback.style.transition = 'opacity 0.3s';
            setTimeout(() => document.body.removeChild(feedback), 300);
        }, 2000);
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