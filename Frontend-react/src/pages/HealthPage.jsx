import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import {
  getMedicines,
  confirmMedicineTaken,
  fetchSuggestions
} from '../services/api';
import { useLanguage } from '../context/LanguageContext';
import '../styles.css';

const HealthPage = () => {
  const navigate = useNavigate();
  const { lang, t, translateDynamic } = useLanguage();
  const [elderlyId, setElderlyId] = useState('');
  const [medicines, setMedicines] = useState([]);
  const [suggestions, setSuggestions] = useState([]);
  const [translatedSuggestions, setTranslatedSuggestions] = useState([]);

  useEffect(() => {
    const id = localStorage.getItem('elderly_id') || '';
    setElderlyId(id);

    loadData(id);
    const interval = setInterval(() => loadData(id), 3000);
    return () => clearInterval(interval);
  }, []);

  useEffect(() => {
    if (suggestions.length === 0) {
      setTranslatedSuggestions([]);
      return;
    }
    let isMounted = true;
    Promise.all(
      suggestions.map(async (sug) => {
        const rawText = typeof sug === 'string' ? sug : (sug.suggestion || sug.notes || sug.text || sug.message || '');
        if (!rawText) return '';
        const translated = await translateDynamic(rawText, lang);
        return translated;
      })
    ).then((results) => {
      if (isMounted) setTranslatedSuggestions(results);
    });
    return () => { isMounted = false; };
  }, [suggestions, lang, translateDynamic]);

  const loadData = async (id) => {
    try {
      const resMeds = await getMedicines(id);
      setMedicines(resMeds.medicines || (Array.isArray(resMeds) ? resMeds : []));

      const { ok, data } = await fetchSuggestions(id);
      if (ok && data) {
        setSuggestions(data.suggestions || (Array.isArray(data) ? data : []));
      }
    } catch (err) {}
  };

  const getCounts = () => {
    let total = 0;
    let taken = 0;
    let missed = 0;

    medicines.forEach(m => {
      const times = Array.isArray(m.times) ? m.times : [m.times];
      times.forEach(t => {
        total++;
        const status = localStorage.getItem(`medicine_${m.id}_${t}`);
        if (status === 'taken') taken++;
        else if (status === 'missed' || status === 'not_taken') missed++;
      });
    });

    return { total, taken, missed };
  };

  const markTaken = async (medicineId, time) => {
    const currentTime = time || new Date().toTimeString().slice(0, 5);
    localStorage.setItem(`medicine_${medicineId}_${currentTime}`, 'taken');
    try {
      await confirmMedicineTaken({
        medicineId,
        elderlyId: elderlyId || 'gauri_shiv',
        timeTaken: currentTime,
        taken: true
      });
    } catch (err) {}
    loadData(elderlyId);
  };

  const logout = () => {
    if (window.confirm(t('confirm_logout'))) {
      localStorage.clear();
      navigate('/login');
    }
  };

  const { total, taken, missed } = getCounts();

  return (
    <div className="container" style={{ paddingBottom: '90px' }}>
      {/* Header matching frontend/health.html */}
      <header className="header" style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '16px 20px', gap: '10px', borderBottom: '1px solid #e5e7eb' }}>
        <button className="back-btn" onClick={() => navigate('/home')} title="Go Back">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
            <path d="M19 12H5M12 19l-7-7 7-7" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
          </svg>
        </button>
        <h1 style={{ fontSize: '17px', fontWeight: '800', margin: 0, color: '#1f2937', textAlign: 'center', flex: 1 }}>
          🏥 {t('health_dashboard')}
        </h1>
        <div className="status-badge success">
          <svg width="16" height="16" viewBox="0 0 20 20" fill="none">
            <circle cx="10" cy="10" r="10" fill="currentColor"/>
            <path d="M6 10l3 3 5-6" stroke="white" strokeWidth="2" fill="none"/>
          </svg>
          <span>{t('active')}</span>
        </div>
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
            boxShadow: '0 2px 6px rgba(37,99,235,0.15)'
          }}
        >
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
            <path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"></path>
            <circle cx="12" cy="7" r="4"></circle>
          </svg>
        </button>
      </header>

      {/* Main Content matching frontend/health.html */}
      <div style={{ padding: '20px' }}>
        
        {/* Health Overview Cards */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px', marginBottom: '24px' }}>
          <div style={{ backgroundColor: '#fef3c7', padding: '16px 12px', borderRadius: '16px', textAlign: 'center', border: '1px solid #fde68a' }}>
            <div style={{ fontSize: '24px', marginBottom: '4px' }}>💊</div>
            <div style={{ fontSize: '24px', fontWeight: '800', color: '#92400e' }}>{total}</div>
            <div style={{ fontSize: '11px', fontWeight: '700', color: '#b45309', textTransform: 'uppercase' }}>{t('total_doses')}</div>
          </div>
          <div style={{ backgroundColor: '#d1fae5', padding: '16px 12px', borderRadius: '16px', textAlign: 'center', border: '1px solid #a7f3d0' }}>
            <div style={{ fontSize: '24px', marginBottom: '4px' }}>✅</div>
            <div style={{ fontSize: '24px', fontWeight: '800', color: '#065f46' }}>{taken}</div>
            <div style={{ fontSize: '11px', fontWeight: '700', color: '#047857', textTransform: 'uppercase' }}>{t('taken')}</div>
          </div>
          <div style={{ backgroundColor: '#fee2e2', padding: '16px 12px', borderRadius: '16px', textAlign: 'center', border: '1px solid #fecaca' }}>
            <div style={{ fontSize: '24px', marginBottom: '4px' }}>❌</div>
            <div style={{ fontSize: '24px', fontWeight: '800', color: '#991b1b' }}>{missed}</div>
            <div style={{ fontSize: '11px', fontWeight: '700', color: '#b91c1c', textTransform: 'uppercase' }}>{t('missed')}</div>
          </div>
        </div>

        {/* Today's Medicines Section */}
        <div style={{ marginBottom: '24px', textAlign: 'left' }}>
          <h2 style={{ fontSize: '18px', fontWeight: '800', color: '#1f2937', marginBottom: '14px' }}>
            💊 {t('todays_medicines')}
          </h2>

          {medicines.length === 0 ? (
            <div style={{ backgroundColor: '#f9fafb', padding: '24px', borderRadius: '16px', textAlign: 'center', color: '#6b7280' }}>
              <div style={{ fontSize: '36px', marginBottom: '8px' }}>💊</div>
              <p style={{ margin: 0, fontWeight: '600' }}>{t('no_medicines')}</p>
            </div>
          ) : (
            medicines.map((med, idx) => {
              const times = Array.isArray(med.times) ? med.times : [med.times];
              return (
                <div key={idx} style={{
                  backgroundColor: '#ffffff',
                  borderRadius: '16px',
                  padding: '16px',
                  marginBottom: '12px',
                  border: '1px solid #e5e7eb',
                  boxShadow: '0 2px 8px rgba(0,0,0,0.03)'
                }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                    <div>
                      <h4 style={{ margin: '0 0 4px 0', fontSize: '16px', fontWeight: '700', color: '#1f2937' }}>
                        {med.medicine_name || med.medicineName || med.name}
                      </h4>
                      <p style={{ margin: '0 0 4px 0', fontSize: '13px', color: '#6b7280' }}>
                        Dosage: {med.dosage || '1 Tablet'}
                      </p>
                      <p style={{ margin: 0, fontSize: '13px', fontWeight: '700', color: '#3b82f6' }}>
                        ⏰ Scheduled: {times.join(', ')}
                      </p>
                    </div>
                    <button
                      onClick={() => markTaken(med.id, times[0])}
                      style={{
                        backgroundColor: '#10b981',
                        color: 'white',
                        border: 'none',
                        padding: '10px 14px',
                        borderRadius: '10px',
                        fontWeight: '700',
                        fontSize: '13px',
                        cursor: 'pointer'
                      }}
                    >
                      {t('mark_taken')}
                    </button>
                  </div>
                </div>
              );
            })
          )}
        </div>

        {/* Guardian Suggestions Section */}
        <div style={{ textAlign: 'left' }}>
          <h2 style={{ fontSize: '18px', fontWeight: '800', color: '#1f2937', marginBottom: '14px' }}>
            📝 {t('caregiver_notes')}
          </h2>
          {suggestions.length === 0 ? (
            <div style={{ backgroundColor: '#f9fafb', padding: '20px', borderRadius: '16px', textAlign: 'center', color: '#6b7280' }}>
              <p style={{ margin: 0, fontWeight: '600' }}>{t('no_notes')}</p>
            </div>
          ) : (
            (translatedSuggestions.length > 0 ? translatedSuggestions : suggestions).map((sug, idx) => (
              <div key={idx} style={{
                backgroundColor: '#f0fdf4',
                borderRadius: '12px',
                padding: '14px',
                marginBottom: '10px',
                borderLeft: '4px solid #10b981',
                color: '#065f46',
                fontWeight: '600',
                fontSize: '14px'
              }}>
                📌 {typeof sug === 'string' ? sug : (sug.suggestion || sug.notes || sug.text || sug.message)}
              </div>
            ))
          )}
        </div>

      </div>

      {/* Bottom Taskbar matching frontend/index.html & frontend/health.html */}
      <nav className="bottom-nav">
        <a href="#" className="nav-item" onClick={(e) => { e.preventDefault(); navigate('/home'); }}>
          <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.2" strokeLinecap="round" strokeLinejoin="round">
            <path d="M3 9l9-7 9 7v11a2 2 0 01-2 2H5a2 2 0 01-2-2V9z"/>
            <polyline points="9 22 9 12 15 12 15 22"/>
          </svg>
          <span>{t('home')}</span>
        </a>

        <a href="#" className="nav-item active" onClick={(e) => e.preventDefault()}>
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

export default HealthPage;
