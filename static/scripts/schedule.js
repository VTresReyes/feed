// Get the API URL dynamically from the current page location
const API_URL = `${window.location.protocol}//${window.location.host}`;

let isEditing = false;

function loadSchedules() {
    if (isEditing) return;
    
    // Load both modules and schedules
    Promise.all([
        fetch(`${API_URL}/modules`).then(res => {
            if (!res.ok) throw new Error(`Modules API error: ${res.status}`);
            return res.json();
        }),
        fetch(`${API_URL}/schedules`).then(res => {
            if (!res.ok) throw new Error(`Schedules API error: ${res.status}`);
            return res.json();
        })
    ])
    .then(([modules, schedules]) => {
        const tbody = document.getElementById('schedulesTable');
        tbody.innerHTML = '';
        
        // Filter only active modules
        const activeModules = modules.filter(module => 
            module.status && module.status.toLowerCase() === 'active'
        );
        
        if (activeModules.length === 0) {
            tbody.innerHTML = '<tr><td colspan="5">No active modules found. Please add modules first.</td></tr>';
            return;
        }
        
        // Create a row for each active module
        activeModules.forEach(module => {
            // Find the schedule for this module
            const schedule = schedules.find(s => s.module_id === module.module_id);
            
            const row = tbody.insertRow();
            row.innerHTML = `
                <td><strong>${module.module_id}</strong></td>
                <td>${schedule ? formatTime(schedule.feed_time) : '<em>Not set</em>'}</td>
                <td>${schedule ? schedule.amount + 'g' : '<em>Not set</em>'}</td>
                <td><span class="status-badge status-${schedule ? schedule.status : 'pending'}">${schedule ? schedule.status : 'pending'}</span></td>
                <td>
                    <button onclick="editSchedule('${module.module_id}', '${schedule ? schedule.feed_time : ''}', ${schedule ? schedule.amount : 0}, '${schedule ? schedule.status : 'pending'}', ${schedule ? schedule.schedule_id : 'null'})" class="btn-edit">Edit</button>
                    ${schedule ? `<button onclick="deleteSchedule(${schedule.schedule_id})" class="btn-delete">Delete</button>` : ''}
                </td>
            `;
        });
    })
    .catch(error => {
        console.error('Error loading data:', error);
        document.getElementById('schedulesTable').innerHTML = 
            `<tr><td colspan="5">Error loading data: ${error.message}</td></tr>`;
    });
}

function formatTime(timeString) {
    if (!timeString) return 'Not set';
    try {
        // Convert 24h to 12h format
        const [hours, minutes] = timeString.split(':');
        const hour = parseInt(hours);
        const ampm = hour >= 12 ? 'PM' : 'AM';
        const displayHour = hour % 12 || 12;
        return `${displayHour}:${minutes} ${ampm}`;
    } catch (e) {
        return timeString;
    }
}

function editSchedule(moduleId, feedTime, amount, status, scheduleId) {
    isEditing = true;
    const row = event.target.closest('tr');
    
    row.innerHTML = `
        <td><strong>${moduleId}</strong></td>
        <td><input type="time" id="edit_feed_time" value="${feedTime}" class="input-time" required /></td>
        <td><input type="number" id="edit_amount" value="${amount}" step="0.01" min="0" class="input-amount" placeholder="grams" required /></td>
        <td><span class="status-badge status-${status}">${status}</span></td>
        <td>
            <button onclick="saveSchedule('${moduleId}', ${scheduleId})" class="btn-save">Save</button>
            <button onclick="cancelEdit()" class="btn-cancel">Cancel</button>
        </td>
    `;
    
    // Focus on first input
    document.getElementById('edit_feed_time').focus();
}

function saveSchedule(moduleId, scheduleId) {
    const feedTime = document.getElementById('edit_feed_time').value;
    const amount = parseFloat(document.getElementById('edit_amount').value);
    
    if (!feedTime) {
        showNotification('Feed time is required', 'error');
        return;
    }
    
    if (isNaN(amount) || amount <= 0) {
        showNotification('Please enter a valid amount greater than 0', 'error');
        return;
    }
    
    const data = {
        module_id: moduleId,
        feed_time: feedTime,
        amount: amount,
        status: 'pending'
    };
    
    // If schedule exists, update it; otherwise create new
    const method = scheduleId !== 'null' && scheduleId ? 'PUT' : 'POST';
    const url = scheduleId !== 'null' && scheduleId 
        ? `${API_URL}/schedules/${scheduleId}` 
        : `${API_URL}/schedules`;
    
    fetch(url, {
        method: method,
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(data)
    })
    .then(res => {
        if (!res.ok) throw new Error(`HTTP error! status: ${res.status}`);
        return res.json();
    })
    .then(() => {
        showNotification('Schedule saved successfully', 'success');
        isEditing = false;
        loadSchedules();
    })
    .catch(error => {
        console.error('Error saving schedule:', error);
        showNotification('Error saving schedule: ' + error.message, 'error');
        isEditing = false;
    });
}

function deleteSchedule(scheduleId) {
    if (!confirm('Are you sure you want to delete this schedule?')) {
        return;
    }
    
    fetch(`${API_URL}/schedules/${scheduleId}`, {
        method: 'DELETE'
    })
    .then(res => {
        if (!res.ok) throw new Error(`HTTP error! status: ${res.status}`);
        return res.json();
    })
    .then(() => {
        showNotification('Schedule deleted successfully', 'success');
        loadSchedules();
    })
    .catch(error => {
        console.error('Error deleting schedule:', error);
        showNotification('Error deleting schedule: ' + error.message, 'error');
    });
}

function cancelEdit() {
    isEditing = false;
    loadSchedules();
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

// Add CSS for styling
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
    .btn-edit {
        background: #2196F3;
        color: white;
        border: none;
        padding: 6px 12px;
        border-radius: 4px;
        cursor: pointer;
        font-size: 0.9em;
        margin-right: 5px;
    }
    .btn-edit:hover {
        background: #1976D2;
    }
    .btn-save {
        background: #4CAF50;
        color: white;
        border: none;
        padding: 6px 12px;
        border-radius: 4px;
        cursor: pointer;
        font-size: 0.9em;
        margin-right: 5px;
    }
    .btn-save:hover {
        background: #45a049;
    }
    .btn-cancel {
        background: #9E9E9E;
        color: white;
        border: none;
        padding: 6px 12px;
        border-radius: 4px;
        cursor: pointer;
        font-size: 0.9em;
        margin-right: 5px;
    }
    .btn-cancel:hover {
        background: #757575;
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
    .input-time, .input-amount {
        padding: 6px 10px;
        border: 1px solid #ddd;
        border-radius: 4px;
        font-size: 0.9em;
        width: 100%;
    }
    .input-time:focus, .input-amount:focus {
        outline: none;
        border-color: #2196F3;
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

// Load schedules on page load
loadSchedules();

// Auto-refresh every 10 seconds (when not editing)
setInterval(() => {
    if (!isEditing) {
        loadSchedules();
    }
}, 10000);