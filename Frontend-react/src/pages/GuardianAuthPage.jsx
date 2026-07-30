import React, { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { guardianLogin, guardianRegister } from '../services/api';

export default function GuardianAuthPage() {
  const navigate = useNavigate();
  const [isLogin, setIsLogin] = useState(true);

  // Login form state
  const [loginUsername, setLoginUsername] = useState('');
  const [loginPassword, setLoginPassword] = useState('');
  const [loginLoading, setLoginLoading] = useState(false);
  const [loginAlert, setLoginAlert] = useState({ show: false, message: '', type: '' });

  // Register form state
  const [regName, setRegName] = useState('');
  const [regUsername, setRegUsername] = useState('');
  const [regPassword, setRegPassword] = useState('');
  const [regPhone, setRegPhone] = useState('');
  const [regEmail, setRegEmail] = useState('');
  const [regLoading, setRegLoading] = useState(false);
  const [regAlert, setRegAlert] = useState({ show: false, message: '', type: '' });

  const showAlert = (isLoginForm, message, type) => {
    const alertSetter = isLoginForm ? setLoginAlert : setRegAlert;
    alertSetter({ show: true, message, type });
    if (type === 'success') {
      setTimeout(() => {
        alertSetter({ show: false, message: '', type: '' });
      }, 3000);
    }
  };

  const handleLoginSubmit = async (e) => {
    if (e) e.preventDefault();
    const username = loginUsername.trim();
    const password = loginPassword;

    if (!username || !password) {
      showAlert(true, 'Please enter username and password', 'error');
      return;
    }

    setLoginLoading(true);

    try {
      const { ok, data } = await guardianLogin(username, password);

      if (ok && data.status === 'success') {
        showAlert(true, 'Login successful! Redirecting...', 'success');

        localStorage.setItem('guardian_username', data.username);
        localStorage.setItem('guardian_name', data.name);
        localStorage.setItem('guardian_phone', data.phone);
        localStorage.setItem('guardian_email', data.email);

        setTimeout(() => {
          navigate('/guardian-dashboard');
        }, 1500);
      } else {
        showAlert(true, (data && data.message) || 'Invalid username or password', 'error');
      }
    } catch (err) {
      console.error(err);
      showAlert(true, 'Server connection error', 'error');
    } finally {
      setLoginLoading(false);
    }
  };

  const handleRegisterSubmit = async (e) => {
    if (e) e.preventDefault();
    const name = regName.trim();
    const username = regUsername.trim();
    const password = regPassword;
    const phone = regPhone.trim();
    const email = regEmail.trim();

    if (!name || !username || !password || !phone || !email) {
      showAlert(false, 'Please fill in all fields', 'error');
      return;
    }

    setRegLoading(true);

    try {
      const { ok, data } = await guardianRegister({
        name,
        username,
        password,
        phone,
        email
      });

      if (ok && data.status === 'success') {
        showAlert(false, 'Registration successful! Please log in.', 'success');
        setTimeout(() => {
          setIsLogin(true);
          setLoginUsername(username);
          setLoginPassword('');
        }, 1500);
      } else {
        showAlert(false, (data && data.message) || 'Registration failed', 'error');
      }
    } catch (err) {
      console.error(err);
      showAlert(false, 'Server connection error', 'error');
    } finally {
      setRegLoading(false);
    }
  };

  const goBack = () => {
    navigate('/');
  };

  return (
    <div style={{
      background: 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)',
      minHeight: '100vh',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      fontFamily: "'Segoe UI', Tahoma, Geneva, Verdana, sans-serif",
      padding: '20px',
      boxSizing: 'border-box'
    }}>
      <div className="container" style={{
        background: 'white',
        borderRadius: '20px',
        boxShadow: '0 20px 60px rgba(0, 0, 0, 0.3)',
        padding: '40px',
        maxWidth: '450px',
        width: '100%',
        margin: '20px',
        position: 'relative',
        boxSizing: 'border-box'
      }}>
        <button 
          className="back-btn" 
          onClick={goBack} 
          title="Go Back"
          style={{
            position: 'absolute',
            top: '20px',
            left: '20px',
            background: 'rgba(102, 126, 234, 0.1)',
            border: '1px solid rgba(102, 126, 234, 0.2)',
            color: '#667eea',
            padding: '8px',
            borderRadius: '8px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center'
          }}
        >
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
            <path d="M19 12H5M12 19l-7-7 7-7" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
          </svg>
        </button>

        {isLogin ? (
          <div>
            <div className="auth-header" style={{ textAlign: 'center', marginBottom: '30px', marginTop: '10px' }}>
              <h1 style={{ color: '#667eea', marginBottom: '10px', fontSize: '28px', fontWeight: 700 }}>Guardian Login</h1>
              <p style={{ color: '#666', fontSize: '14px', margin: 0 }}>Access your elderly family members</p>
            </div>

            {loginAlert.show && (
              <div style={{
                padding: '12px 15px',
                borderRadius: '8px',
                marginBottom: '20px',
                fontSize: '14px',
                background: loginAlert.type === 'success' ? '#d4edda' : '#f8d7da',
                color: loginAlert.type === 'success' ? '#155724' : '#721c24',
                border: loginAlert.type === 'success' ? '1px solid #c3e6cb' : '1px solid #f5c6cb'
              }}>
                {loginAlert.message}
              </div>
            )}

            <form onSubmit={handleLoginSubmit}>
              <div className="form-group" style={{ marginBottom: '20px', textAlign: 'left' }}>
                <label htmlFor="loginUsername" style={{ display: 'block', marginBottom: '8px', color: '#333', fontWeight: 500, fontSize: '14px' }}>Username</label>
                <input
                  type="text"
                  id="loginUsername"
                  placeholder="Enter your username"
                  value={loginUsername}
                  onChange={(e) => setLoginUsername(e.target.value)}
                  required
                  style={{
                    width: '100%',
                    padding: '12px 15px',
                    border: '2px solid #e0e0e0',
                    borderRadius: '8px',
                    fontSize: '14px',
                    boxSizing: 'border-box'
                  }}
                />
              </div>

              <div className="form-group" style={{ marginBottom: '20px', textAlign: 'left' }}>
                <label htmlFor="loginPassword" style={{ display: 'block', marginBottom: '8px', color: '#333', fontWeight: 500, fontSize: '14px' }}>Password</label>
                <input
                  type="password"
                  id="loginPassword"
                  placeholder="Enter your password"
                  value={loginPassword}
                  onChange={(e) => setLoginPassword(e.target.value)}
                  required
                  style={{
                    width: '100%',
                    padding: '12px 15px',
                    border: '2px solid #e0e0e0',
                    borderRadius: '8px',
                    fontSize: '14px',
                    boxSizing: 'border-box'
                  }}
                />
              </div>

              <button 
                type="submit" 
                className="btn btn-primary" 
                disabled={loginLoading}
                style={{
                  width: '100%',
                  padding: '14px',
                  border: 'none',
                  borderRadius: '8px',
                  fontSize: '16px',
                  fontWeight: 600,
                  cursor: 'pointer',
                  marginBottom: '15px',
                  background: 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)',
                  color: 'white'
                }}
              >
                {loginLoading ? 'Logging in...' : 'Login'}
              </button>

              <div className="toggle-form" style={{ textAlign: 'center', color: '#666', fontSize: '14px' }}>
                Don't have account? <a style={{ color: '#667eea', textDecoration: 'none', fontWeight: 600, cursor: 'pointer' }} onClick={() => setIsLogin(false)}>Register here</a>
              </div>
            </form>
          </div>
        ) : (
          <div>
            <div className="auth-header" style={{ textAlign: 'center', marginBottom: '30px', marginTop: '10px' }}>
              <h1 style={{ color: '#667eea', marginBottom: '10px', fontSize: '28px', fontWeight: 700 }}>Guardian Registration</h1>
              <p style={{ color: '#666', fontSize: '14px', margin: 0 }}>Create your account</p>
            </div>

            {regAlert.show && (
              <div style={{
                padding: '12px 15px',
                borderRadius: '8px',
                marginBottom: '20px',
                fontSize: '14px',
                background: regAlert.type === 'success' ? '#d4edda' : '#f8d7da',
                color: regAlert.type === 'success' ? '#155724' : '#721c24',
                border: regAlert.type === 'success' ? '1px solid #c3e6cb' : '1px solid #f5c6cb'
              }}>
                {regAlert.message}
              </div>
            )}

            <form onSubmit={handleRegisterSubmit}>
              <div className="form-group" style={{ marginBottom: '20px', textAlign: 'left' }}>
                <label htmlFor="regName" style={{ display: 'block', marginBottom: '8px', color: '#333', fontWeight: 500, fontSize: '14px' }}>Full Name</label>
                <input
                  type="text"
                  id="regName"
                  placeholder="Enter your full name"
                  value={regName}
                  onChange={(e) => setRegName(e.target.value)}
                  required
                  style={{ width: '100%', padding: '12px 15px', border: '2px solid #e0e0e0', borderRadius: '8px', fontSize: '14px', boxSizing: 'border-box' }}
                />
              </div>

              <div className="form-group" style={{ marginBottom: '20px', textAlign: 'left' }}>
                <label htmlFor="regUsername" style={{ display: 'block', marginBottom: '8px', color: '#333', fontWeight: 500, fontSize: '14px' }}>Username</label>
                <input
                  type="text"
                  id="regUsername"
                  placeholder="Choose a username"
                  value={regUsername}
                  onChange={(e) => setRegUsername(e.target.value)}
                  required
                  style={{ width: '100%', padding: '12px 15px', border: '2px solid #e0e0e0', borderRadius: '8px', fontSize: '14px', boxSizing: 'border-box' }}
                />
              </div>

              <div className="form-group" style={{ marginBottom: '20px', textAlign: 'left' }}>
                <label htmlFor="regPassword" style={{ display: 'block', marginBottom: '8px', color: '#333', fontWeight: 500, fontSize: '14px' }}>Password</label>
                <input
                  type="password"
                  id="regPassword"
                  placeholder="Create a strong password"
                  value={regPassword}
                  onChange={(e) => setRegPassword(e.target.value)}
                  required
                  style={{ width: '100%', padding: '12px 15px', border: '2px solid #e0e0e0', borderRadius: '8px', fontSize: '14px', boxSizing: 'border-box' }}
                />
              </div>

              <div className="form-group" style={{ marginBottom: '20px', textAlign: 'left' }}>
                <label htmlFor="regPhone" style={{ display: 'block', marginBottom: '8px', color: '#333', fontWeight: 500, fontSize: '14px' }}>Phone Number</label>
                <input
                  type="tel"
                  id="regPhone"
                  placeholder="+91 98765 43210"
                  value={regPhone}
                  onChange={(e) => setRegPhone(e.target.value)}
                  required
                  style={{ width: '100%', padding: '12px 15px', border: '2px solid #e0e0e0', borderRadius: '8px', fontSize: '14px', boxSizing: 'border-box' }}
                />
              </div>

              <div className="form-group" style={{ marginBottom: '20px', textAlign: 'left' }}>
                <label htmlFor="regEmail" style={{ display: 'block', marginBottom: '8px', color: '#333', fontWeight: 500, fontSize: '14px' }}>Email Address</label>
                <input
                  type="email"
                  id="regEmail"
                  placeholder="your.email@example.com"
                  value={regEmail}
                  onChange={(e) => setRegEmail(e.target.value)}
                  required
                  style={{ width: '100%', padding: '12px 15px', border: '2px solid #e0e0e0', borderRadius: '8px', fontSize: '14px', boxSizing: 'border-box' }}
                />
              </div>

              <button 
                type="submit" 
                className="btn btn-primary" 
                disabled={regLoading}
                style={{
                  width: '100%',
                  padding: '14px',
                  border: 'none',
                  borderRadius: '8px',
                  fontSize: '16px',
                  fontWeight: 600,
                  cursor: 'pointer',
                  marginBottom: '15px',
                  background: 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)',
                  color: 'white'
                }}
              >
                {regLoading ? 'Creating account...' : 'Register'}
              </button>

              <div className="toggle-form" style={{ textAlign: 'center', color: '#666', fontSize: '14px' }}>
                Already have account? <a style={{ color: '#667eea', textDecoration: 'none', fontWeight: 600, cursor: 'pointer' }} onClick={() => setIsLogin(true)}>Login here</a>
              </div>
            </form>
          </div>
        )}

        <div style={{ textAlign: 'center', marginTop: '24px' }}>
          <a href="#" style={{ color: '#667eea', textDecoration: 'none', fontSize: '14px', fontWeight: '600' }} onClick={(e) => { e.preventDefault(); navigate('/'); }}>
            ← Back to Portal Selection
          </a>
        </div>
      </div>
    </div>
  );
}
