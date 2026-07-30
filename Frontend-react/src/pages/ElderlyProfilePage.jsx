import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { useLanguage } from '../context/LanguageContext';
import '../styles.css';

const ElderlyProfilePage = () => {
  const navigate = useNavigate();
  const { t } = useLanguage();

  const [elderlyName, setElderlyName] = useState('');
  const [elderlyPhone, setElderlyPhone] = useState('');
  const [guardianName, setGuardianName] = useState('');
  const [guardianPhone, setGuardianPhone] = useState('');

  useEffect(() => {
    const name = localStorage.getItem('elderly_name') || 'Senior Citizen';
    const phone = localStorage.getItem('elderly_phone') || '+91 93229 76718';
    const gName = localStorage.getItem('guardian_name') || 'Isha (Guardian)';
    const gPhone = localStorage.getItem('guardian_phone') || '+91 98765 43210';

    setElderlyName(name);
    setElderlyPhone(phone);
    setGuardianName(gName);
    setGuardianPhone(gPhone);
  }, []);

  const handleLogout = () => {
    if (window.confirm(t('confirm_logout') || 'Are you sure you want to logout?')) {
      localStorage.removeItem('elderlyLoggedIn');
      localStorage.removeItem('elderly_id');
      localStorage.removeItem('elderly_name');
      localStorage.removeItem('elderly_phone');
      navigate('/login');
    }
  };

  return (
    <div className="container" style={{ paddingBottom: '90px', backgroundColor: '#f8fafc', minHeight: '100vh' }}>
      {/* Header matching dashboard style */}
      <header className="header" style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '16px 20px', gap: '12px', borderBottom: '1px solid #e2e8f0', backgroundColor: '#ffffff' }}>
        <button className="back-btn" onClick={() => navigate('/home')} title="Go Back">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
            <path d="M19 12H5M12 19l-7-7 7-7" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
          </svg>
        </button>
        <h1 style={{ fontSize: '18px', fontWeight: '800', margin: 0, color: '#1f2937', textAlign: 'center', flex: 1 }}>
          👤 Profile & Account
        </h1>
        <div style={{ width: '38px', flexShrink: 0 }}></div>
      </header>

      <div style={{ padding: '20px' }}>
        
        {/* Profile Avatar Card */}
        <div style={{
          backgroundColor: '#ffffff',
          borderRadius: '20px',
          padding: '24px 20px',
          textAlign: 'center',
          boxShadow: '0 4px 16px rgba(0,0,0,0.04)',
          border: '1px solid #e2e8f0',
          marginBottom: '20px'
        }}>
          <div style={{
            width: '80px',
            height: '80px',
            borderRadius: '50%',
            backgroundColor: '#dbeafe',
            color: '#1d4ed8',
            fontSize: '40px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            margin: '0 auto 12px auto',
            border: '4px solid #bfdbfe'
          }}>
            👴
          </div>
          <h2 style={{ margin: '0 0 4px 0', fontSize: '22px', fontWeight: '800', color: '#0f172a' }}>
            {elderlyName}
          </h2>
          <div style={{
            display: 'inline-flex',
            alignItems: 'center',
            gap: '6px',
            backgroundColor: '#d1fae5',
            color: '#047857',
            padding: '4px 12px',
            borderRadius: '20px',
            fontSize: '12px',
            fontWeight: '700',
            marginTop: '4px'
          }}>
            <span style={{ width: '6px', height: '6px', backgroundColor: '#10b981', borderRadius: '50%' }}></span>
            Active Senior Member
          </div>
        </div>

        {/* Senior Personal Information Card */}
        <div style={{
          backgroundColor: '#ffffff',
          borderRadius: '20px',
          padding: '20px',
          boxShadow: '0 4px 16px rgba(0,0,0,0.04)',
          border: '1px solid #e2e8f0',
          marginBottom: '16px',
          textAlign: 'left'
        }}>
          <h3 style={{ margin: '0 0 14px 0', fontSize: '15px', fontWeight: '800', color: '#1e293b', textTransform: 'uppercase', letterSpacing: '0.5px' }}>
            👤 Senior Information
          </h3>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '10px 14px', backgroundColor: '#f8fafc', borderRadius: '12px' }}>
              <span style={{ fontSize: '13px', color: '#64748b', fontWeight: '600' }}>Full Name</span>
              <span style={{ fontSize: '14px', color: '#0f172a', fontWeight: '700' }}>{elderlyName}</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '10px 14px', backgroundColor: '#f8fafc', borderRadius: '12px' }}>
              <span style={{ fontSize: '13px', color: '#64748b', fontWeight: '600' }}>Phone Number</span>
              <span style={{ fontSize: '14px', color: '#2563eb', fontWeight: '700' }}>{elderlyPhone}</span>
            </div>
          </div>
        </div>

        {/* Guardian Information Card */}
        <div style={{
          backgroundColor: '#ffffff',
          borderRadius: '20px',
          padding: '20px',
          boxShadow: '0 4px 16px rgba(0,0,0,0.04)',
          border: '1px solid #e2e8f0',
          marginBottom: '24px',
          textAlign: 'left'
        }}>
          <h3 style={{ margin: '0 0 14px 0', fontSize: '15px', fontWeight: '800', color: '#1e293b', textTransform: 'uppercase', letterSpacing: '0.5px' }}>
            🛡️ Linked Guardian Information
          </h3>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '10px 14px', backgroundColor: '#f0fdf4', borderRadius: '12px', border: '1px solid #dcfce7' }}>
              <span style={{ fontSize: '13px', color: '#166534', fontWeight: '600' }}>Guardian Name</span>
              <span style={{ fontSize: '14px', color: '#14532d', fontWeight: '700' }}>{guardianName}</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '10px 14px', backgroundColor: '#f0fdf4', borderRadius: '12px', border: '1px solid #dcfce7' }}>
              <span style={{ fontSize: '13px', color: '#166534', fontWeight: '600' }}>Guardian Phone</span>
              <a href={`tel:${guardianPhone}`} style={{ fontSize: '14px', color: '#16a34a', fontWeight: '700', textDecoration: 'none' }}>
                📞 {guardianPhone}
              </a>
            </div>
          </div>
        </div>

        {/* Logout Section */}
        <button
          onClick={handleLogout}
          style={{
            width: '100%',
            backgroundColor: '#ef4444',
            color: '#ffffff',
            border: 'none',
            padding: '16px',
            borderRadius: '16px',
            fontSize: '16px',
            fontWeight: '700',
            cursor: 'pointer',
            boxShadow: '0 8px 20px rgba(239, 68, 68, 0.25)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '8px'
          }}
        >
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
            <path d="M9 21H5a2 2 0 01-2 2v1a2 2 0 01-2 2h4a2 2 0 01-2 2v1a2 2 0 01-2 2h4M16 17l-4 4-4 4v1a2 2 0 01-2 2h4a2 2 0 01-2 2v-1a2 2 0 01-2 2h-4a2 2 0 01-2 2v-1a2 2 0 01-2 2h4M7 14l5 5 5 5s-5 5h-1.71l-.29-.29a1 1 0 01-1.42 0l-1.29-1.29A1 1 0 017 7l3.59 3.59A2 2 0 0112 9l-3.59-3.59A2 2 0 0111 7z" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
          </svg>
          {t('logout') || 'Logout Account'}
        </button>

      </div>

      {/* Bottom Taskbar */}
      <nav className="bottom-nav">
        <a href="#" className="nav-item" onClick={(e) => { e.preventDefault(); navigate('/home'); }}>
          <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.2" strokeLinecap="round" strokeLinejoin="round">
            <path d="M3 9l9-7 9 7v11a2 2 0 01-2 2H5a2 2 0 01-2-2V9z"/>
            <polyline points="9 22 9 12 15 12 15 22"/>
          </svg>
          <span>{t('home')}</span>
        </a>

        <a href="#" className="nav-item" onClick={(e) => { e.preventDefault(); navigate('/health'); }}>
          <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.2" strokeLinecap="round" strokeLinejoin="round">
            <path d="M22 12h-4l-3 9L9 3l-3 9H2"/>
          </svg>
          <span>{t('health')}</span>
        </a>

        <button className="nav-item assistant-nav-btn" onClick={() => navigate('/assistant')} style={{ background: 'none', border: 'none', cursor: 'pointer' }}>
          <svg width="26" height="26" viewBox="0 0 24 24" fill="none">
            <path d="M12 4.5V2" stroke="#3b82f6" strokeWidth="2" strokeLinecap="round"/>
            <circle cx="12" cy="1.8" r="1.8" fill="#ef4444"/>
            <rect x="3" y="4.5" width="18" height="15" rx="7.5" fill="#eff6ff" stroke="#3b82f6" strokeWidth="2"/>
            <circle cx="6.5" cy="13.5" r="1.2" fill="#f472b6" opacity="0.8"/>
            <circle cx="17.5" cy="13.5" r="1.2" fill="#f472b6" opacity="0.8"/>
            <path d="M7.5 10c.8-.8 2.2-.8 3 0" stroke="#1d4ed8" strokeWidth="2.2" strokeLinecap="round"/>
            <path d="M13.5 10c.8-.8 2.2-.8 3 0" stroke="#1d4ed8" strokeWidth="2.2" strokeLinecap="round"/>
            <path d="M9.5 14c1.2 1.3 3.8 1.3 5 0" stroke="#1d4ed8" strokeWidth="2.2" strokeLinecap="round"/>
          </svg>
          <span>{t('assistant')}</span>
        </button>
      </nav>
    </div>
  );
};

export default ElderlyProfilePage;
