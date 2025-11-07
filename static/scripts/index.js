// Get the API URL dynamically from the current page location
const API_URL = `${window.location.protocol}//${window.location.host}`;
let previousModules = [];

function loadModules() {
    fetch(`${API_URL}/modules`)
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            const tbody = document.getElementById('moduleTableBody');
            tbody.innerHTML = '';
            
            if (data.length === 0) {
                tbody.innerHTML = '<tr><td colspan="4">No modules found</td></tr>';
                return;
            }
            
            data.forEach(module => {
                const row = document.createElement('tr');
                row.innerHTML = `
                    <td>${module.module_id}</td>
                    <td>${module.cam_id}</td>
                    <td>${module.status}</td>
                    <td>${module.weight ? module.weight + 'g' : 'N/A'}</td>
                `;
                tbody.appendChild(row);
            });

            // Check if weight has changed
            checkWeightChanges(data);
            previousModules = data;
        })
        .catch(error => {
            console.error('Error loading modules:', error);
            document.getElementById('moduleTableBody').innerHTML = 
                `<tr><td colspan="4">Error loading modules: ${error.message}</td></tr>`;
        });
}

function checkWeightChanges(currentModules) {
    currentModules.forEach(currentModule => {
        const previousModule = previousModules.find(m => m.module_id === currentModule.module_id);
        if (previousModule && previousModule.weight !== currentModule.weight) {
            console.log(`Weight changed for ${currentModule.module_id}: ${previousModule.weight}g -> ${currentModule.weight}g`);
            // Optional: Show visual notification
            showWeightChangeNotification(currentModule.module_id, previousModule.weight, currentModule.weight);
        }
    });
}

function showWeightChangeNotification(moduleId, oldWeight, newWeight) {
    // You can add a toast notification here
    const notification = document.createElement('div');
    notification.style.cssText = `
        position: fixed;
        top: 20px;
        right: 20px;
        background: #4CAF50;
        color: white;
        padding: 15px 20px;
        border-radius: 5px;
        box-shadow: 0 2px 5px rgba(0,0,0,0.2);
        z-index: 1000;
        animation: slideIn 0.3s ease-out;
    `;
    notification.textContent = `${moduleId}: Weight updated ${oldWeight}g → ${newWeight}g`;
    document.body.appendChild(notification);
    
    setTimeout(() => {
        notification.style.animation = 'slideOut 0.3s ease-out';
        setTimeout(() => notification.remove(), 300);
    }, 3000);
}

// Add CSS animation for notifications
const style = document.createElement('style');
style.textContent = `
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

// Load modules on page load
loadModules();

// Auto-refresh every 2 seconds to detect weight changes
setInterval(loadModules, 2000);