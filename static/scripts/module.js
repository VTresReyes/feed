const API_URL = '/modules'; // relative to the Flask server
const CAMERA_API_URL = '/cameras'; // API endpoint for cameras
let previousModules = [];
let isEditing = false; // Flag to prevent auto-refresh during editing

async function addModule() {
    const module_id = document.getElementById('module_id').value;
    const cam_id = document.getElementById('cam_id').value;
    const status = document.getElementById('status').value;
    const weight = document.getElementById('weight').value || '0';

    if (!module_id) {
        alert('Module ID is required!');
        return;
    }

    // If cam_id is provided, add it to the camera table first
    if (cam_id) {
        try {
            const cameraRes = await fetch(CAMERA_API_URL, {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({ 
                    cam_id: cam_id, 
                    status: 'active' // Default status for new cameras
                })
            });

            const cameraResult = await cameraRes.json();
            
            // Continue even if camera already exists (common scenario)
            if (!cameraResult.success && !cameraResult.message?.includes('already exists')) {
                console.warn('Warning adding camera:', cameraResult.message);
            }
        } catch (error) {
            console.error('Error adding camera:', error);
            // Continue with module addition even if camera addition fails
        }
    }

    // Add the module
    const res = await fetch(API_URL, {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({ module_id, cam_id, status, weight })
    });

    const result = await res.json();
    alert(result.success ? 'Module added!' : 'Error adding module');
    
    // Clear form
    document.getElementById('moduleForm').reset();
    // Reset weight to 0 after form reset
    document.getElementById('weight').value = '0';
    loadModules();
}

function editModule(module_id, cam_id, status, weight) {
    isEditing = true;
    const row = event.target.closest('tr');
    
    row.innerHTML = `
        <td>${module_id}</td>
        <td><input type="text" id="edit_cam_id" value="${cam_id || ''}" /></td>
        <td>
            <select id="edit_status">
                <option value="active" ${status === 'active' ? 'selected' : ''}>Active</option>
                <option value="inactive" ${status === 'inactive' ? 'selected' : ''}>Inactive</option>
            </select>
        </td>
        <td><input type="number" id="edit_weight" value="${weight || '0'}" step="0.01" /></td>
        <td>
            <button onclick="saveModule('${module_id}', '${cam_id || ''}')" class="btn-save">Save</button>
            <button onclick="cancelEdit()" class="btn-cancel">Cancel</button>
        </td>
    `;
}

async function saveModule(module_id, old_cam_id) {
    const cam_id = document.getElementById('edit_cam_id').value;
    const status = document.getElementById('edit_status').value;
    const weight = document.getElementById('edit_weight').value || '0';

    // If cam_id changed and new cam_id is provided, add it to camera table
    if (cam_id && cam_id !== old_cam_id) {
        try {
            const cameraRes = await fetch(CAMERA_API_URL, {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({ 
                    cam_id: cam_id, 
                    status: 'active'
                })
            });

            const cameraResult = await cameraRes.json();
            
            if (!cameraResult.success && !cameraResult.message?.includes('already exists')) {
                console.warn('Warning adding camera:', cameraResult.message);
            }
        } catch (error) {
            console.error('Error adding camera:', error);
        }
    }

    try {
        const res = await fetch(`${API_URL}/${module_id}`, {
            method: 'PUT',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ cam_id, status, weight})
        });

        const result = await res.json();
        alert(result.success ? 'Module updated!' : 'Error updating module');
        isEditing = false;
        loadModules();
    } catch (error) {
        console.error('Error updating module:', error);
        alert('Error updating module');
        isEditing = false;
    }
}

function cancelEdit() {
    isEditing = false;
    loadModules();
}

async function deleteModule(module_id) {
    if (!confirm(`Are you sure you want to delete module ${module_id}?`)) {
        return;
    }

    try {
        const res = await fetch(`${API_URL}/${module_id}`, {
            method: 'DELETE'
        });

        const result = await res.json();
        alert(result.success ? 'Module deleted!' : 'Error deleting module');
        loadModules();
    } catch (error) {
        console.error('Error deleting module:', error);
        alert('Error deleting module');
    }
}

async function loadModules() {
    if (isEditing) return; // Don't reload while editing
    
    try {
        const res = await fetch(API_URL);
        const data = await res.json();
        
        const tbody = document.getElementById('moduleTableBody');
        tbody.innerHTML = '';
        
        if (data.length === 0) {
            tbody.innerHTML = '<tr><td colspan="5">No modules found</td></tr>';
            return;
        }
        
        data.forEach(module => {
            const row = document.createElement('tr');
            row.innerHTML = `
                <td>${module.module_id}</td>
                <td>${module.cam_id || 'N/A'}</td>
                <td>${module.status || 'N/A'}</td>
                <td>${module.weight || '0'}</td>
                <td>
                    <button onclick="editModule('${module.module_id}', '${module.cam_id || ''}', '${module.status || ''}', '${module.weight || '0'}')" class="btn-edit">Edit</button>
                    <button onclick="deleteModule('${module.module_id}')" class="btn-delete">Delete</button>
                </td>
            `;
            tbody.appendChild(row);
        });

        // Check if weight has changed
        checkWeightChanges(data);
        previousModules = data;
    } catch (error) {
        console.error('Error loading modules:', error);
        document.getElementById('moduleTableBody').innerHTML = 
            '<tr><td colspan="5">Error loading modules</td></tr>';
    }
}

function checkWeightChanges(currentModules) {
    currentModules.forEach(currentModule => {
        const previousModule = previousModules.find(m => m.module_id === currentModule.module_id);
        if (previousModule && previousModule.weight !== currentModule.weight) {
            console.log(`Weight changed for ${currentModule.module_id}: ${previousModule.weight} -> ${currentModule.weight}`);
        }
    });
}

function manualRefresh() {
    const button = document.getElementById('toggleRefresh');
    button.classList.add('refresh-active');
    loadModules();
    
    // Remove the active class after animation
    setTimeout(() => {
        button.classList.remove('refresh-active');
    }, 300);
}

// Load modules on page load
loadModules();