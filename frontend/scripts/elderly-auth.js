const API_BASE = "http://127.0.0.1:5001";

// Test function to check API connection
async function testAPIConnection() {
    try {
        console.log('🔍 Testing API connection...');
        const response = await fetch(`${API_BASE}/test-api`);
        const data = await response.json();
        console.log('✅ API Test Result:', data);
        return true;
    } catch (error) {
        console.error('❌ API Test Failed:', error);
        return false;
    }
}

// Auto-test on page load
document.addEventListener('DOMContentLoaded', function() {
    testAPIConnection();
});

function showAlert(message, type) {
    const alertEl = document.getElementById('alertBox');
    alertEl.className = `alert alert-${type}`;
    alertEl.textContent = message;
    alertEl.style.display = 'block';
    
    if (type === 'success') {
        setTimeout(() => {
            alertEl.style.display = 'none';
        }, 4000);
    }
}

document.addEventListener('DOMContentLoaded', () => {
    const elderlyId = localStorage.getItem('elderly_id');
    const elderlyName = localStorage.getItem('elderly_name');
    const rememberMe = localStorage.getItem('elderly_remember_me');
    
    if (elderlyId && elderlyName && rememberMe === 'true') {
        // Set elderly login status to true for remembered users
        localStorage.setItem('elderlyLoggedIn', 'true');
        console.log('✅ Elderly auto-logged in (remember me) - showing portal button');
        window.location.href = 'index.html';
        return;
    }
    
    document.getElementById('guardianPassword').addEventListener('keypress', (e) => {
        if (e.key === 'Enter') handleElderlyRegister();
    });
    
    const savedElderlyName = localStorage.getItem('elderly_name');
    const savedElderlyPhone = localStorage.getItem('elderly_phone');
    if (savedElderlyName && savedElderlyPhone) {
        document.getElementById('elderlyName').value = savedElderlyName;
        document.getElementById('elderlyPhone').value = savedElderlyPhone;
        document.getElementById('rememberMe').checked = true;
    }
});

async function handleElderlyRegister() {
    const name = document.getElementById('elderlyName').value.trim();
    const age = document.getElementById('elderlyAge').value;
    const phone = document.getElementById('elderlyPhone').value.trim();
    const location = document.getElementById('elderlyLocation').value.trim() || "Home";
    const medicalHistory = document.getElementById('elderlyMedicalHistory').value.trim();
    const guardianUsername = document.getElementById('guardianUsername').value.trim();
    const guardianPassword = document.getElementById('guardianPassword').value;
    const rememberMe = document.getElementById('rememberMe').checked;

    if (!name || !age || !guardianUsername || !guardianPassword) {
        showAlert('Please fill all required fields (*)', 'error');
        return;
    }

    if (age < 1 || age > 150) {
        showAlert('Please enter a valid age', 'error');
        return;
    }

    document.getElementById('registerBtnText').style.display = 'none';
    document.getElementById('registerLoading').classList.add('active');

    try {
        console.log('🔍 Attempting registration to:', `${API_BASE}/elderly-register`);
        console.log('🔍 Sending data:', {
            name,
            age: parseInt(age),
            phone,
            location,
            medical_history: medicalHistory,
            guardian_username: guardianUsername,
            guardian_password: guardianPassword
        });
        
        const response = await fetch(`${API_BASE}/elderly-register`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                name,
                age: parseInt(age),
                phone,
                location,
                medical_history: medicalHistory,
                guardian_username: guardianUsername,
                guardian_password: guardianPassword
            })
        });

        console.log('🔍 Response status:', response.status);
        console.log('🔍 Response ok:', response.ok);

        const data = await response.json();
        console.log('🔍 Response data:', data);
        console.log('🔍 Response status:', response.status);
        console.log('🔍 Response ok:', response.ok);
        console.log('🔍 Data status:', data.status);

        if (response.ok && data.status === 'success') {
            showAlert('Registration successful! You are now linked to your guardian.', 'success');
            
            localStorage.setItem('elderly_id', data.elderly_id);
            localStorage.setItem('elderly_name', name);
            localStorage.setItem('guardian_username', guardianUsername);
            localStorage.setItem('elderly_remember_me', rememberMe.toString());
            
            // Set elderly login status to true
            localStorage.setItem('elderlyLoggedIn', 'true');
            console.log('✅ Elderly logged in - showing portal button');
            
            if (rememberMe) {
                localStorage.setItem('elderly_name', name);
                localStorage.setItem('elderly_phone', phone);
                localStorage.setItem('elderly_age', age);
            } else {
                localStorage.removeItem('elderly_name');
                localStorage.removeItem('elderly_phone');
                localStorage.removeItem('elderly_age');
            }
            
            setTimeout(() => {
                window.location.href = 'index.html';
            }, 2000);
        } else {
            showAlert(data.message || 'Registration failed', 'error');
        }
    } catch (error) {
        console.error('Registration error:', error);
        showAlert('Error connecting to server. Please try again.', 'error');
    } finally {
        document.getElementById('registerBtnText').style.display = 'inline';
        document.getElementById('registerLoading').classList.remove('active');
    }
}
