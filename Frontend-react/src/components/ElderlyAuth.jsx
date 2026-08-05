import React, { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { elderlyRegister } from '../services/api';

const ElderlyAuth = () => {
  const navigate = useNavigate();
  const [formData, setFormData] = useState({
    name: '',
    age: '',
    phone: '',
    location: '',
    medicalHistory: '',
    guardianUsername: '',
    guardianPassword: ''
  });
  const [rememberMe, setRememberMe] = useState(false);
  const [loading, setLoading] = useState(false);
  const [alert, setAlert] = useState({ show: false, message: '', type: '' });

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

  const handleRegister = async (e) => {
    e.preventDefault();
    
    if (!formData.name || !formData.age || !formData.phone || !formData.guardianUsername || !formData.guardianPassword) {
      showAlert('Please fill all required fields', 'error');
      return;
    }

    setLoading(true);

    try {
      const response = await elderlyRegister({
        name: formData.name,
        age: parseInt(formData.age),
        phone: formData.phone,
        location: formData.location,
        medical_history: formData.medicalHistory,
        guardian_username: formData.guardianUsername,
        guardian_password: formData.guardianPassword,
        preferred_language: formData.preferredLanguage || 'en'
      });

      if (response.status === 'success') {
        showAlert('Registration successful! Redirecting to login...', 'success');
        
        const elderlyKey = response.elderly_id || formData.name.toLowerCase().trim().replaceAll(/\s+/, '_');
        const preferredLang = formData.preferredLanguage || response.preferred_language || 'en';
        localStorage.setItem(`elderly_language_${elderlyKey}`, preferredLang);
        localStorage.setItem('app_lang', preferredLang);

        if (rememberMe) {
          localStorage.setItem('elderly_name', formData.name);
          localStorage.setItem('elderly_phone', formData.phone);
        }

        if (response.guardian_name) {
          localStorage.setItem('guardian_name', response.guardian_name);
        }
        if (response.guardian_phone) {
          localStorage.setItem('guardian_phone', response.guardian_phone);
        }

        setTimeout(() => {
          navigate('/login');
        }, 2000);
      } else {
        showAlert(response.message || response.error || 'Registration failed. Please check guardian credentials.', 'error');
      }
    } catch (error) {
      console.error('Registration error:', error);
      const serverMsg = error.response?.data?.message || error.response?.data?.error || 'Registration failed. Check guardian username & password.';
      showAlert(serverMsg, 'error');
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
      WebkitFontSmoothing: 'antialiased',
      MozOsxFontSmoothing: 'grayscale'
    }}>
      <div className="auth-container" style={{ textAlign: 'center', maxWidth: '450px' }}>
        <a href="#" className="back-link" onClick={(e) => { e.preventDefault(); navigate('/login'); }}>
          ← Back to Portal
        </a>

        {alert.show && (
          <div className={`alert alert-${alert.type}`} style={{ display: 'block' }}>
            {alert.message}
          </div>
        )}

        <div className="auth-header">
          <h1 style={{ color: '#28a745', margin: '0 0 10px 0', fontSize: '28px', fontWeight: '700' }}>
            Elderly Registration
          </h1>
          <p style={{ color: '#666', margin: '0', fontSize: '16px' }}>
            Join SilverCare and connect with your guardian
          </p>
        </div>

        <form onSubmit={handleRegister}>
          <div className="form-group">
            <label htmlFor="elderlyName">Full Name *</label>
            <input
              type="text"
              id="elderlyName"
              name="name"
              value={formData.name}
              onChange={handleInputChange}
              placeholder="Enter your full name"
              required
            />
          </div>

          <div className="form-group">
            <label htmlFor="elderlyAge">Age *</label>
            <input
              type="number"
              id="elderlyAge"
              name="age"
              value={formData.age}
              onChange={handleInputChange}
              placeholder="Enter your age"
              min="1"
              max="120"
              required
            />
          </div>

          <div className="form-group">
            <label htmlFor="elderlyPhone">Phone Number *</label>
            <input
              type="tel"
              id="elderlyPhone"
              name="phone"
              value={formData.phone}
              onChange={handleInputChange}
              placeholder="+919876543210"
              required
            />
          </div>

          <div className="form-group">
            <label htmlFor="elderlyLocation">Location</label>
            <input
              type="text"
              id="elderlyLocation"
              name="location"
              value={formData.location}
              onChange={handleInputChange}
              placeholder="Home, Hospital, etc."
            />
          </div>

          <div className="form-group">
            <label htmlFor="elderlyMedicalHistory">Medical History</label>
            <textarea
              id="elderlyMedicalHistory"
              name="medicalHistory"
              value={formData.medicalHistory}
              onChange={handleInputChange}
              placeholder="Diabetes, High BP, Heart conditions, etc."
            />
          </div>

          <div className="form-group">
            <label htmlFor="preferredLanguage">🌐 Preferred Language for Elderly *</label>
            <select
              id="preferredLanguage"
              name="preferredLanguage"
              value={formData.preferredLanguage || 'en'}
              onChange={handleInputChange}
              style={{
                width: '100%',
                padding: '12px',
                borderRadius: '8px',
                border: '1px solid #ccc',
                fontSize: '15px',
                fontWeight: '600'
              }}
            >
              <option value="en">English 🇬🇧</option>
              <option value="hi">हिन्दी (Hindi) 🇮🇳</option>
              <option value="mr">मराठी (Marathi) 🇮🇳</option>
            </select>
          </div>

          <div className="guardian-section">
            <h3>
              🔗 Guardian Connection
            </h3>
            <p style={{ fontSize: '14px', color: '#666', margin: '0 0 15px 0' }}>
              Enter your guardian's credentials to link your account
            </p>

            <div className="form-group">
              <label htmlFor="guardianUsername">Guardian Username *</label>
              <input
                type="text"
                id="guardianUsername"
                name="guardianUsername"
                value={formData.guardianUsername}
                onChange={handleInputChange}
                placeholder="e.g., isha"
                required
              />
            </div>

            <div className="form-group">
              <label htmlFor="guardianPassword">Guardian Password *</label>
              <input
                type="password"
                id="guardianPassword"
                name="guardianPassword"
                value={formData.guardianPassword}
                onChange={handleInputChange}
                placeholder="Ask guardian for password"
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
          </div>

          <button type="submit" className="btn btn-primary" disabled={loading}>
            <span style={{ display: loading ? 'none' : 'inline' }}>Register & Link to Guardian</span>
            <span className="loading" style={{ display: loading ? 'inline' : 'none' }}>Registering...</span>
          </button>

          <button
            type="button"
            className="btn btn-secondary"
            onClick={() => navigate('/login')}
          >
            Cancel
          </button>
        </form>
      </div>
    </div>
  );
};

export default ElderlyAuth;
