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
      localStorage.setItem('elderlyLoggedIn', 'true');
      navigate('/home');
      return;
    }

    if (elderlyName && elderlyPhone) {
      setFormData({ name: elderlyName, phone: elderlyPhone });
      setRememberMe(true);
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
        
        localStorage.setItem('elderlyLoggedIn', 'true');
        localStorage.setItem('elderly_id', response.elderly_id);
        localStorage.setItem('elderly_name', response.name);
        localStorage.setItem('elderly_phone', formData.phone);
        localStorage.setItem('elderly_remember_me', rememberMe.toString());

        if (rememberMe) {
          localStorage.setItem('elderly_name', formData.name);
          localStorage.setItem('elderly_phone', formData.phone);
        } else {
          localStorage.removeItem('elderly_name');
          localStorage.removeItem('elderly_phone');
        }

        setTimeout(() => {
          navigate('/home');
        }, 1500);
      } else {
        showAlert(response.message || 'Login failed. Please try again.', 'error');
      }
    } catch (error) {
      console.error('Login error:', error);
      showAlert('Login failed. Please try again.', 'error');
    } finally {
      setLoading(false);
    }
  };

  const goBack = () => {
    if (window.history.length > 1) {
      window.history.back();
    } else {
      navigate('/login');
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
      WebkitFontSmoothing: 'antialiased',
      MozOsxFontSmoothing: 'grayscale'
    }}>
      <div className="auth-container">
        <button className="back-btn" onClick={goBack} title="Go Back">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
            <path d="M19 12H5M12 19l-7-7 7-7" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
          </svg>
        </button>
        
        <div className="auth-header">
          <h1 className="auth-title">👴 Elderly Login</h1>
          <p className="auth-subtitle">Welcome back! Please login to continue</p>
        </div>

        {alert.show && (
          <div className={`alert alert-${alert.type}`} style={{ display: 'block' }}>
            {alert.message}
          </div>
        )}

        <form onSubmit={handleLogin}>
          <div className="form-group">
            <label htmlFor="elderlyName">Your Name *</label>
            <input
              type="text"
              id="elderlyName"
              name="name"
              value={formData.name}
              onChange={handleInputChange}
              placeholder="Enter your name"
              required
            />
          </div>

          <div className="form-group">
            <label htmlFor="elderlyPhone">Your Phone Number *</label>
            <input
              type="tel"
              id="elderlyPhone"
              name="phone"
              value={formData.phone}
              onChange={handleInputChange}
              placeholder="Enter your phone number"
              required
            />
          </div>

          <div className="remember-me">
            <input
              type="checkbox"
              id="rememberMe"
              checked={rememberMe}
              onChange={(e) => setRememberMe(e.target.checked)}
            />
            <label htmlFor="rememberMe">Remember me on this device</label>
          </div>

          <button type="submit" className="btn btn-primary" disabled={loading}>
            <span style={{ display: loading ? 'none' : 'inline' }}>Login to Portal</span>
            <span className="loading" style={{ display: loading ? 'inline' : 'none' }}>Logging in...</span>
          </button>

          <button
            type="button"
            className="btn btn-secondary"
            onClick={() => navigate('/register')}
          >
            New User? Register Here
          </button>
        </form>

        <a href="#" className="back-link" onClick={(e) => { e.preventDefault(); navigate('/login'); }}>
          ← Back to Portal Selection
        </a>
      </div>
    </div>
  );
};

export default ElderlyLogin;
