import React, { useState, useEffect, useRef } from 'react';
import { useNavigate } from 'react-router-dom';
import { getMedicines, confirmMedicineTaken } from '../services/api';

const ElderlyHome = () => {
  const navigate = useNavigate();
  const [medicineCount, setMedicineCount] = useState('Loading medicines...');
  const [sosPressed, setSosPressed] = useState(false);
  const [showReminders, setShowReminders] = useState(false);
  const [medicines, setMedicines] = useState([]);
  const sosTimerRef = useRef(null);
  const sosButtonRef = useRef(null);

  useEffect(() => {
    // Check if user is logged in
    const loggedIn = localStorage.getItem('elderlyLoggedIn');
    if (!loggedIn || loggedIn !== 'true') {
      navigate('/login');
      return;
    }

    // Set elderly ID if not already set
    if (!localStorage.getItem('elderly_id') || localStorage.getItem('elderly_id').includes('elderly_')) {
      localStorage.setItem('elderly_id', 'isha_amit');
    }

    // Load medicines
    loadElderlyMedicines();

    // Start real-time updates
    const interval = setInterval(loadElderlyMedicines, 15000);

    return () => clearInterval(interval);
  }, [navigate]);

  const loadElderlyMedicines = async () => {
    try {
      let elderlyId = localStorage.getItem('elderly_id') || 'isha_amit';
      if (elderlyId.includes('elderly_')) {
        elderlyId = 'isha_amit';
        localStorage.setItem('elderly_id', 'isha_amit');
      }

      const response = await getMedicines(elderlyId);
      const medicinesData = response.medicines || [];
      setMedicines(medicinesData);
      updateMedicineCountDisplay(medicinesData);
    } catch (error) {
      console.error('Error loading medicines:', error);
      setMedicineCount('Failed to load');
    }
  };

  const updateMedicineCountDisplay = (medicines) => {
    if (medicines.length === 0) {
      setMedicineCount('No medicines scheduled');
      return;
    }

    const today = new Date().toISOString().split('T')[0];
    let pendingCount = 0;
    let missedCount = 0;
    let nextMedicineTime = null;

    medicines.forEach(medicine => {
      const times = Array.isArray(medicine.times) ? medicine.times : [medicine.times];
      times.forEach(time => {
        const status = getMedicineStatus(medicine.id, time);
        if (status === 'pending') {
          pendingCount++;
          if (!nextMedicineTime || time < nextMedicineTime) {
            nextMedicineTime = time;
          }
        } else if (status === 'missed') {
          missedCount++;
        }
      });
    });

    if (medicines.length === 0) {
      setMedicineCount('No medicines today');
    } else if (missedCount > 0) {
      setMedicineCount(`${missedCount} missed • Next at ${nextMedicineTime}`);
    } else if (pendingCount > 0) {
      setMedicineCount(`${pendingCount} pending • Next at ${nextMedicineTime}`);
    } else {
      setMedicineCount('All medicines taken');
    }
  };

  const getMedicineStatus = (medicineId, time) => {
    const key = `medicine_${medicineId}_${time}`;
    return localStorage.getItem(key) || 'pending';
  };

  const handleSosStart = (e) => {
    e.preventDefault();
    setSosPressed(true);
    sosTimerRef.current = setTimeout(() => {
      alert('SOS alert sent!');
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

  const handleSosLeave = () => {
    if (sosPressed) {
      clearTimeout(sosTimerRef.current);
      setSosPressed(false);
    }
  };

  const logout = () => {
    if (confirm('Are you sure you want to logout?')) {
      localStorage.setItem('elderlyLoggedIn', 'false');
      localStorage.clear();
      navigate('/login');
    }
  };

  const goBack = () => {
    if (window.history.length > 1) {
      window.history.back();
    } else {
      navigate('/login');
    }
  };

  const openReminders = () => {
    setShowReminders(true);
  };

  const closeReminders = () => {
    setShowReminders(false);
  };

  const openAlerts = () => {
    alert('No new alerts');
  };

  const openAssistantChat = () => {
    window.open('test.html', '_blank');
  };

  const openHealth = () => {
    window.location.href = 'health.html';
  };

  const markMedicineTaken = async (medicineId, time) => {
    try {
      const currentTime = new Date().toTimeString().slice(0, 5);
      localStorage.setItem(`medicine_${medicineId}_${currentTime}`, 'taken');

      const data = await confirmMedicineTaken({
        medicineId,
        elderlyId: localStorage.getItem('elderly_id') || 'isha_amit',
        timeTaken: currentTime,
        taken: true,
      });

      if (data.status === 'success' || data.message?.includes('marked')) {
        loadElderlyMedicines();
      }
    } catch (error) {
      console.error('Error confirming medicine:', error);
    }
  };

  const renderRemindersModal = () => {
    if (!showReminders) return null;

    const medicinesByTime = {};
    medicines.forEach(medicine => {
      const times = Array.isArray(medicine.times) ? medicine.times : [medicine.times];
      times.forEach(time => {
        if (!medicinesByTime[time]) {
          medicinesByTime[time] = [];
        }
        medicinesByTime[time].push(medicine);
      });
    });

    const sortedTimes = Object.keys(medicinesByTime).sort();

    return (
      <div style={{
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        background: 'white',
        zIndex: 1000,
        overflowY: 'auto'
      }}>
        <div style={{
          background: '#007AFF',
          color: 'white',
          padding: '16px',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          position: 'sticky',
          top: 0,
          zIndex: 1001
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <button
              onClick={closeReminders}
              style={{
                background: 'rgba(255,255,255,0.3)',
                border: '1px solid rgba(255,255,255,0.5)',
                color: 'white',
                padding: '8px 12px',
                borderRadius: '6px',
                cursor: 'pointer',
                fontSize: '16px',
                fontWeight: 'bold'
              }}
            >
              ← Back
            </button>
            <h2 style={{ margin: 0, fontSize: '18px' }}>💊 Medicine Reminders</h2>
          </div>
          <button
            onClick={closeReminders}
            style={{
              background: 'none',
              border: 'none',
              color: 'white',
              fontSize: '24px',
              cursor: 'pointer',
              padding: 0,
              width: '30px',
              height: '30px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center'
            }}
          >
            ×
          </button>
        </div>

        <div style={{ padding: '20px' }}>
          <h3 style={{ marginBottom: '20px', color: '#333' }}>💊 Today's Medicines</h3>
          <div style={{ fontSize: '12px', color: '#666', marginBottom: '15px' }}>
            Last updated: {new Date().toLocaleTimeString()}
          </div>

          {medicines.length === 0 ? (
            <div style={{ textAlign: 'center', padding: '40px', color: '#666' }}>
              <div style={{ fontSize: '48px', marginBottom: '16px' }}>💊</div>
              <h3>No medicines scheduled</h3>
              <p>Your guardian will add medicines here</p>
            </div>
          ) : (
            sortedTimes.map(time => (
              <div key={time} style={{ marginBottom: '25px' }}>
                <h4 style={{ color: '#007AFF', marginBottom: '12px', fontSize: '16px' }}>
                  🕐 {time}
                </h4>
                {medicinesByTime[time].map(medicine => {
                  const status = getMedicineStatus(medicine.id, time);
                  const statusColor = status === 'taken' ? '#28a745' : status === 'missed' ? '#dc3545' : '#ffc107';
                  const statusText = status === 'taken' ? '✅ Taken' : status === 'missed' ? '❌ Missed' : '⏳ Pending';

                  return (
                    <div
                      key={medicine.id}
                      style={{
                        background: '#f8f9fa',
                        borderRadius: '12px',
                        padding: '16px',
                        marginBottom: '12px',
                        borderLeft: `4px solid ${statusColor}`
                      }}
                    >
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                        <div>
                          <h4 style={{ margin: 0, color: '#333' }}>{medicine.medicine_name || medicine.name}</h4>
                          <p style={{ margin: '4px 0', color: '#666', fontSize: '14px' }}>
                            {medicine.dosage || 'As prescribed'}
                          </p>
                          <p style={{ margin: '4px 0', color: statusColor, fontWeight: 'bold' }}>
                            🕐 {time} • {statusText}
                          </p>
                        </div>
                        <button
                          onClick={() => markMedicineTaken(medicine.id, time)}
                          disabled={status === 'taken'}
                          style={{
                            background: status === 'taken' ? '#6c757d' : '#28a745',
                            color: 'white',
                            border: 'none',
                            padding: '8px 16px',
                            borderRadius: '6px',
                            cursor: status === 'taken' ? 'not-allowed' : 'pointer'
                          }}
                        >
                          {status === 'taken' ? '✅ Already Taken' : '✅ Taken'}
                        </button>
                      </div>
                    </div>
                  );
                })}
              </div>
            ))
          )}
        </div>
      </div>
    );
  };

  return (
    <div className="container">
      {/* Header */}
      <header className="header">
        <div className="status-badge success">
          <svg width="20" height="20" viewBox="0 0 20 20" fill="none">
            <circle cx="10" cy="10" r="10" fill="currentColor"/>
            <path d="M6 10l3 3 5-6" stroke="white" strokeWidth="2" fill="none"/>
          </svg>
          <span>belt Connected</span>
        </div>
        
        <button
          className="back-btn"
          onClick={goBack}
          title="Go Back"
          style={{
            background: 'rgba(255, 255, 255, 0.1)',
            border: '1px solid rgba(255, 255, 255, 0.2)',
            color: 'white',
            padding: '8px',
            borderRadius: '8px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            transition: 'all 0.2s ease',
            marginRight: '10px'
          }}
        >
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
            <path d="M19 12H5M12 19l-7-7 7-7" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
          </svg>
        </button>

        <div className="language-selector">
          <select id="language">
            <option value="en">English</option>
            <option value="hi">हिन्दी</option>
            <option value="mr">मराठी</option>
          </select>
        </div>

        <button className="logout-btn" onClick={logout} title="Logout">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
            <path d="M9 21H5a2 2 0 01-2 2v1a2 2 0 01-2 2h4a2 2 0 01-2 2v1a2 2 0 01-2 2h4M16 17l-4 4-4 4v1a2 2 0 01-2 2h4a2 2 0 01-2 2v-1a2 2 0 01-2 2h-4a2 2 0 01-2 2v-1a2 2 0 01-2 2h4M7 14l5 5 5 5s-5 5h-1.71l-.29-.29a1 1 0 01-1.42 0l-1.29-1.29A1 1 0 017 7l3.59 3.59A2 2 0 0112 9l-3.59-3.59A2 2 0 0111 7z" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
          </svg>
        </button>
      </header>

      {/* SOS Button */}
      <div className="sos-section">
        <button
          ref={sosButtonRef}
          className="sos-button"
          onMouseDown={handleSosStart}
          onMouseUp={handleSosEnd}
          onMouseLeave={handleSosLeave}
          onTouchStart={handleSosStart}
          onTouchEnd={handleSosEnd}
        >
          <div className="sos-icon">
            <svg width="60" height="60" viewBox="0 0 60 60" fill="none">
              <path d="M30 10v10M30 30v10M20 20l7 7M40 20l-7 7M20 40l7-7M40 40l-7-7" stroke="white" strokeWidth="4" strokeLinecap="round"/>
            </svg>
          </div>
          <div className="sos-text">SOS</div>
          <div className="sos-subtext">PRESS FOR HELP</div>
        </button>
        <p className="sos-instruction">Press and hold for 3 seconds</p>
      </div>

      {/* Menu Items */}
      <div className="menu-list">
        <div className="menu-item" onClick={openReminders}>
          <div className="menu-icon yellow-bg">
            <svg width="32" height="32" viewBox="0 0 32 32" fill="none">
              <rect x="8" y="10" width="16" height="16" rx="2" stroke="#D97706" strokeWidth="2" fill="none"/>
              <path d="M16 14v4M14 18h4" stroke="#D97706" strokeWidth="2" strokeLinecap="round"/>
            </svg>
          </div>
          <div className="menu-content">
            <h3 className="menu-title">Reminders</h3>
            <p className="menu-subtitle">{medicineCount}</p>
          </div>
          <svg className="menu-arrow" width="24" height="24" viewBox="0 0 24 24" fill="none">
            <path d="M9 6l6 6-6 6" stroke="#9CA3AF" strokeWidth="2" strokeLinecap="round"/>
          </svg>
        </div>

        <div className="menu-item" onClick={openAlerts}>
          <div className="menu-icon pink-bg">
            <svg width="32" height="32" viewBox="0 0 32 32" fill="none">
              <path d="M16 8c-3 0-5 2-5 5v3l-2 4h14l-2-4v-3c0-3-2-5-5-5zM14 20v1c0 1.1.9 2 2 2s2-.9 2-2v-1" stroke="#DC2626" strokeWidth="2" strokeLinecap="round"/>
            </svg>
          </div>
          <div className="menu-content">
            <h3 className="menu-title">Alerts</h3>
            <p className="menu-subtitle">No new alerts</p>
          </div>
          <svg className="menu-arrow" width="24" height="24" viewBox="0 0 24 24" fill="none">
            <path d="M9 6l6 6-6 6" stroke="#9CA3AF" strokeWidth="2" strokeLinecap="round"/>
          </svg>
        </div>
      </div>

      {/* Bottom Navigation */}
      <nav className="bottom-nav">
        <a href="#" className="nav-item active" onClick={(e) => { e.preventDefault(); }}>
          <svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor">
            <path d="M3 9l9-7 9 7v11a2 2 0 01-2 2H5a2 2 0 01-2-2V9z"/>
          </svg>
          <span>Home</span>
        </a>
        <button className="nav-item assistant-nav-btn" onClick={openAssistantChat} title="Open Assistant">
          <svg width="24" height="24" viewBox="0 0 32 32" fill="currentColor">
            <path d="M16 28c6.627 0 12-5.373 12-12S22.627 4 16 4 4 9.373 4 16c0 2.4.7 4.6 2 6.5L4 28l5.5-2c1.9 1.3 4.1 2 6.5 2z" fill="#3B82F6"/>
          </svg>
          <span>Assistant</span>
        </button>
        <a href="#" className="nav-item" onClick={(e) => { e.preventDefault(); openHealth(); }}>
          <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor">
            <path d="M4 12h16M12 4v16" strokeWidth="2"/>
          </svg>
          <span>Health</span>
        </a>
      </nav>

      {renderRemindersModal()}
    </div>
  );
};

export default ElderlyHome;
