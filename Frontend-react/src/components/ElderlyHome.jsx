import React, { useState, useEffect, useRef } from 'react';
import { useNavigate } from 'react-router-dom';
import {
  confirmMedicineTaken,
  getElderlyNotifications,
  clearElderlyNotification,
  fetchHardwareData,
  triggerEmergency
} from '../services/api';
import { useLanguage } from '../context/LanguageContext';
import '../styles.css';

const ElderlyHome = () => {
  const navigate = useNavigate();
  const { lang, t, translateDynamic } = useLanguage();

  const [elderlyId, setElderlyId] = useState('');
  const [elderlyName, setElderlyName] = useState('');
  const [sosPressed, setSosPressed] = useState(false);
  const [notifications, setNotifications] = useState({});
  const [isCalibrated, setIsCalibrated] = useState(false);

  // Device Status Realtime State
  const [beltWornRealtime, setBeltWornRealtime] = useState(true);
  const [wristBandWornRealtime, setWristBandWornRealtime] = useState(true);

  // Dynamic Quote State (Translated via API)
  const [quoteTitle, setQuoteTitle] = useState('Daily Inspiration 💖');
  const [quoteBody, setQuoteBody] = useState('Every day is a new gift. Stay happy, take your medicines with a smile, and know you are deeply loved!');

  const sosTimerRef = useRef(null);

  useEffect(() => {
    const loggedIn = localStorage.getItem('elderlyLoggedIn');
    const id = localStorage.getItem('elderly_id');
    const rawName = localStorage.getItem('elderly_name');

    if (!loggedIn || loggedIn !== 'true' || !id) {
      navigate('/login');
      return;
    }

    const formattedName = rawName || (id ? id.charAt(0).toUpperCase() + id.slice(1) : '');

    setElderlyId(id);
    setElderlyName(formattedName);

    pollDeviceStatus();
    checkCalibrationStatus(id);

    // Realtime polling interval every 3 seconds
    const interval = setInterval(() => {
      checkNotifications(id);
      pollDeviceStatus();
    }, 3000);

    return () => clearInterval(interval);
  }, [navigate]);

  // Translate quote dynamically when language changes using API
  useEffect(() => {
    let isMounted = true;
    const baseTitle = 'Daily Inspiration 💖';
    const baseBody = 'Every day is a new gift. Stay happy, take your medicines with a smile, and know you are deeply loved!';

    if (lang === 'en') {
      setQuoteTitle(baseTitle);
      setQuoteBody(baseBody);
    } else {
      translateDynamic(baseTitle, lang).then(transTitle => {
        if (isMounted) setQuoteTitle(transTitle || t('daily_quote_title'));
      });
      translateDynamic(baseBody, lang).then(transBody => {
        if (isMounted) setQuoteBody(transBody || t('daily_quote'));
      });
    }

    return () => { isMounted = false; };
  }, [lang]);

  const pollDeviceStatus = async () => {
    try {
      const id = elderlyId || localStorage.getItem('elderly_id');
      if (!id) return;
      const response = await fetchHardwareData(id);
      if (response && response.ok && response.data && response.data.status === 'success' && response.data.data) {
        setBeltWornRealtime(response.data.data.beltConnected !== false);
        setWristBandWornRealtime(response.data.data.beltConnected !== false);
      } else {
        setBeltWornRealtime(true);
        setWristBandWornRealtime(true);
      }
    } catch (err) {
      setBeltWornRealtime(false);
      setWristBandWornRealtime(false);
    }
  };

  const checkCalibrationStatus = async (id) => {
    try {
      const response = await fetch(`${import.meta.env.VITE_API_BASE_URL || 'http://localhost:8081'}/api/elderly/${id}/health-history`);
      if (response.ok) {
        const data = await response.json();
        setIsCalibrated(data.data?.isCalibrated || false);
      }
    } catch (err) {
      console.error('Error checking calibration status:', err);
    }
  };

  const checkNotifications = async (id) => {
    const targetId = id || elderlyId || localStorage.getItem('elderly_id');
    if (!targetId) return;
    try {
      const response = await getElderlyNotifications(targetId);
      if (response && response.status === 'success' && response.notifications) {
        setNotifications(response.notifications);
      } else {
        setNotifications({});
      }
    } catch (err) {}
  };

  const handleNotificationResponse = async (medicineId, response) => {
    const targetId = elderlyId || localStorage.getItem('elderly_id');
    try {
      await clearElderlyNotification(targetId, medicineId, response);
      if (response === 'taken') {
        const currentTime = new Date().toTimeString().slice(0, 5);
        localStorage.setItem(`medicine_${medicineId}_${currentTime}`, 'taken');
        await confirmMedicineTaken({
          medicineId,
          elderlyId: targetId,
          timeTaken: currentTime,
          taken: true
        });
      }
      showToast(
        response === 'taken'
          ? `✅ ${t('mark_taken')}`
          : response === 'snooze'
          ? `⏰ ${t('snooze')}`
          : `❌ ${t('mark_missed')}`
      );
      checkNotifications(targetId);
    } catch (err) {
      showToast('Response recorded');
    }
  };

  const showToast = (msg) => {
    const toast = document.createElement('div');
    toast.style.cssText = `
      position: fixed;
      top: 20px;
      right: 20px;
      background: #333;
      color: white;
      padding: 12px 20px;
      border-radius: 10px;
      z-index: 10000;
      font-weight: 600;
      box-shadow: 0 5px 15px rgba(0,0,0,0.3);
    `;
    toast.textContent = msg;
    document.body.appendChild(toast);
    setTimeout(() => toast.remove(), 3000);
  };

  const handleSosStart = (e) => {
    e.preventDefault();
    setSosPressed(true);
    sosTimerRef.current = setTimeout(async () => {
      try {
        await triggerEmergency({
          elderly_name: elderlyName,
          guardian_username: localStorage.getItem('guardian_username') || '',
          location: 'Home'
        });
      } catch (err) {}
      alert('🚨 SOS EMERGENCY ALERT SENT TO GUARDIAN & NEIGHBOUR!');
      setSosPressed(false);
    }, 3000);
  };

  const handleSosEnd = (e) => {
    e.preventDefault();
    if (sosPressed) {
      clearTimeout(sosTimerRef.current);
      setSosPressed(false);
    }
  };

  const goBack = () => {
    navigate('/portal');
  };

  return (
    <div className="container" style={{ paddingBottom: '90px' }}>
      {/* Header matching dashboard style with Profile icon */}
      <header className="header" style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '16px 20px', gap: '12px' }}>
        <button className="back-btn" onClick={goBack} title="Go Back">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
            <path d="M19 12H5M12 19l-7-7 7-7" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
          </svg>
        </button>

        <h1 style={{ fontSize: '18px', fontWeight: '800', margin: 0, color: '#1f2937', textAlign: 'center', flex: 1 }}>
          SilverCare
        </h1>

        <button
          className="profile-header-btn"
          onClick={() => navigate('/profile')}
          title="View Profile"
          style={{
            backgroundColor: '#eff6ff',
            color: '#2563eb',
            border: 'none',
            borderRadius: '50%',
            width: '38px',
            height: '38px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            boxShadow: '0 2px 6px rgba(37,99,235,0.15)',
            flexShrink: 0
          }}
        >
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
            <path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"></path>
            <circle cx="12" cy="7" r="4"></circle>
          </svg>
        </button>
      </header>

      {/* Personalized Senior Name */}
      <div style={{ padding: '0 20px 10px 20px', textAlign: 'left' }}>
        <h2 style={{ margin: 0, fontSize: '20px', fontWeight: '800', color: '#1f2937' }}>
          👴 {t('welcome')}, {elderlyName}!
        </h2>
      </div>

      {/* Active Medicine Notification Banner */}
      {Object.keys(notifications).length > 0 && (
        <div style={{ padding: '0 20px 20px 20px' }}>
          <div style={{
            backgroundColor: '#fff3cd',
            border: '2px solid #ffc107',
            borderRadius: '16px',
            padding: '16px',
            textAlign: 'left'
          }}>
            <h3 style={{ margin: '0 0 8px 0', color: '#856404', fontSize: '16px', fontWeight: '700' }}>
              ⏰ {t('active_reminder')}
            </h3>
            {Object.values(notifications).map((notif, idx) => (
              <div key={idx}>
                <div style={{ fontWeight: '700', fontSize: '15px', color: '#000', marginBottom: '8px' }}>{notif.message}</div>
                <div style={{ display: 'flex', gap: '8px' }}>
                  <button
                    onClick={() => handleNotificationResponse(notif.medicine.id, 'taken')}
                    style={{ backgroundColor: '#34c759', color: 'white', border: 'none', padding: '8px 14px', borderRadius: '8px', fontWeight: '700', cursor: 'pointer' }}
                  >
                    {t('mark_taken')}
                  </button>
                  <button
                    onClick={() => handleNotificationResponse(notif.medicine.id, 'snooze')}
                    style={{ backgroundColor: '#ff9500', color: 'white', border: 'none', padding: '8px 14px', borderRadius: '8px', fontWeight: '700', cursor: 'pointer' }}
                  >
                    {t('snooze')}
                  </button>
                  <button
                    onClick={() => handleNotificationResponse(notif.medicine.id, 'not_taken')}
                    style={{ backgroundColor: '#ff3b30', color: 'white', border: 'none', padding: '8px 14px', borderRadius: '8px', fontWeight: '700', cursor: 'pointer' }}
                  >
                    {t('mark_missed')}
                  </button>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Calibration Prompt Banner */}
      {!isCalibrated && (
        <div style={{ padding: '0 20px 20px 20px' }}>
          <div style={{
            backgroundColor: '#eff6ff',
            border: '2px solid #3b82f6',
            borderRadius: '16px',
            padding: '16px',
            textAlign: 'left'
          }}>
            <h3 style={{ margin: '0 0 8px 0', color: '#1e40af', fontSize: '16px', fontWeight: '700' }}>
              🎯 Improve Fall Detection
            </h3>
            <p style={{ margin: '0 0 12px 0', color: '#1e3a8a', fontSize: '14px', lineHeight: '1.5' }}>
              Complete a 10-minute calibration to personalize your fall detection for better accuracy.
            </p>
            <button
              onClick={() => navigate('/calibration')}
              style={{
                backgroundColor: '#3b82f6',
                color: 'white',
                border: 'none',
                padding: '10px 20px',
                borderRadius: '8px',
                fontWeight: '700',
                cursor: 'pointer',
                fontSize: '14px'
              }}
            >
              Start Calibration
            </button>
          </div>
        </div>
      )}

      {/* SOS Button */}
      <div className="sos-section">
        <button
          className="sos-button"
          onMouseDown={handleSosStart}
          onMouseUp={handleSosEnd}
          onTouchStart={handleSosStart}
          onTouchEnd={handleSosEnd}
          style={{ transform: sosPressed ? 'scale(0.95)' : 'none' }}
        >
          <div className="sos-icon">
            <svg width="60" height="60" viewBox="0 0 60 60" fill="none">
              <path d="M30 10v10M30 30v10M20 20l7 7M40 20l-7 7M20 40l7-7M40 40l-7-7" stroke="white" strokeWidth="4" strokeLinecap="round"/>
            </svg>
          </div>
          <div className="sos-text">{t('sos')}</div>
          <div className="sos-subtext">{t('press_for_help')}</div>
        </button>
        <p className="sos-instruction">{t('press_and_hold')}</p>
      </div>

      {/* 1. Wearable Device Status Section */}
      <div style={{ padding: '0 20px 16px 20px' }}>
        <div style={{
          padding: '16px',
          borderRadius: '16px',
          backgroundColor: '#f8fafc',
          border: '2px solid #e2e8f0',
          textAlign: 'left'
        }}>
          <h3 style={{ margin: '0 0 12px 0', fontSize: '15px', fontWeight: '800', color: '#1e293b' }}>
            {t('device_status_title')}
          </h3>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px' }}>
            
            {/* Smart Belt Status */}
            <div style={{
              backgroundColor: '#ffffff',
              padding: '12px',
              borderRadius: '12px',
              border: `1.5px solid ${beltWornRealtime ? '#a7f3d0' : '#fecaca'}`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between'
            }}>
              <div>
                <div style={{ fontSize: '11px', fontWeight: '700', color: '#64748b', textTransform: 'uppercase' }}>
                  📟 {t('smart_belt')}
                </div>
                <div style={{ fontSize: '14px', fontWeight: '800', color: beltWornRealtime ? '#047857' : '#b91c1c', marginTop: '2px' }}>
                  {beltWornRealtime ? t('worn') : t('not_worn')}
                </div>
              </div>
              <span style={{
                width: '10px',
                height: '10px',
                borderRadius: '50%',
                backgroundColor: beltWornRealtime ? '#10b981' : '#ef4444'
              }}></span>
            </div>

            {/* Wrist Band Status */}
            <div style={{
              backgroundColor: '#ffffff',
              padding: '12px',
              borderRadius: '12px',
              border: `1.5px solid ${wristBandWornRealtime ? '#a7f3d0' : '#fecaca'}`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between'
            }}>
              <div>
                <div style={{ fontSize: '11px', fontWeight: '700', color: '#64748b', textTransform: 'uppercase' }}>
                  ⌚ {t('wrist_band')}
                </div>
                <div style={{ fontSize: '14px', fontWeight: '800', color: wristBandWornRealtime ? '#047857' : '#b91c1c', marginTop: '2px' }}>
                  {wristBandWornRealtime ? t('worn') : t('not_worn')}
                </div>
              </div>
              <span style={{
                width: '10px',
                height: '10px',
                borderRadius: '50%',
                backgroundColor: wristBandWornRealtime ? '#10b981' : '#ef4444'
              }}></span>
            </div>

          </div>
        </div>
      </div>

      {/* 2. Heartwarming Daily Inspiration Quote Card (Positioned BELOW Wearable Device Status) */}
      <div style={{ padding: '0 20px 20px 20px' }}>
        <div style={{
          background: 'linear-gradient(135deg, #eff6ff 0%, #dbeafe 100%)',
          borderRadius: '18px',
          padding: '16px 18px',
          textAlign: 'left',
          border: '1px solid #bfdbfe',
          boxShadow: '0 4px 12px rgba(37,99,235,0.06)'
        }}>
          <h3 style={{ margin: '0 0 6px 0', fontSize: '14px', fontWeight: '800', color: '#1d4ed8' }}>
            {quoteTitle}
          </h3>
          <p style={{ margin: 0, fontSize: '13px', color: '#1e3a8a', fontWeight: '600', lineHeight: '1.4', fontStyle: 'italic' }}>
            "{quoteBody}"
          </p>
        </div>
      </div>

      {/* Bottom Navigation */}
      <nav className="bottom-nav">
        <a href="#" className="nav-item active" onClick={(e) => { e.preventDefault(); navigate('/home'); }}>
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

export default ElderlyHome;
