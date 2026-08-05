import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { elderlyLogin } from '../services/api';

const ElderlyLogin = () => {
  const navigate = useNavigate();
  const [formData, setFormData] = useState({
    name: '',
    phone: ''
  });
  const [rememberMe, setRememberMe] = useState(false);
  const [loading, setLoading] = useState(false);
  const [alert, setAlert] = useState({ show: false, message: '', type: '' });

  useEffect(() => {
    const elderlyName = localStorage.getItem('elderly_name');
    const elderlyPhone = localStorage.getItem('elderly_phone');
    const rememberMeStored = localStorage.getItem('elderly_remember_me');

    if (elderlyName && elderlyPhone && rememberMeStored === 'true') {
      // Validate credentials against DB before auto-logging in
      elderlyLogin(elderlyName, elderlyPhone)
        .then((response) => {
          if (response && response.status === 'success') {
            const loggedInId = response.elderly_id || response.id || elderlyName.toLowerCase().trim().replaceAll(/\s+/, '_');
            localStorage.setItem('elderly_id', loggedInId);
            localStorage.setItem('elderly_name', response.name || elderlyName);
            localStorage.setItem('elderly_phone', response.phone || elderlyPhone);
            localStorage.setItem('elderlyLoggedIn', 'true');
            navigate('/home');
          } else {
            // Credentials not in DB - clear session
            localStorage.removeItem('elderly_remember_me');
            localStorage.removeItem('elderlyLoggedIn');
            localStorage.removeItem('elderly_id');
            localStorage.removeItem('elderly_name');
            localStorage.removeItem('elderly_phone');
            setRememberMe(false);
          }
        })
        .catch(() => {
          localStorage.removeItem('elderly_remember_me');
          localStorage.removeItem('elderlyLoggedIn');
        });
    } else {
      setRememberMe(false);
    }
  }, [navigate]);

  const showAlert = (message, type) => {
    setAlert({ show: true, message, type });
    if (type === 'success') {
      setTimeout(() => setAlert({ show: false, message: '', type: '' }), 4000);
    }
  };

  const handleInputChange = (e) => {
    setFormData({
      ...formData,
      [e.target.name]: e.target.value
    });
  };

  const handleLogin = async (e) => {
    e.preventDefault();
    
    if (!formData.name || !formData.phone) {
      showAlert('Please fill all required fields', 'error');
      return;
    }

    setLoading(true);

    try {
      const response = await elderlyLogin(formData.name, formData.phone);

      if (response.status === 'success') {
        showAlert('Login successful! Redirecting to portal...', 'success');

        if (rememberMe) {
          localStorage.setItem('elderly_remember_me', 'true');
        } else {
          localStorage.removeItem('elderly_remember_me');
        }

        const loggedInName = response.name || formData.name;
        const loggedInId = response.elderly_id || response.id || formData.name.toLowerCase().trim().replaceAll(/\s+/, '_');
        const loggedInPhone = response.phone || formData.phone || '';
        let guardianName = response.guardian_name || response.guardianName || response.guardian_username || '';
        let guardianPhone = response.guardian_phone || response.guardianPhone || '';
        const preferredLang = response.preferred_language || response.preferredLanguage || 'en';

        if (guardianName.includes('Isha (Guardian)')) guardianName = response.guardian_username || '';
        if (guardianPhone.includes('98765 43210') || guardianPhone.includes('9876543210')) guardianPhone = '';

        localStorage.setItem('elderly_id', loggedInId);
        localStorage.setItem('elderly_name', loggedInName);
        localStorage.setItem('elderly_phone', loggedInPhone);
        localStorage.setItem('guardian_name', guardianName);
        localStorage.setItem('guardian_phone', guardianPhone);
        localStorage.setItem(`elderly_language_${loggedInId}`, preferredLang);
        localStorage.setItem('app_lang', preferredLang);
        localStorage.setItem('elderlyLoggedIn', 'true');

        setTimeout(() => {
          navigate('/home');
        }, 1000);
      } else {
        showAlert(response.message || 'Login failed', 'error');
      }
    } catch (error) {
      showAlert('Server error. Please try again.', 'error');
      console.error(error);
    } finally {
      setLoading(false);
    }
  };

  return (
    <div style={{
      background: 'linear-gradient(135deg, #28a745 0%, #20c997 100%)',
      minHeight: '100vh',
      fontFamily: "'Segoe UI', Tahoma, Geneva, Verdana, sans-serif",
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      margin: 0,
      padding: '20px',
      boxSizing: 'border-box'
    }}>
      <div className="auth-container" style={{
        background: 'white',
        borderRadius: '20px',
        padding: '40px',
        boxShadow: '0 20px 40px rgba(0, 0, 0, 0.1)',
        width: '100%',
        maxWidth: '400px',
        position: 'relative',
        boxSizing: 'border-box',
        textAlign: 'center'
      }}>
        <button 
          className="back-btn" 
          onClick={() => navigate('/')} 
          title="Go Back"
          style={{
            position: 'absolute',
            top: '20px',
            left: '20px',
            background: 'rgba(40, 167, 69, 0.1)',
            border: '1px solid rgba(40, 167, 69, 0.2)',
            color: '#28a745',
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

        <div className="auth-header" style={{ marginBottom: '30px', marginTop: '10px' }}>
          <h1 className="auth-title" style={{
            color: '#28a745',
            fontSize: '28px',
            fontWeight: 700,
            marginBottom: '8px'
          }}>👴 Elderly Login</h1>
          <p className="auth-subtitle" style={{
            color: '#666',
            fontSize: '16px',
            marginBottom: '20px'
          }}>Welcome back! Please login to continue</p>
        </div>

        {alert.show && (
          <div style={{
            padding: '12px 16px',
            borderRadius: '8px',
            marginBottom: '20px',
            fontSize: '14px',
            background: alert.type === 'success' ? '#d4edda' : '#f8d7da',
            color: alert.type === 'success' ? '#155724' : '#721c24',
            border: alert.type === 'success' ? '1px solid #c3e6cb' : '1px solid #f5c6cb'
          }}>
            {alert.message}
          </div>
        )}

        <form onSubmit={handleLogin}>
          <div className="form-group" style={{ marginBottom: '20px', textAlign: 'left' }}>
            <label htmlFor="elderlyName" style={{
              display: 'block',
              marginBottom: '8px',
              color: '#333',
              fontWeight: 500,
              fontSize: '14px'
            }}>Your Name *</label>
            <input
              type="text"
              id="elderlyName"
              name="name"
              value={formData.name}
              onChange={handleInputChange}
              placeholder="Enter your name"
              required
              style={{
                width: '100%',
                padding: '12px 16px',
                border: '2px solid #e1e1e1',
                borderRadius: '10px',
                fontSize: '16px',
                boxSizing: 'border-box'
              }}
            />
          </div>

          <div className="form-group" style={{ marginBottom: '20px', textAlign: 'left' }}>
            <label htmlFor="elderlyPhone" style={{
              display: 'block',
              marginBottom: '8px',
              color: '#333',
              fontWeight: 500,
              fontSize: '14px'
            }}>Your Phone Number *</label>
            <input
              type="tel"
              id="elderlyPhone"
              name="phone"
              value={formData.phone}
              onChange={handleInputChange}
              placeholder="Enter your phone number"
              required
              style={{
                width: '100%',
                padding: '12px 16px',
                border: '2px solid #e1e1e1',
                borderRadius: '10px',
                fontSize: '16px',
                boxSizing: 'border-box'
              }}
            />
          </div>

          <div className="remember-me" style={{
            display: 'flex',
            alignItems: 'center',
            marginBottom: '20px',
            textAlign: 'left'
          }}>
            <input
              type="checkbox"
              id="rememberMe"
              checked={rememberMe}
              onChange={(e) => setRememberMe(e.target.checked)}
              style={{ marginRight: '8px' }}
            />
            <label htmlFor="rememberMe" style={{ color: '#666', fontSize: '14px', cursor: 'pointer' }}>Remember me on this device</label>
          </div>

          <button 
            type="submit" 
            className="btn btn-primary" 
            disabled={loading}
            style={{
              padding: '14px 24px',
              border: 'none',
              borderRadius: '10px',
              fontSize: '16px',
              fontWeight: 600,
              cursor: 'pointer',
              display: 'inline-block',
              width: '100%',
              marginBottom: '12px',
              boxSizing: 'border-box',
              background: '#28a745',
              color: 'white'
            }}
          >
            {loading ? 'Logging in...' : 'Login to Portal'}
          </button>

          <button
            type="button"
            className="btn btn-secondary"
            onClick={() => navigate('/register')}
            style={{
              padding: '14px 24px',
              border: 'none',
              borderRadius: '10px',
              fontSize: '16px',
              fontWeight: 600,
              cursor: 'pointer',
              display: 'inline-block',
              width: '100%',
              marginBottom: '12px',
              boxSizing: 'border-box',
              background: '#6c757d',
              color: 'white'
            }}
          >
            New User? Register Here
          </button>
        </form>

        <a 
          href="#" 
          className="back-link" 
          onClick={(e) => { e.preventDefault(); navigate('/'); }}
          style={{
            color: '#28a745',
            textDecoration: 'none',
            fontSize: '14px',
            marginTop: '15px',
            display: 'inline-block'
          }}
        >
          ← Back to Portal Selection
        </a>
      </div>
    </div>
  );
};

export default ElderlyLogin;
