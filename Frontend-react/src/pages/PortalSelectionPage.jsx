import React from 'react';
import { useNavigate } from 'react-router-dom';

const PortalSelectionPage = () => {
  const navigate = useNavigate();
  const elderlyLoggedIn = localStorage.getItem('elderlyLoggedIn') === 'true' || !!localStorage.getItem('elderly_id');

  return (
    <div style={{
      background: '#ffffff',
      minHeight: '100vh',
      fontFamily: "-apple-system, BlinkMacSystemFont, 'Segoe UI', system-ui, sans-serif",
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      margin: 0,
      padding: '20px',
      boxSizing: 'border-box'
    }}>
      <div className="portal-container" style={{
        background: '#ffffff',
        borderRadius: '20px',
        padding: '40px 20px',
        boxShadow: '0 0 20px rgba(0, 0, 0, 0.1)',
        textAlign: 'center',
        maxWidth: '375px',
        width: '100%',
        boxSizing: 'border-box'
      }}>
        <h1 className="portal-title" style={{
          color: '#000000',
          fontSize: '32px',
          marginBottom: '8px',
          fontWeight: 700
        }}>SilverCare</h1>
        <p className="portal-subtitle" style={{
          color: '#6c6c70',
          fontSize: '16px',
          marginBottom: '40px',
          fontWeight: 400
        }}>Choose your portal to continue</p>
        
        <div className="portal-options" style={{
          display: 'flex',
          flexDirection: 'column',
          gap: '16px'
        }}>
          <button 
            onClick={() => navigate('/guardian-auth')} 
            className="portal-button"
            style={{
              background: '#007AFF',
              color: 'white',
              border: 'none',
              padding: '16px 24px',
              borderRadius: '12px',
              fontSize: '16px',
              fontWeight: 500,
              cursor: 'pointer',
              transition: 'all 0.15s ease',
              textDecoration: 'none',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '12px',
              minHeight: '52px',
              boxSizing: 'border-box',
              width: '100%'
            }}
          >
            <span className="icon" style={{ fontSize: '20px' }}>👤</span>
            <span>Guardian Portal</span>
          </button>
          
          <button 
            onClick={() => navigate('/login')} 
            className="portal-button elderly"
            style={{
              background: '#34c759',
              color: 'white',
              border: 'none',
              padding: '16px 24px',
              borderRadius: '12px',
              fontSize: '16px',
              fontWeight: 500,
              cursor: 'pointer',
              transition: 'all 0.15s ease',
              textDecoration: 'none',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '12px',
              minHeight: '52px',
              boxSizing: 'border-box',
              width: '100%'
            }}
          >
            <span className="icon" style={{ fontSize: '20px' }}>👴</span>
            <span>Elderly Login/Register</span>
          </button>
          
          {elderlyLoggedIn && (
            <button 
              onClick={() => navigate('/home')} 
              className="portal-button elderly"
              style={{
                background: '#34c759',
                color: 'white',
                border: 'none',
                padding: '16px 24px',
                borderRadius: '12px',
                fontSize: '16px',
                fontWeight: 500,
                cursor: 'pointer',
                transition: 'all 0.15s ease',
                textDecoration: 'none',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '12px',
                minHeight: '52px',
                boxSizing: 'border-box',
                width: '100%'
              }}
            >
              <span className="icon" style={{ fontSize: '20px' }}>🏠</span>
              <span>Elderly Portal</span>
              <small style={{ fontSize: '12px', opacity: 0.8, marginLeft: '4px' }}>(Logged In)</small>
            </button>
          )}
        </div>
      </div>
    </div>
  );
};

export default PortalSelectionPage;
