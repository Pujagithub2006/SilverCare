import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { useLanguage } from '../context/LanguageContext';
import { fetchElderlyInfo, fetchHealthHistory, updateHealthHistory, fetchElderlyContacts, updateElderlyContacts } from '../services/api';
import '../styles.css';

const ElderlyProfilePage = () => {
  const navigate = useNavigate();
  const { lang, changeLanguage, t } = useLanguage();

  const [elderlyName, setElderlyName] = useState('');
  const [elderlyPhone, setElderlyPhone] = useState('');
  const [guardianName, setGuardianName] = useState('');
  const [guardianPhone, setGuardianPhone] = useState('');
  
  // Emergency contacts state
  const [emergencyContacts, setEmergencyContacts] = useState({
    hospitalPhone: '',
    ambulancePhone: '',
    hospitalName: '',
    secondaryGuardianName: '',
    secondaryGuardianPhone: '',
    neighborName: '',
    neighborPhone: ''
  });
  const [showEmergencyForm, setShowEmergencyForm] = useState(false);
  
  // Health history state
  const [showHealthForm, setShowHealthForm] = useState(false);
  const [healthData, setHealthData] = useState({
    age: '',
    weight: '',
    height: '',
    conditions: [],
    mobility: 'independent',
    fallCount: 0,
    lastFallDays: 365,
    medications: ''
  });

  useEffect(() => {
    let storedGName = localStorage.getItem('guardian_name') || '';
    let storedGPhone = localStorage.getItem('guardian_phone') || '';

    if (storedGName.includes('Isha (Guardian)')) {
      localStorage.removeItem('guardian_name');
      storedGName = '';
    }
    if (storedGPhone.includes('98765 43210') || storedGPhone.includes('9876543210')) {
      localStorage.removeItem('guardian_phone');
      storedGPhone = '';
    }

    const id = localStorage.getItem('elderly_id');
    const name = localStorage.getItem('elderly_name') || 'Senior Citizen';
    const phone = localStorage.getItem('elderly_phone') || '';

    console.log('[ELDERLY PROFILE] elderly_id from localStorage:', id);
    console.log('[ELDERLY PROFILE] Using targetId:', id || 'default_senior');

    setElderlyName(name);
    setElderlyPhone(phone);
    setGuardianName(storedGName);
    setGuardianPhone(storedGPhone);

    const targetId = id || 'default_senior';
    if (id) {
      fetchElderlyInfo(id).then((res) => {
        if (res.ok && res.data && res.data.status === 'success') {
          const profile = res.data.data;
          if (profile) {
            if (profile.name) {
              setElderlyName(profile.name);
              localStorage.setItem('elderly_name', profile.name);
            }
            if (profile.phone !== undefined) {
              setElderlyPhone(profile.phone || '');
              localStorage.setItem('elderly_phone', profile.phone || '');
            }

            const realGName = profile.guardian_name || profile.guardian_username || res.data.guardian_name || res.data.guardian_username || '';
            const realGPhone = profile.guardian_phone || res.data.guardian_phone || '';

            setGuardianName(realGName);
            setGuardianPhone(realGPhone);
            if (realGName) localStorage.setItem('guardian_name', realGName);
            if (realGPhone) localStorage.setItem('guardian_phone', realGPhone);

            // Load emergency contacts
            setEmergencyContacts({
              hospitalPhone: profile.hospitalPhone || profile.hospital_phone || '',
              ambulancePhone: profile.ambulancePhone || profile.ambulance_phone || '',
              hospitalName: profile.hospitalName || profile.hospital_name || '',
              secondaryGuardianName: profile.secondaryGuardianName || profile.secondary_guardian_name || '',
              secondaryGuardianPhone: profile.secondaryGuardianPhone || profile.secondary_guardian_phone || '',
              neighborName: profile.neighborName || profile.neighbourName || profile.neighbor_name || profile.neighbour_name || '',
              neighborPhone: profile.neighborPhone || profile.neighbourPhone || profile.neighbor_phone || profile.neighbour_phone || ''
            });

            const prefLang = profile.preferred_language || res.data.preferred_language;
            if (prefLang) {
              changeLanguage(prefLang, id);
            }
          }
        }
      }).catch((err) => console.error('Failed to fetch profile:', err));
    }

    // Fetch emergency contacts using API service
    fetchElderlyContacts(targetId).then(res => {
      if (res && res.data) {
        const cData = res.data;
        setEmergencyContacts({
          hospitalPhone: cData.hospitalPhone || cData.hospital_phone || '',
          ambulancePhone: cData.ambulancePhone || cData.ambulance_phone || '',
          hospitalName: cData.hospitalName || cData.hospital_name || '',
          secondaryGuardianName: cData.secondaryGuardianName || cData.secondary_guardian_name || '',
          secondaryGuardianPhone: cData.secondaryGuardianPhone || cData.secondary_guardian_phone || '',
          neighborName: cData.neighborName || cData.neighbourName || cData.neighbor_name || cData.neighbour_name || '',
          neighborPhone: cData.neighborPhone || cData.neighbourPhone || cData.neighbor_phone || cData.neighbour_phone || ''
        });
      }
    }).catch(err => console.error('Failed to load emergency contacts:', err));

    // Fetch existing health history using API service
    fetchHealthHistory(targetId).then(data => {
      if (data && data.data) {
        const hData = data.data;
        let parsedConditions = [];
        if (Array.isArray(hData.conditions)) {
          parsedConditions = hData.conditions;
        } else if (typeof hData.conditions === 'string' && hData.conditions.trim()) {
          parsedConditions = hData.conditions.split(',').map(s => s.trim());
        }

        setHealthData(prev => ({
          ...prev,
          age: hData.age !== null && hData.age !== undefined ? String(hData.age) : prev.age,
          weight: hData.weight !== null && hData.weight !== undefined ? String(hData.weight) : prev.weight,
          height: hData.height !== null && hData.height !== undefined ? String(hData.height) : prev.height,
          conditions: parsedConditions.length > 0 ? parsedConditions : prev.conditions,
          mobility: hData.mobility || prev.mobility,
          fallCount: hData.fallCount !== null && hData.fallCount !== undefined ? String(hData.fallCount) : prev.fallCount,
          lastFallDays: hData.lastFallDays !== null && hData.lastFallDays !== undefined ? String(hData.lastFallDays) : prev.lastFallDays,
          medications: hData.medications || prev.medications
        }));
      }
    }).catch(err => console.error('Failed to load health history:', err));
  }, [changeLanguage]);

  const handleLogout = () => {
    if (window.confirm(t('confirm_logout') || 'Are you sure you want to logout?')) {
      localStorage.removeItem('elderlyLoggedIn');
      localStorage.removeItem('elderly_id');
      localStorage.removeItem('elderly_name');
      localStorage.removeItem('elderly_phone');
      localStorage.removeItem('guardian_name');
      localStorage.removeItem('guardian_phone');
      localStorage.removeItem('elderly_remember_me');
      navigate('/login');
    }
  };

  const handleEmergencyContactsSubmit = async (e) => {
    e.preventDefault();
    const elderlyId = localStorage.getItem('elderly_id') || 'default_senior';
    
    console.log('[ElderlyProfile] Saving emergency contacts for elderlyId:', elderlyId);
    console.log('[ElderlyProfile] Emergency contacts data:', emergencyContacts);
    
    try {
      const responseData = await updateElderlyContacts(elderlyId, {
        hospitalPhone: emergencyContacts.hospitalPhone,
        ambulancePhone: emergencyContacts.ambulancePhone,
        hospitalName: emergencyContacts.hospitalName,
        secondaryGuardianName: emergencyContacts.secondaryGuardianName,
        secondaryGuardianPhone: emergencyContacts.secondaryGuardianPhone,
        neighborName: emergencyContacts.neighborName,
        neighborPhone: emergencyContacts.neighborPhone
      });
      console.log('[ElderlyProfile] Save response:', responseData);
      
      if (responseData && (responseData.status === 'success' || responseData.success)) {
        alert('Emergency contacts saved successfully!');
        setShowEmergencyForm(false);
      } else {
        alert(`Failed to save emergency contacts: ${responseData.message || responseData.error || 'Unknown error'}`);
      }
    } catch (err) {
      console.error('[ElderlyProfile] Error saving emergency contacts:', err);
      alert(`Error saving emergency contacts: ${err.message}`);
    }
  };

  const handleHealthSubmit = async (e) => {
    e.preventDefault();
    const elderlyId = localStorage.getItem('elderly_id') || 'default_senior';
    
    try {
      const pAge = healthData.age ? parseInt(healthData.age, 10) : null;
      const pWeight = healthData.weight ? parseFloat(healthData.weight) : null;
      const pHeight = healthData.height ? parseFloat(healthData.height) : null;
      const pFallCount = healthData.fallCount ? parseInt(healthData.fallCount, 10) : null;
      const pLastFallDays = healthData.lastFallDays ? parseInt(healthData.lastFallDays, 10) : null;

      const payload = {
        age: isNaN(pAge) ? null : pAge,
        weight: isNaN(pWeight) ? null : pWeight,
        height: isNaN(pHeight) ? null : pHeight,
        conditions: Array.isArray(healthData.conditions) ? healthData.conditions.join(', ') : (healthData.conditions || ''),
        mobility: healthData.mobility || '',
        fallCount: isNaN(pFallCount) ? null : pFallCount,
        lastFallDays: isNaN(pLastFallDays) ? null : pLastFallDays,
        medications: healthData.medications || ''
      };

      const resData = await updateHealthHistory(elderlyId, payload);

      if (resData && (resData.status === 'success' || resData.success || resData.data)) {
        alert('Health history saved successfully to backend & database!');
        setShowHealthForm(false);
      } else {
        alert(`Failed to save health history: ${resData.message || resData.error || 'Unknown error'}`);
      }
    } catch (err) {
      console.error('Error saving health history:', err);
      alert(`Error saving health history: ${err.message}`);
    }
  };

  const toggleCondition = (condition) => {
    setHealthData(prev => ({
      ...prev,
      conditions: prev.conditions.includes(condition)
        ? prev.conditions.filter(c => c !== condition)
        : [...prev.conditions, condition]
    }));
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
          👤 {t('profile_title')}
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
            {t('active_member')}
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
            👤 {t('senior_info')}
          </h3>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '10px 14px', backgroundColor: '#f8fafc', borderRadius: '12px' }}>
              <span style={{ fontSize: '13px', color: '#64748b', fontWeight: '600' }}>{t('full_name')}</span>
              <span style={{ fontSize: '14px', color: '#0f172a', fontWeight: '700' }}>{elderlyName}</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '10px 14px', backgroundColor: '#f8fafc', borderRadius: '12px' }}>
              <span style={{ fontSize: '13px', color: '#64748b', fontWeight: '600' }}>{t('phone_number')}</span>
              <span style={{ fontSize: '14px', color: '#2563eb', fontWeight: '700' }}>{elderlyPhone || '—'}</span>
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
          marginBottom: '16px',
          textAlign: 'left'
        }}>
          <h3 style={{ margin: '0 0 14px 0', fontSize: '15px', fontWeight: '800', color: '#1e293b', textTransform: 'uppercase', letterSpacing: '0.5px' }}>
            🛡️ {t('linked_guardian')}
          </h3>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '10px 14px', backgroundColor: '#f0fdf4', borderRadius: '12px', border: '1px solid #dcfce7' }}>
              <span style={{ fontSize: '13px', color: '#166534', fontWeight: '600' }}>{t('guardian_name_label')}</span>
              <span style={{ fontSize: '14px', color: '#14532d', fontWeight: '700' }}>{guardianName || '—'}</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '10px 14px', backgroundColor: '#f0fdf4', borderRadius: '12px', border: '1px solid #dcfce7' }}>
              <span style={{ fontSize: '13px', color: '#166534', fontWeight: '600' }}>{t('guardian_phone_label')}</span>
              {guardianPhone ? (
                <a href={`tel:${guardianPhone}`} style={{ fontSize: '14px', color: '#16a34a', fontWeight: '700', textDecoration: 'none' }}>
                  📞 {guardianPhone}
                </a>
              ) : (
                <span style={{ fontSize: '14px', color: '#14532d', fontWeight: '700' }}>—</span>
              )}
            </div>
          </div>
        </div>

        {/* Emergency Contacts Card */}
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
            Emergency Contacts
          </h3>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '10px 14px', backgroundColor: '#fef2f2', borderRadius: '12px', border: '1px solid #fecaca' }}>
              <span style={{ fontSize: '13px', color: '#991b1b', fontWeight: '600' }}>Hospital</span>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <span style={{ fontSize: '14px', color: '#7f1d1d', fontWeight: '700' }}>{emergencyContacts.hospitalPhone || 'Not set'}</span>
                {emergencyContacts.hospitalPhone && (
                  <a
                    href={`tel:${emergencyContacts.hospitalPhone}`}
                    style={{
                      backgroundColor: '#dc2626',
                      color: 'white',
                      border: 'none',
                      borderRadius: '8px',
                      padding: '6px 12px',
                      fontSize: '12px',
                      fontWeight: '700',
                      cursor: 'pointer',
                      textDecoration: 'none',
                      display: 'inline-flex',
                      alignItems: 'center',
                      gap: '4px'
                    }}
                  >
                    Call
                  </a>
                )}
              </div>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '10px 14px', backgroundColor: '#fef2f2', borderRadius: '12px', border: '1px solid #fecaca' }}>
              <span style={{ fontSize: '13px', color: '#991b1b', fontWeight: '600' }}>Ambulance</span>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <span style={{ fontSize: '14px', color: '#7f1d1d', fontWeight: '700' }}>{emergencyContacts.ambulancePhone || 'Not set'}</span>
                {emergencyContacts.ambulancePhone && (
                  <a
                    href={`tel:${emergencyContacts.ambulancePhone}`}
                    style={{
                      backgroundColor: '#dc2626',
                      color: 'white',
                      border: 'none',
                      borderRadius: '8px',
                      padding: '6px 12px',
                      fontSize: '12px',
                      fontWeight: '700',
                      cursor: 'pointer',
                      textDecoration: 'none',
                      display: 'inline-flex',
                      alignItems: 'center',
                      gap: '4px'
                    }}
                  >
                    Call
                  </a>
                )}
              </div>
            </div>
            
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '10px 14px', backgroundColor: '#fef2f2', borderRadius: '12px', border: '1px solid #fecaca' }}>
              <span style={{ fontSize: '13px', color: '#991b1b', fontWeight: '600' }}>Secondary Guardian</span>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <span style={{ fontSize: '14px', color: '#7f1d1d', fontWeight: '700' }}>
                  {emergencyContacts.secondaryGuardianName ? `${emergencyContacts.secondaryGuardianName} (${emergencyContacts.secondaryGuardianPhone})` : 'Not set'}
                </span>
                {emergencyContacts.secondaryGuardianPhone && (
                  <a
                    href={`tel:${emergencyContacts.secondaryGuardianPhone}`}
                    style={{
                      backgroundColor: '#dc2626',
                      color: 'white',
                      border: 'none',
                      borderRadius: '8px',
                      padding: '6px 12px',
                      fontSize: '12px',
                      fontWeight: '700',
                      cursor: 'pointer',
                      textDecoration: 'none',
                      display: 'inline-flex',
                      alignItems: 'center',
                      gap: '4px'
                    }}
                  >
                    Call
                  </a>
                )}
              </div>
            </div>
            
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '10px 14px', backgroundColor: '#fef2f2', borderRadius: '12px', border: '1px solid #fecaca' }}>
              <span style={{ fontSize: '13px', color: '#991b1b', fontWeight: '600' }}>Neighbor</span>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <span style={{ fontSize: '14px', color: '#7f1d1d', fontWeight: '700' }}>
                  {emergencyContacts.neighborName ? `${emergencyContacts.neighborName} (${emergencyContacts.neighborPhone})` : 'Not set'}
                </span>
                {emergencyContacts.neighborPhone && (
                  <a
                    href={`tel:${emergencyContacts.neighborPhone}`}
                    style={{
                      backgroundColor: '#dc2626',
                      color: 'white',
                      border: 'none',
                      borderRadius: '8px',
                      padding: '6px 12px',
                      fontSize: '12px',
                      fontWeight: '700',
                      cursor: 'pointer',
                      textDecoration: 'none',
                      display: 'inline-flex',
                      alignItems: 'center',
                      gap: '4px'
                    }}
                  >
                    Call
                  </a>
                )}
              </div>
            </div>
          </div>
          <button
            onClick={() => setShowEmergencyForm(true)}
            style={{
              width: '100%',
              backgroundColor: '#ef4444',
              color: '#ffffff',
              border: 'none',
              padding: '12px',
              borderRadius: '12px',
              fontSize: '14px',
              fontWeight: '700',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '8px',
              marginTop: '12px'
            }}
          >
            Update Emergency Contacts
          </button>
        </div>

        {/* Health History Card */}
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
            🏥 Health History
          </h3>
          <p style={{ fontSize: '13px', color: '#64748b', marginBottom: '12px' }}>
            Help us personalize your fall detection by providing your health information
          </p>
          <button
            onClick={() => setShowHealthForm(true)}
            style={{
              width: '100%',
              backgroundColor: '#3b82f6',
              color: '#ffffff',
              border: 'none',
              padding: '12px',
              borderRadius: '12px',
              fontSize: '14px',
              fontWeight: '700',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '8px'
            }}
          >
            📝 Update Health History
          </button>
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
            <path d="M9 21H5a2 2 0 01-2-2V5a2 2 0 012-2h4M16 17l5-5-5-5M21 12H9" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
          </svg>
          {t('logout') || 'Logout Account'}
        </button>

      </div>

      {/* Emergency Contacts Modal */}
      {showEmergencyForm && (
        <div style={{
          position: 'fixed',
          top: 0, left: 0, right: 0, bottom: 0,
          backgroundColor: 'rgba(0, 0, 0, 0.65)',
          backdropFilter: 'blur(6px)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 9999,
          padding: '20px'
        }}>
          <div style={{
            backgroundColor: '#ffffff',
            borderRadius: '24px',
            padding: '24px',
            maxWidth: '500px',
            width: '100%',
            maxHeight: '90vh',
            overflowY: 'auto',
            boxShadow: '0 25px 50px -12px rgba(0,0,0,0.3)',
            boxSizing: 'border-box'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px', borderBottom: '1px solid #e2e8f0', paddingBottom: '12px' }}>
              <div>
                <h2 style={{ fontSize: '18px', fontWeight: '800', color: '#0f172a', margin: 0 }}>
                  Emergency Contacts
                </h2>
                <div style={{ fontSize: '12px', color: '#64748b', marginTop: '2px' }}>
                  Set hospital and ambulance phone numbers
                </div>
              </div>
              <button
                onClick={() => setShowEmergencyForm(false)}
                style={{
                  background: '#f1f5f9', border: 'none', borderRadius: '50%',
                  width: '32px', height: '32px', fontSize: '16px', cursor: 'pointer',
                  display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#64748b'
                }}
              >
                ✕
              </button>
            </div>

            <form onSubmit={handleEmergencyContactsSubmit}>
              <div style={{ marginBottom: '16px' }}>
                <label style={{ display: 'block', fontSize: '13px', fontWeight: '700', color: '#334155', marginBottom: '6px' }}>
                  Hospital Name
                </label>
                <input
                  type="text"
                  value={emergencyContacts.hospitalName}
                  onChange={(e) => setEmergencyContacts({...emergencyContacts, hospitalName: e.target.value})}
                  placeholder="Hospital name"
                  style={{ width: '100%', padding: '10px 12px', border: '1px solid #d1d5db', borderRadius: '8px', fontSize: '14px' }}
                />
              </div>

              <div style={{ marginBottom: '16px' }}>
                <label style={{ display: 'block', fontSize: '13px', fontWeight: '700', color: '#334155', marginBottom: '6px' }}>
                  Hospital Phone Number *
                </label>
                <input
                  type="tel"
                  value={emergencyContacts.hospitalPhone}
                  onChange={(e) => setEmergencyContacts({...emergencyContacts, hospitalPhone: e.target.value})}
                  placeholder="Hospital phone number"
                  required
                  style={{ width: '100%', padding: '10px 12px', border: '1px solid #d1d5db', borderRadius: '8px', fontSize: '14px' }}
                />
              </div>

              <div style={{ marginBottom: '20px' }}>
                <label style={{ display: 'block', fontSize: '13px', fontWeight: '700', color: '#334155', marginBottom: '6px' }}>
                  Ambulance Phone Number *
                </label>
                <input
                  type="tel"
                  value={emergencyContacts.ambulancePhone}
                  onChange={(e) => setEmergencyContacts({...emergencyContacts, ambulancePhone: e.target.value})}
                  placeholder="Ambulance phone number"
                  required
                  style={{ width: '100%', padding: '10px 12px', border: '1px solid #d1d5db', borderRadius: '8px', fontSize: '14px' }}
                />
              </div>

              <div style={{ marginBottom: '16px' }}>
                <label style={{ display: 'block', fontSize: '13px', fontWeight: '700', color: '#334155', marginBottom: '6px' }}>
                  Neighbor Name
                </label>
                <input
                  type="text"
                  value={emergencyContacts.neighborName}
                  onChange={(e) => setEmergencyContacts({...emergencyContacts, neighborName: e.target.value})}
                  placeholder="Neighbor name"
                  style={{ width: '100%', padding: '10px 12px', border: '1px solid #d1d5db', borderRadius: '8px', fontSize: '14px' }}
                />
              </div>

              <div style={{ marginBottom: '16px' }}>
                <label style={{ display: 'block', fontSize: '13px', fontWeight: '700', color: '#334155', marginBottom: '6px' }}>
                  Neighbor Phone
                </label>
                <input
                  type="tel"
                  value={emergencyContacts.neighborPhone}
                  onChange={(e) => setEmergencyContacts({...emergencyContacts, neighborPhone: e.target.value})}
                  placeholder="Neighbor phone number"
                  style={{ width: '100%', padding: '10px 12px', border: '1px solid #d1d5db', borderRadius: '8px', fontSize: '14px' }}
                />
              </div>

              <div style={{ marginBottom: '16px' }}>
                <label style={{ display: 'block', fontSize: '13px', fontWeight: '700', color: '#334155', marginBottom: '6px' }}>
                  Secondary Guardian Name
                </label>
                <input
                  type="text"
                  value={emergencyContacts.secondaryGuardianName}
                  onChange={(e) => setEmergencyContacts({...emergencyContacts, secondaryGuardianName: e.target.value})}
                  placeholder="Secondary guardian name"
                  style={{ width: '100%', padding: '10px 12px', border: '1px solid #d1d5db', borderRadius: '8px', fontSize: '14px' }}
                />
              </div>

              <div style={{ marginBottom: '20px' }}>
                <label style={{ display: 'block', fontSize: '13px', fontWeight: '700', color: '#334155', marginBottom: '6px' }}>
                  Secondary Guardian Phone
                </label>
                <input
                  type="tel"
                  value={emergencyContacts.secondaryGuardianPhone}
                  onChange={(e) => setEmergencyContacts({...emergencyContacts, secondaryGuardianPhone: e.target.value})}
                  placeholder="Secondary guardian phone number"
                  style={{ width: '100%', padding: '10px 12px', border: '1px solid #d1d5db', borderRadius: '8px', fontSize: '14px' }}
                />
              </div>

              <button
                type="submit"
                style={{
                  width: '100%',
                  backgroundColor: '#ef4444',
                  color: '#ffffff',
                  border: 'none',
                  padding: '14px',
                  borderRadius: '12px',
                  fontSize: '15px',
                  fontWeight: '700',
                  cursor: 'pointer'
                }}
              >
                Save Emergency Contacts
              </button>
            </form>
          </div>
        </div>
      )}

      {/* Health History Modal */}
      {showHealthForm && (
        <div style={{
          position: 'fixed',
          top: 0, left: 0, right: 0, bottom: 0,
          backgroundColor: 'rgba(0, 0, 0, 0.65)',
          backdropFilter: 'blur(6px)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 9999,
          padding: '20px'
        }}>
          <div style={{
            backgroundColor: '#ffffff',
            borderRadius: '24px',
            padding: '24px',
            maxWidth: '500px',
            width: '100%',
            maxHeight: '90vh',
            overflowY: 'auto',
            boxShadow: '0 25px 50px -12px rgba(0,0,0,0.3)',
            boxSizing: 'border-box'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px', borderBottom: '1px solid #e2e8f0', paddingBottom: '12px' }}>
              <div>
                <h2 style={{ fontSize: '18px', fontWeight: '800', color: '#0f172a', margin: 0, display: 'flex', alignItems: 'center', gap: '8px' }}>
                  🏥 Health History
                </h2>
                <div style={{ fontSize: '12px', color: '#64748b', marginTop: '2px' }}>
                  Personalize your fall detection model
                </div>
              </div>
              <button
                onClick={() => setShowHealthForm(false)}
                style={{
                  background: '#f1f5f9', border: 'none', borderRadius: '50%',
                  width: '32px', height: '32px', fontSize: '16px', cursor: 'pointer',
                  display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#64748b'
                }}
              >
                ✕
              </button>
            </div>

            <form onSubmit={handleHealthSubmit}>
              {/* Basic Info */}
              <div style={{ marginBottom: '16px' }}>
                <label style={{ display: 'block', fontSize: '13px', fontWeight: '700', color: '#334155', marginBottom: '6px' }}>
                  Age *
                </label>
                <input
                  type="number"
                  value={healthData.age}
                  onChange={(e) => setHealthData({...healthData, age: e.target.value})}
                  required
                  min="50"
                  max="120"
                  style={{ width: '100%', padding: '10px 12px', border: '1px solid #d1d5db', borderRadius: '8px', fontSize: '14px' }}
                />
              </div>

              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginBottom: '16px' }}>
                <div>
                  <label style={{ display: 'block', fontSize: '13px', fontWeight: '700', color: '#334155', marginBottom: '6px' }}>
                    Weight (kg) *
                  </label>
                  <input
                    type="number"
                    value={healthData.weight}
                    onChange={(e) => setHealthData({...healthData, weight: e.target.value})}
                    required
                    min="30"
                    max="200"
                    style={{ width: '100%', padding: '10px 12px', border: '1px solid #d1d5db', borderRadius: '8px', fontSize: '14px' }}
                  />
                </div>
                <div>
                  <label style={{ display: 'block', fontSize: '13px', fontWeight: '700', color: '#334155', marginBottom: '6px' }}>
                    Height (cm) *
                  </label>
                  <input
                    type="number"
                    value={healthData.height}
                    onChange={(e) => setHealthData({...healthData, height: e.target.value})}
                    required
                    min="100"
                    max="250"
                    style={{ width: '100%', padding: '10px 12px', border: '1px solid #d1d5db', borderRadius: '8px', fontSize: '14px' }}
                  />
                </div>
              </div>

              {/* Health Conditions */}
              <div style={{ marginBottom: '16px' }}>
                <label style={{ display: 'block', fontSize: '13px', fontWeight: '700', color: '#334155', marginBottom: '8px' }}>
                  Health Conditions (select all that apply)
                </label>
                <div style={{ display: 'flex', flexWrap: 'wrap', gap: '8px' }}>
                  {['Hypertension', 'Diabetes', 'Heart Disease', 'Arthritis', 'Osteoporosis', 'Stroke', 'Parkinson\'s', 'Dementia'].map(condition => (
                    <button
                      key={condition}
                      type="button"
                      onClick={() => toggleCondition(condition)}
                      style={{
                        padding: '8px 14px',
                        borderRadius: '20px',
                        fontSize: '12px',
                        fontWeight: '600',
                        border: healthData.conditions.includes(condition) ? '2px solid #3b82f6' : '1px solid #d1d5db',
                        backgroundColor: healthData.conditions.includes(condition) ? '#eff6ff' : '#ffffff',
                        color: healthData.conditions.includes(condition) ? '#1d4ed8' : '#64748b',
                        cursor: 'pointer'
                      }}
                    >
                      {healthData.conditions.includes(condition) ? '✓ ' : ''}{condition}
                    </button>
                  ))}
                </div>
              </div>

              {/* Mobility Level */}
              <div style={{ marginBottom: '16px' }}>
                <label style={{ display: 'block', fontSize: '13px', fontWeight: '700', color: '#334155', marginBottom: '6px' }}>
                  Mobility Level *
                </label>
                <select
                  value={healthData.mobility}
                  onChange={(e) => setHealthData({...healthData, mobility: e.target.value})}
                  required
                  style={{ width: '100%', padding: '10px 12px', border: '1px solid #d1d5db', borderRadius: '8px', fontSize: '14px' }}
                >
                  <option value="independent">Independent (no assistance needed)</option>
                  <option value="cane">Uses cane</option>
                  <option value="walker">Uses walker</option>
                  <option value="wheelchair">Wheelchair</option>
                  <option value="bedridden">Bedridden</option>
                </select>
              </div>

              {/* Fall History */}
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginBottom: '16px' }}>
                <div>
                  <label style={{ display: 'block', fontSize: '13px', fontWeight: '700', color: '#334155', marginBottom: '6px' }}>
                    Falls in past year
                  </label>
                  <input
                    type="number"
                    value={healthData.fallCount}
                    onChange={(e) => setHealthData({...healthData, fallCount: e.target.value})}
                    min="0"
                    max="20"
                    style={{ width: '100%', padding: '10px 12px', border: '1px solid #d1d5db', borderRadius: '8px', fontSize: '14px' }}
                  />
                </div>
                <div>
                  <label style={{ display: 'block', fontSize: '13px', fontWeight: '700', color: '#334155', marginBottom: '6px' }}>
                    Days since last fall
                  </label>
                  <input
                    type="number"
                    value={healthData.lastFallDays}
                    onChange={(e) => setHealthData({...healthData, lastFallDays: e.target.value})}
                    min="0"
                    max="365"
                    style={{ width: '100%', padding: '10px 12px', border: '1px solid #d1d5db', borderRadius: '8px', fontSize: '14px' }}
                  />
                </div>
              </div>

              {/* Medications */}
              <div style={{ marginBottom: '20px' }}>
                <label style={{ display: 'block', fontSize: '13px', fontWeight: '700', color: '#334155', marginBottom: '6px' }}>
                  Current Medications (optional)
                </label>
                <textarea
                  value={healthData.medications}
                  onChange={(e) => setHealthData({...healthData, medications: e.target.value})}
                  placeholder="List any medications you're currently taking..."
                  rows="3"
                  style={{ width: '100%', padding: '10px 12px', border: '1px solid #d1d5db', borderRadius: '8px', fontSize: '14px', resize: 'vertical' }}
                />
              </div>

              <button
                type="submit"
                style={{
                  width: '100%',
                  backgroundColor: '#3b82f6',
                  color: '#ffffff',
                  border: 'none',
                  padding: '14px',
                  borderRadius: '12px',
                  fontSize: '15px',
                  fontWeight: '700',
                  cursor: 'pointer'
                }}
              >
                Save Health History
              </button>
            </form>
          </div>
        </div>
      )}

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
