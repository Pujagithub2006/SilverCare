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
      console.log('🔍 Attempting guardian login to backend...');
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
        showAlert(true, data.message || 'Login failed', 'error');
      }
    } catch (error) {
      console.error('Login error:', error);
      showAlert(true, 'Error connecting to server', 'error');
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
      showAlert(false, 'Please fill all fields', 'error');
      return;
    }

    if (password.length < 6) {
      showAlert(false, 'Password must be at least 6 characters', 'error');
      return;
    }

    setRegLoading(true);

    try {
      console.log('🔍 Attempting guardian registration to backend...');
      const { ok, data } = await guardianRegister({ name, username, password, phone, email });

      if (ok && data.status === 'success') {
        showAlert(false, 'Registration successful! Switching to login...', 'success');

        setRegName('');
        setRegUsername('');
        setRegPassword('');
        setRegPhone('');
        setRegEmail('');

        setTimeout(() => {
          setIsLogin(true);
          setLoginUsername(username);
        }, 1500);
      } else {
        showAlert(false, data.message || 'Registration failed', 'error');
      }
    } catch (error) {
      console.error('Registration error:', error);
      showAlert(false, 'Error connecting to server', 'error');
    } finally {
      setRegLoading(false);
    }
  };

  const goBack = () => {
    navigate('/portal');
  };

  return (
    <div style={{
      background: 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)',
      minHeight: '100vh',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      fontFamily: "'Segoe UI', Tahoma, Geneva, Verdana, sans-serif",
      padding: '20px'
    }}>
      <style>{`
        .auth-card {
          background: white;
          border-radius: 20px;
          box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
          padding: 40px;
          max-width: 450px;
          width: 100%;
          margin: 20px;
          position: relative;
        }

        .back-btn-auth {
          position: absolute;
          top: 20px;
          left: 20px;
          background: rgba(102, 126, 234, 0.1);
          border: 1px solid rgba(102, 126, 234, 0.2);
          color: #667eea;
          padding: 8px;
          border-radius: 8px;
          cursor: pointer;
          display: flex;
          align-items: center;
          justify-content: center;
          transition: all 0.2s ease;
        }

        .back-btn-auth:hover {
          background: rgba(102, 126, 234, 0.2);
          transform: translateX(-2px);
        }

        .auth-header {
          text-align: center;
          margin-bottom: 30px;
        }

        .auth-header h1 {
          color: #667eea;
          margin-bottom: 10px;
          font-size: 28px;
        }

        .auth-header p {
          color: #666;
          font-size: 14px;
        }

        .form-group {
          margin-bottom: 20px;
        }

        .form-group label {
          display: block;
          margin-bottom: 8px;
          color: #333;
          font-weight: 500;
          font-size: 14px;
        }

        .form-group input {
          width: 100%;
          padding: 12px 15px;
          border: 2px solid #e0e0e0;
          border-radius: 8px;
          font-size: 14px;
          transition: border-color 0.3s;
          box-sizing: border-box;
        }

        .form-group input:focus {
          outline: none;
          border-color: #667eea;
          background-color: #f8f9ff;
        }

        .btn-auth {
          width: 100%;
          padding: 12px;
          border: none;
          border-radius: 8px;
          font-size: 16px;
          font-weight: 600;
          cursor: pointer;
          transition: all 0.3s;
          margin-bottom: 15px;
        }

        .btn-primary-auth {
          background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
          color: white;
        }

        .btn-primary-auth:hover {
          transform: translateY(-2px);
          box-shadow: 0 10px 20px rgba(102, 126, 234, 0.4);
        }

        .toggle-form {
          text-align: center;
          color: #666;
          font-size: 14px;
        }

        .toggle-form button {
          background: none;
          border: none;
          color: #667eea;
          font-weight: 600;
          cursor: pointer;
          padding: 0;
          font-size: 14px;
          text-decoration: underline;
        }

        .alert-box {
          padding: 12px 15px;
          border-radius: 8px;
          margin-bottom: 20px;
          font-size: 14px;
        }

        .alert-success {
          background: #d4edda;
          color: #155724;
          border: 1px solid #c3e6cb;
        }

        .alert-error {
          background: #f8d7da;
          color: #721c24;
          border: 1px solid #f5c6cb;
        }
      `}</style>

      <div className="auth-card">
        <button className="back-btn-auth" onClick={goBack} title="Go Back">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
            <path d="M19 12H5M12 19l-7-7 7-7" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
          </svg>
        </button>

        {isLogin ? (
          <div>
            <div className="auth-header">
              <h1>Guardian Login</h1>
              <p>Access your elderly family members</p>
            </div>

            {loginAlert.show && (
              <div className={`alert-box alert-${loginAlert.type}`}>
                {loginAlert.message}
              </div>
            )}

            <form onSubmit={handleLoginSubmit}>
              <div className="form-group">
                <label htmlFor="loginUsername">Username</label>
                <input
                  type="text"
                  id="loginUsername"
                  placeholder="Enter your username"
                  value={loginUsername}
                  onChange={(e) => setLoginUsername(e.target.value)}
                  required
                />
              </div>

              <div className="form-group">
                <label htmlFor="loginPassword">Password</label>
                <input
                  type="password"
                  id="loginPassword"
                  placeholder="Enter your password"
                  value={loginPassword}
                  onChange={(e) => setLoginPassword(e.target.value)}
                  required
                />
              </div>

              <button type="submit" className="btn-auth btn-primary-auth" disabled={loginLoading}>
                {loginLoading ? 'Logging in...' : 'Login'}
              </button>

              <div className="toggle-form">
                Don't have account? <button type="button" onClick={() => setIsLogin(false)}>Register here</button>
              </div>
            </form>
          </div>
        ) : (
          <div>
            <div className="auth-header">
              <h1>Guardian Registration</h1>
              <p>Create your account</p>
            </div>

            {regAlert.show && (
              <div className={`alert-box alert-${regAlert.type}`}>
                {regAlert.message}
              </div>
            )}

            <form onSubmit={handleRegisterSubmit}>
              <div className="form-group">
                <label htmlFor="regName">Full Name</label>
                <input
                  type="text"
                  id="regName"
                  placeholder="Enter your full name"
                  value={regName}
                  onChange={(e) => setRegName(e.target.value)}
                  required
                />
              </div>

              <div className="form-group">
                <label htmlFor="regUsername">Username</label>
                <input
                  type="text"
                  id="regUsername"
                  placeholder="Choose a username"
                  value={regUsername}
                  onChange={(e) => setRegUsername(e.target.value)}
                  required
                />
              </div>

              <div className="form-group">
                <label htmlFor="regPassword">Password</label>
                <input
                  type="password"
                  id="regPassword"
                  placeholder="Create a strong password"
                  value={regPassword}
                  onChange={(e) => setRegPassword(e.target.value)}
                  required
                />
              </div>

              <div className="form-group">
                <label htmlFor="regPhone">Phone Number</label>
                <input
                  type="tel"
                  id="regPhone"
                  placeholder="+91 98765 43210"
                  value={regPhone}
                  onChange={(e) => setRegPhone(e.target.value)}
                  required
                />
              </div>

              <div className="form-group">
                <label htmlFor="regEmail">Email Address</label>
                <input
                  type="email"
                  id="regEmail"
                  placeholder="your.email@example.com"
                  value={regEmail}
                  onChange={(e) => setRegEmail(e.target.value)}
                  required
                />
              </div>

              <button type="submit" className="btn-auth btn-primary-auth" disabled={regLoading}>
                {regLoading ? 'Creating account...' : 'Register'}
              </button>

              <div className="toggle-form">
                Already have account? <button type="button" onClick={() => setIsLogin(true)}>Login here</button>
              </div>
            </form>
          </div>
        )}
      </div>
    </div>
  );
}
