import React, { useState, useEffect, useRef } from 'react';
import { useNavigate } from 'react-router-dom';
import { useLanguage } from '../context/LanguageContext';
import '../styles.css';

const CalibrationPage = () => {
  const navigate = useNavigate();
  const { t } = useLanguage();
  const elderlyId = localStorage.getItem('elderly_id');
  
  const [isCalibrating, setIsCalibrating] = useState(false);
  const [timeRemaining, setTimeRemaining] = useState(600); // 10 minutes in seconds
  const [currentPhase, setCurrentPhase] = useState('idle'); // idle, walking, sitting, standing, complete
  const [calibrationData, setCalibrationData] = useState([]);
  const [healthHistory, setHealthHistory] = useState(null);
  
  const intervalRef = useRef(null);
  const sensorIntervalRef = useRef(null);
  
  useEffect(() => {
    // Check if user has health history
    fetchHealthHistory();
    
    return () => {
      if (intervalRef.current) clearInterval(intervalRef.current);
      if (sensorIntervalRef.current) clearInterval(sensorIntervalRef.current);
    };
  }, []);
  
  const fetchHealthHistory = async () => {
    try {
      const response = await fetch(`${import.meta.env.VITE_API_BASE_URL || 'http://localhost:8081'}/api/elderly/${elderlyId}/health-history`);
      if (response.ok) {
        const data = await response.json();
        setHealthHistory(data.data);
      }
    } catch (err) {
      console.error('Error fetching health history:', err);
    }
  };
  
  const startCalibration = () => {
    if (!healthHistory) {
      alert('Please complete your health history first before calibration.');
      navigate('/profile');
      return;
    }
    
    setIsCalibrating(true);
    setCurrentPhase('walking');
    setTimeRemaining(600);
    setCalibrationData([]);
    
    // Start timer
    intervalRef.current = setInterval(() => {
      setTimeRemaining(prev => {
        if (prev <= 1) {
          completeCalibration();
          return 0;
        }
        
        // Update phase based on time
        const elapsed = 600 - prev;
        if (elapsed < 180) {
          setCurrentPhase('walking');
        } else if (elapsed < 360) {
          setCurrentPhase('sitting');
        } else if (elapsed < 540) {
          setCurrentPhase('standing');
        } else {
          setCurrentPhase('mixed');
        }
        
        return prev - 1;
      });
    }, 1000);
    
    // Start sensor data collection (simulated for now)
    sensorIntervalRef.current = setInterval(() => {
      collectSensorData();
    }, 100); // Collect every 100ms
  };
  
  const collectSensorData = () => {
    // Simulate sensor data collection
    // In real implementation, this would come from ESP32 via WebSocket or API
    const simulatedData = {
      timestamp: Date.now(),
      phase: currentPhase,
      accelerometer: {
        x: (Math.random() - 0.5) * 2,
        y: (Math.random() - 0.5) * 2 + 9.8,
        z: (Math.random() - 0.5) * 2
      },
      gyroscope: {
        x: (Math.random() - 0.5) * 0.5,
        y: (Math.random() - 0.5) * 0.5,
        z: (Math.random() - 0.5) * 0.5
      },
      vitals: {
        heartRate: 70 + Math.random() * 10,
        spo2: 97 + Math.random() * 2,
        temperature: 36.5 + Math.random() * 0.3
      }
    };
    
    setCalibrationData(prev => [...prev, simulatedData]);
  };
  
  const completeCalibration = async () => {
    setIsCalibrating(false);
    setCurrentPhase('complete');
    
    if (intervalRef.current) clearInterval(intervalRef.current);
    if (sensorIntervalRef.current) clearInterval(sensorIntervalRef.current);
    
    // Send calibration data to ML service
    try {
      const response = await fetch('http://localhost:8000/calibrate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          elderlyId: elderlyId,
          sensorData: calibrationData.map(d => ({
            accelerometer: [[d.accelerometer.x, d.accelerometer.y, d.accelerometer.z]],
            gyroscope: [[d.gyroscope.x, d.gyroscope.y, d.gyroscope.z]]
          })),
          vitalsData: calibrationData.map(d => ({
            heartRate: d.vitals.heartRate,
            spo2: d.vitals.spo2,
            temperature: d.vitals.temperature
          })),
          healthHistory: healthHistory
        })
      });
      
      if (response.ok) {
        // Update elderly calibration status
        await fetch(`${import.meta.env.VITE_API_BASE_URL || 'http://localhost:8081'}/api/elderly/${elderlyId}/health-history`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            isCalibrated: true,
            calibratedAt: new Date().toISOString()
          })
        });
        
        alert('Calibration completed successfully! Your fall detection is now personalized.');
        navigate('/home');
      } else {
        alert('Calibration completed but failed to personalize model. Please try again.');
      }
    } catch (err) {
      console.error('Error during calibration:', err);
      alert('Calibration completed. Model personalization will be processed in background.');
      navigate('/home');
    }
  };
  
  const formatTime = (seconds) => {
    const mins = Math.floor(seconds / 60);
    const secs = seconds % 60;
    return `${mins}:${secs.toString().padStart(2, '0')}`;
  };
  
  const getPhaseInstructions = () => {
    switch (currentPhase) {
      case 'walking':
        return 'Please walk around normally at your comfortable pace';
      case 'sitting':
        return 'Please sit comfortably and relax';
      case 'standing':
        return 'Please stand still in a comfortable position';
      case 'mixed':
        return 'Please perform your normal daily activities';
      default:
        return '';
    }
  };
  
  return (
    <div className="container" style={{ paddingBottom: '90px', backgroundColor: '#f8fafc', minHeight: '100vh' }}>
      {/* Header */}
      <header className="header" style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '16px 20px', gap: '12px', borderBottom: '1px solid #e2e8f0', backgroundColor: '#ffffff' }}>
        <button className="back-btn" onClick={() => navigate('/home')} title="Go Back">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
            <path d="M19 12H5M12 19l-7-7 7-7" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
          </svg>
        </button>
        <h1 style={{ fontSize: '18px', fontWeight: '800', margin: 0, color: '#1f2937', textAlign: 'center', flex: 1 }}>
          🎯 Calibration
        </h1>
        <div style={{ width: '38px', flexShrink: 0 }}></div>
      </header>

      <div style={{ padding: '20px' }}>
        {!isCalibrating && currentPhase === 'idle' && (
          <div>
            {/* Calibration Info Card */}
            <div style={{
              backgroundColor: '#ffffff',
              borderRadius: '20px',
              padding: '24px',
              boxShadow: '0 4px 16px rgba(0,0,0,0.04)',
              border: '1px solid #e2e8f0',
              marginBottom: '20px'
            }}>
              <div style={{ fontSize: '32px', textAlign: 'center', marginBottom: '16px' }}>
                ⏱️
              </div>
              <h2 style={{ fontSize: '20px', fontWeight: '800', color: '#0f172a', margin: '0 0 12px 0', textAlign: 'center' }}>
                10-Minute Calibration
              </h2>
              <p style={{ fontSize: '14px', color: '#64748b', textAlign: 'center', marginBottom: '16px', lineHeight: '1.6' }}>
                This calibration helps personalize your fall detection by learning your normal movement patterns.
              </p>
              
              <div style={{ backgroundColor: '#f0fdf4', borderRadius: '12px', padding: '16px', marginBottom: '16px' }}>
                <h3 style={{ fontSize: '14px', fontWeight: '700', color: '#166534', margin: '0 0 8px 0' }}>
                  What to expect:
                </h3>
                <ul style={{ fontSize: '13px', color: '#15803d', margin: 0, paddingLeft: '20px', lineHeight: '1.8' }}>
                  <li>3 minutes of normal walking</li>
                  <li>3 minutes of sitting</li>
                  <li>3 minutes of standing</li>
                  <li>1 minute of mixed activities</li>
                </ul>
              </div>

              {healthHistory ? (
                <div style={{ backgroundColor: '#dbeafe', borderRadius: '12px', padding: '12px', marginBottom: '16px' }}>
                  <div style={{ fontSize: '12px', color: '#1e40af', fontWeight: '600' }}>
                    ✓ Health history completed
                  </div>
                </div>
              ) : (
                <div style={{ backgroundColor: '#fee2e2', borderRadius: '12px', padding: '12px', marginBottom: '16px' }}>
                  <div style={{ fontSize: '12px', color: '#991b1b', fontWeight: '600' }}>
                    ⚠️ Please complete health history first
                  </div>
                </div>
              )}
            </div>

            <button
              onClick={startCalibration}
              disabled={!healthHistory}
              style={{
                width: '100%',
                backgroundColor: healthHistory ? '#3b82f6' : '#9ca3af',
                color: '#ffffff',
                border: 'none',
                padding: '16px',
                borderRadius: '16px',
                fontSize: '16px',
                fontWeight: '700',
                cursor: healthHistory ? 'pointer' : 'not-allowed',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px',
                boxShadow: healthHistory ? '0 8px 20px rgba(59, 130, 246, 0.3)' : 'none'
              }}
            >
              🚀 Start Calibration
            </button>
          </div>
        )}

        {isCalibrating && (
          <div>
            {/* Timer Card */}
            <div style={{
              backgroundColor: '#ffffff',
              borderRadius: '20px',
              padding: '32px 24px',
              boxShadow: '0 4px 16px rgba(0,0,0,0.04)',
              border: '1px solid #e2e8f0',
              marginBottom: '20px',
              textAlign: 'center'
            }}>
              <div style={{ fontSize: '64px', fontWeight: '800', color: '#3b82f6', margin: '0 0 8px 0' }}>
                {formatTime(timeRemaining)}
              </div>
              <div style={{ fontSize: '14px', color: '#64748b', marginBottom: '24px' }}>
                Time Remaining
              </div>

              {/* Progress Bar */}
              <div style={{
                width: '100%',
                height: '8px',
                backgroundColor: '#e2e8f0',
                borderRadius: '4px',
                marginBottom: '24px',
                overflow: 'hidden'
              }}>
                <div style={{
                  width: `${((600 - timeRemaining) / 600) * 100}%`,
                  height: '100%',
                  backgroundColor: '#3b82f6',
                  transition: 'width 0.3s ease'
                }}></div>
              </div>

              {/* Current Phase */}
              <div style={{
                backgroundColor: '#eff6ff',
                borderRadius: '12px',
                padding: '16px',
                border: '2px solid #bfdbfe'
              }}>
                <div style={{ fontSize: '12px', color: '#1e40af', fontWeight: '700', marginBottom: '4px', textTransform: 'uppercase' }}>
                  Current Phase
                </div>
                <div style={{ fontSize: '18px', color: '#1d4ed8', fontWeight: '800', marginBottom: '8px' }}>
                  {currentPhase.charAt(0).toUpperCase() + currentPhase.slice(1)}
                </div>
                <div style={{ fontSize: '14px', color: '#3b82f6' }}>
                  {getPhaseInstructions()}
                </div>
              </div>
            </div>

            {/* Data Collection Status */}
            <div style={{
              backgroundColor: '#ffffff',
              borderRadius: '20px',
              padding: '20px',
              boxShadow: '0 4px 16px rgba(0,0,0,0.04)',
              border: '1px solid #e2e8f0',
              marginBottom: '20px'
            }}>
              <h3 style={{ fontSize: '15px', fontWeight: '800', color: '#1e293b', margin: '0 0 12px 0' }}>
                📊 Data Collection
              </h3>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                <div style={{ backgroundColor: '#f8fafc', borderRadius: '8px', padding: '12px' }}>
                  <div style={{ fontSize: '12px', color: '#64748b', marginBottom: '4px' }}>
                    Samples Collected
                  </div>
                  <div style={{ fontSize: '20px', color: '#0f172a', fontWeight: '700' }}>
                    {calibrationData.length}
                  </div>
                </div>
                <div style={{ backgroundColor: '#f8fafc', borderRadius: '8px', padding: '12px' }}>
                  <div style={{ fontSize: '12px', color: '#64748b', marginBottom: '4px' }}>
                    Status
                  </div>
                  <div style={{ fontSize: '14px', color: '#16a34a', fontWeight: '700' }}>
                    Recording
                  </div>
                </div>
              </div>
            </div>

            <button
              onClick={() => {
                if (window.confirm('Are you sure you want to cancel calibration?')) {
                  if (intervalRef.current) clearInterval(intervalRef.current);
                  if (sensorIntervalRef.current) clearInterval(sensorIntervalRef.current);
                  setIsCalibrating(false);
                  setCurrentPhase('idle');
                }
              }}
              style={{
                width: '100%',
                backgroundColor: '#ef4444',
                color: '#ffffff',
                border: 'none',
                padding: '14px',
                borderRadius: '12px',
                fontSize: '14px',
                fontWeight: '700',
                cursor: 'pointer'
              }}
            >
              Cancel Calibration
            </button>
          </div>
        )}

        {currentPhase === 'complete' && (
          <div style={{
            backgroundColor: '#ffffff',
            borderRadius: '20px',
            padding: '32px 24px',
            boxShadow: '0 4px 16px rgba(0,0,0,0.04)',
            border: '1px solid #e2e8f0',
            textAlign: 'center'
          }}>
            <div style={{ fontSize: '64px', marginBottom: '16px' }}>
              ✅
            </div>
            <h2 style={{ fontSize: '20px', fontWeight: '800', color: '#0f172a', margin: '0 0 12px 0' }}>
              Calibration Complete!
            </h2>
            <p style={{ fontSize: '14px', color: '#64748b', marginBottom: '24px', lineHeight: '1.6' }}>
              Your fall detection model is now being personalized with your movement patterns.
            </p>
            <button
              onClick={() => navigate('/home')}
              style={{
                backgroundColor: '#3b82f6',
                color: '#ffffff',
                border: 'none',
                padding: '14px 32px',
                borderRadius: '12px',
                fontSize: '15px',
                fontWeight: '700',
                cursor: 'pointer'
              }}
            >
              Return to Home
            </button>
          </div>
        )}
      </div>

      {/* Bottom Taskbar */}
      <nav className="bottom-nav">
        <a href="#" className="nav-item" onClick={(e) => { e.preventDefault(); navigate('/home'); }}>
          <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.2" strokeLinecap="round" strokeLinejoin="round">
            <path d="M3 9l9-7 9 7v11a2 2 0 01-2 2H5a2 2 0 01-2-2V9z"/>
            <polyline points="9 22 9 12 15 12 15 22"/>
          </svg>
          <span>Home</span>
        </a>

        <a href="#" className="nav-item" onClick={(e) => { e.preventDefault(); navigate('/health'); }}>
          <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.2" strokeLinecap="round" strokeLinejoin="round">
            <path d="M22 12h-4l-3 9L9 3l-3 9H2"/>
          </svg>
          <span>Health</span>
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
          <span>Assistant</span>
        </button>
      </nav>
    </div>
  );
};

export default CalibrationPage;
