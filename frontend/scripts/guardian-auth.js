const API_BASE = "http://127.0.0.1:5001";

function toggleForm() {
    document.getElementById('loginForm').classList.toggle('active');
    document.getElementById('registerForm').classList.toggle('active');
}

function showAlert(elementId, message, type) {
    const alertEl = document.getElementById(elementId);
    alertEl.className = `alert alert-${type}`;
    alertEl.textContent = message;
    alertEl.style.display = 'block';
    
    if (type === 'success') {
        setTimeout(() => {
            alertEl.style.display = 'none';
        }, 3000);
    }
}

async function handleLogin() {
    const username = document.getElementById('loginUsername').value.trim();
    const password = document.getElementById('loginPassword').value;

    if (!username || !password) {
        showAlert('loginAlert', 'Please enter username and password', 'error');
        return;
    }

    document.getElementById('loginBtnText').style.display = 'none';
    document.getElementById('loginLoading').classList.add('active');

    try {
        console.log('🔍 Attempting guardian login to:', `${API_BASE}/guardian-login`);
        console.log('🔍 Sending data:', { username, password });
        
        const response = await fetch(`${API_BASE}/guardian-login`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ username, password })
        });

        console.log('🔍 Guardian login response status:', response.status);
        console.log('🔍 Guardian login response ok:', response.ok);

        const data = await response.json();
        console.log('🔍 Guardian login response data:', data);

        if (response.ok && data.status === 'success') {
            showAlert('loginAlert', 'Login successful! Redirecting...', 'success');
            
            localStorage.setItem('guardian_username', data.username);
            localStorage.setItem('guardian_name', data.name);
            localStorage.setItem('guardian_phone', data.phone);
            localStorage.setItem('guardian_email', data.email);
            
            setTimeout(() => {
                window.location.href = 'guardian-dashboard.html';
            }, 1500);
        } else {
            showAlert('loginAlert', data.message || 'Login failed', 'error');
        }
    } catch (error) {
        console.error('Login error:', error);
        showAlert('loginAlert', 'Error connecting to server', 'error');
    } finally {
        document.getElementById('loginBtnText').style.display = 'inline';
        document.getElementById('loginLoading').classList.remove('active');
    }
}

async function handleRegister() {
    const name = document.getElementById('regName').value.trim();
    const username = document.getElementById('regUsername').value.trim();
    const password = document.getElementById('regPassword').value;
    const phone = document.getElementById('regPhone').value.trim();
    const email = document.getElementById('regEmail').value.trim();

    if (!name || !username || !password || !phone || !email) {
        showAlert('registerAlert', 'Please fill all fields', 'error');
        return;
    }

    if (password.length < 6) {
        showAlert('registerAlert', 'Password must be at least 6 characters', 'error');
        return;
    }

    document.getElementById('registerBtnText').style.display = 'none';
    document.getElementById('registerLoading').classList.add('active');

    try {
        console.log('🔍 Attempting guardian registration to:', `${API_BASE}/guardian-register`);
        console.log('🔍 Sending data:', {
            name,
            username,
            password,
            phone,
            email
        });
        
        const response = await fetch(`${API_BASE}/guardian-register`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                name,
                username,
                password,
                phone,
                email
            })
        });

        console.log('🔍 Guardian register response status:', response.status);
        console.log('🔍 Guardian register response ok:', response.ok);

        const data = await response.json();
        console.log('🔍 Guardian register response data:', data);

        if (response.ok && data.status === 'success') {
            showAlert('registerAlert', 'Registration successful! Switching to login...', 'success');
            
            document.getElementById('regName').value = '';
            document.getElementById('regUsername').value = '';
            document.getElementById('regPassword').value = '';
            document.getElementById('regPhone').value = '';
            document.getElementById('regEmail').value = '';
            
            setTimeout(() => {
                toggleForm();
                document.getElementById('loginUsername').value = username;
                document.getElementById('loginPassword').focus();
            }, 1500);
        } else {
            showAlert('registerAlert', data.message || 'Registration failed', 'error');
        }
    } catch (error) {
        console.error('Registration error:', error);
        showAlert('registerAlert', 'Error connecting to server', 'error');
    } finally {
        document.getElementById('registerBtnText').style.display = 'inline';
        document.getElementById('registerLoading').classList.remove('active');
    }
}

document.addEventListener('DOMContentLoaded', () => {
    document.getElementById('loginPassword').addEventListener('keypress', (e) => {
        if (e.key === 'Enter') handleLogin();
    });

    document.getElementById('regEmail').addEventListener('keypress', (e) => {
        if (e.key === 'Enter') handleRegister();
    });
});
