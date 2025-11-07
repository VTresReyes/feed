// Get the API URL dynamically from the current page location
const API_URL = `${window.location.protocol}//${window.location.host}`;

function loadHistory() {
    fetch(`${API_URL}/history`)
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            const tbody = document.getElementById('historyTableBody');
            tbody.innerHTML = '';
            
            if (data.length === 0) {
                tbody.innerHTML = '<tr><td colspan="8">No history records found</td></tr>';
                return;
            }
            
            data.forEach(record => {
                const row = document.createElement('tr');
                row.innerHTML = `
                    <td>${record.history_id}</td>
                    <td>${formatDateTime(record.created_at)}</td>
                    <td>${record.schedule_id || 'N/A'}</td>
                    <td>${record.module_id || 'N/A'}</td>
                    <td>${record.feed_time || 'N/A'}</td>
                    <td>${record.amount ? record.amount + 'g' : 'N/A'}</td>
                    <td><span class="status-badge status-${record.status}">${record.status || 'N/A'}</span></td>
                    <td>
                        <button class="btn-delete" onclick="deleteHistory(${record.history_id})">Delete</button>
                    </td>
                `;
                tbody.appendChild(row);
            });
        })
        .catch(error => {
            console.error('Error loading history:', error);
            document.getElementById('historyTableBody').innerHTML = 
                `<tr><td colspan="8">Error loading history: ${error.message}</td></tr>`;
        });
}

function formatDateTime(dateTimeString) {
    if (!dateTimeString) return 'N/A';
    try {
        const date = new Date(dateTimeString);
        return date.toLocaleString('en-US', {
            year: 'numeric',
            month: 'short',
            day: 'numeric',
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit'
        });
    } catch (e) {
        return dateTimeString;
    }
}

function deleteHistory(historyId) {
    if (!confirm('Are you sure you want to delete this history record?')) {
        return;
    }
    
    fetch(`${API_URL}/history/${historyId}`, {
        method: 'DELETE'
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        if (data.success) {
            showNotification('History record deleted successfully', 'success');
            loadHistory();
        }
    })
    .catch(error => {
        console.error('Error deleting history:', error);
        showNotification('Error deleting history record: ' + error.message, 'error');
    });
}

function showNotification(message, type = 'info') {
    const notification = document.createElement('div');
    const bgColor = type === 'success' ? '#4CAF50' : type === 'error' ? '#f44336' : '#2196F3';
    notification.style.cssText = `
        position: fixed;
        top: 20px;
        right: 20px;
        background: ${bgColor};
        color: white;
        padding: 15px 20px;
        border-radius: 5px;
        box-shadow: 0 2px 5px rgba(0,0,0,0.2);
        z-index: 1000;
        animation: slideIn 0.3s ease-out;
    `;
    notification.textContent = message;
    document.body.appendChild(notification);
    
    setTimeout(() => {
        notification.style.animation = 'slideOut 0.3s ease-out';
        setTimeout(() => notification.remove(), 300);
    }, 3000);
}

// Add CSS for status badges and animations
const style = document.createElement('style');
style.textContent = `
    .status-badge {
        padding: 4px 12px;
        border-radius: 12px;
        font-size: 0.85em;
        font-weight: 500;
    }
    .status-pending {
        background: #fff3cd;
        color: #856404;
    }
    .status-done {
        background: #d4edda;
        color: #155724;
    }
    .btn-delete {
        background: #f44336;
        color: white;
        border: none;
        padding: 6px 12px;
        border-radius: 4px;
        cursor: pointer;
        font-size: 0.9em;
    }
    .btn-delete:hover {
        background: #d32f2f;
    }
    @keyframes slideIn {
        from { transform: translateX(100%); opacity: 0; }
        to { transform: translateX(0); opacity: 1; }
    }
    @keyframes slideOut {
        from { transform: translateX(0); opacity: 1; }
        to { transform: translateX(100%); opacity: 0; }
    }
`;
document.head.appendChild(style);

// Load history on page load
loadHistory();

// Auto-refresh every 5 seconds
setInterval(loadHistory, 5000);