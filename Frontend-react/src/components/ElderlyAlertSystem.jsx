import React, { useState, useEffect, useCallback, useRef } from 'react';
import { useLocation, useNavigate } from 'react-router-dom';
import { useLanguage } from '../context/LanguageContext';
import { getMedicines, confirmMedicine, fetchHardwareData, triggerEmergency, confirmSafe, notifyGuardianFall } from '../services/api';

const ElderlyAlertSystem = ({ children }) => {
  const location = useLocation();
  const navigate = useNavigate();
  const { lang, t, translateDynamic } = useLanguage();

  // Active Elderly Member
  const [elderlyId, setElderlyId] = useState('');
  const [elderlyName, setElderlyName] = useState('');

  // Medicine State
  const [medicines, setMedicines] = useState([]);
  const [activeMedicinePopup, setActiveMedicinePopup] = useState(null); // { med, time }
  const alertedMedicinesRef = useRef(new Set());

  // Sensor Alert State (Fall, Sudden Movement, Prefall)
  const [activeSensorAlert, setActiveSensorAlert] = useState(null); // { type, title, subtitle }
  const [countdown, setCountdown] = useState(10);
  const [alertStatusText, setAlertStatusText] = useState('');
  const alertDismissedRef = useRef(false);

  // Sync elderly user info
  useEffect(() => {
    const checkUser = () => {
      const id = localStorage.getItem('elderly_id') || '';
      const name = localStorage.getItem('elderly_name') || 'Senior Citizen';
      setElderlyId(id);
      setElderlyName(name);
    };
    checkUser();
    const interval = setInterval(checkUser, 2000);
    return () => clearInterval(interval);
  }, []);

  // Request Browser Notifications Permission
  useEffect(() => {
    if ('Notification' in window && Notification.permission === 'default') {
      Notification.requestPermission();
    }
  }, []);

  // Speak Voice Alerts
  const speakVoiceAlert = useCallback((text) => {
    if ('speechSynthesis' in window) {
      window.speechSynthesis.cancel();
      const utterance = new SpeechSynthesisUtterance(text);
      utterance.rate = 0.9;
      window.speechSynthesis.speak(utterance);
    }
  }, []);

  // ----------------------------------------------------
  // 1. MEDICINE REMINDER CHECKER & POPUP
  // ----------------------------------------------------
  const loadMedicines = useCallback(async (id) => {
    if (!id) return;
    try {
      const res = await getMedicines(id);
      let raw = res?.data || res?.medicines || res;
      let list = [];
      if (Array.isArray(raw)) list = raw;
      else if (raw && Array.isArray(raw.data)) list = raw.data;
      else if (raw && Array.isArray(raw.medicines)) list = raw.medicines;
      console.log('💊 [ALERTS SYSTEM] Loaded medicines for elderly:', id, list);
      setMedicines(list);
    } catch (e) {
      console.error('Failed to load medicines for popup checker:', e);
    }
  }, []);

  useEffect(() => {
    if (elderlyId) {
      loadMedicines(elderlyId);
      const interval = setInterval(() => loadMedicines(elderlyId), 5000);
      return () => clearInterval(interval);
    }
  }, [elderlyId, loadMedicines]);

  // Check current time against medicine schedule
  useEffect(() => {
    if (!elderlyId || medicines.length === 0) return;

    const checkSchedule = () => {
      const now = new Date();
      const hours = String(now.getHours()).padStart(2, '0');
      const mins = String(now.getMinutes()).padStart(2, '0');
      const currentTime = `${hours}:${mins}`;

      medicines.forEach((med) => {
        const medName = med.medicine_name || med.name || 'Medicine';
        const medId = med.id || medName;
        const times = Array.isArray(med.times) ? med.times : [med.times || '09:00'];

        times.forEach((time) => {
          if (!time) return;
          const formattedTime = time.length === 5 ? time : time.padStart(5, '0');
          const alertKey = `${medId}_${formattedTime}`;
          const storageKey = `medicine_${medId}_${formattedTime}`;
          const isTaken = localStorage.getItem(storageKey) === 'taken';

          if (formattedTime === currentTime && !isTaken && !alertedMedicinesRef.current.has(alertKey)) {
            alertedMedicinesRef.current.add(alertKey);
            triggerMedicinePopup(med, formattedTime);
          }
        });
      });
    };

    checkSchedule();
    const interval = setInterval(checkSchedule, 3000);
    return () => clearInterval(interval);
  }, [elderlyId, medicines]);

  const triggerMedicinePopup = (med, time) => {
    const medName = med.medicine_name || med.name || 'Medicine';
    const dosage = med.dosage || '1 Tablet';
    const medId = med.id || medName;
    const storageKey = `medicine_${medId}_${time}`;

    setActiveMedicinePopup({ med, time });

    // Voice announcement
    speakVoiceAlert(`Time to take your medicine: ${medName}`);

    // Browser Notification
    if ('Notification' in window && Notification.permission === 'granted') {
      new Notification('💊 Time for Medicine!', {
        body: `Take ${medName} - ${dosage}`,
        icon: '💊',
        requireInteraction: true
      });
    }

    // Vibration
    if ('vibrate' in navigator) {
      navigator.vibrate([100, 100, 100]);
    }

    // Schedule 3-step follow-up reminders
    scheduleFollowUpReminders(med, time, storageKey);
  };

  const scheduleFollowUpReminders = (med, time, storageKey) => {
    const medName = med.medicine_name || med.name || 'Medicine';

    // 2nd Reminder (after 5 minutes)
    setTimeout(() => {
      if (localStorage.getItem(storageKey) !== 'taken') {
        console.log(`⏰ [MEDICINE FOLLOW-UP 1] ${medName} not taken yet after 5 mins.`);
        speakVoiceAlert(`Second reminder: Please take your medicine ${medName}`);
        if ('Notification' in window && Notification.permission === 'granted') {
          new Notification('⏰ Medicine Follow-up', {
            body: `${medName} - Still not taken`,
            icon: '⏰'
          });
        }
      }
    }, 5 * 60 * 1000);

    // 3rd & Final Urgent Escalation (after 15 minutes) -> Notify Guardian!
    setTimeout(async () => {
      if (localStorage.getItem(storageKey) !== 'taken') {
        console.log(`🚨 [URGENT MEDICINE ALERT] ${medName} not taken after 15 mins (3 reminders)! Alerting Guardian...`);
        speakVoiceAlert(`Urgent: Medicine ${medName} not taken! Contacting guardian.`);

        if ('Notification' in window && Notification.permission === 'granted') {
          new Notification('🚨 URGENT Medicine Alert', {
            body: `Guardian is being notified about missed medicine: ${medName}`,
            icon: '🚨',
            requireInteraction: true
          });
        }

        try {
          await notifyGuardianFall({
            elderly_name: elderlyName || 'Senior',
            elderly_id: elderlyId,
            alert_type: 'MISSED_MEDICINE',
            location: `Missed medicine: ${medName} scheduled at ${time}`,
            message: `MISSED MEDICINE: ${elderlyName || 'Senior'} has not taken medicine ${medName} after 3 reminders.`
          });
        } catch (e) {
          console.error('Error sending missed medicine SMS notification to guardian:', e);
        }
      }
    }, 15 * 60 * 1000);
  };

  const handleMarkTaken = async () => {
    if (!activeMedicinePopup) return;
    const { med, time } = activeMedicinePopup;
    const medId = med.id || med.medicine_name || med.name;

    localStorage.setItem(`medicine_${medId}_${time}`, 'taken');
    setActiveMedicinePopup(null);

    try {
      await confirmMedicine({
        medicine_id: medId,
        elderly_id: elderlyId,
        time: time,
        taken: true
      });
    } catch (e) {
      console.error('Error confirming medicine:', e);
    }
  };

  const handleSnooze = () => {
    if (!activeMedicinePopup) return;
    const { med, time } = activeMedicinePopup;
    const medId = med.id || med.medicine_name || med.name;
    const alertKey = `${medId}_${time}`;

    setActiveMedicinePopup(null);
    setTimeout(() => {
      alertedMedicinesRef.current.delete(alertKey);
    }, 5 * 60 * 1000); // 5 minutes snooze
  };

  // ----------------------------------------------------
  // 2. HARDWARE / MOVEMENT / FALL DETECTION POPUP
  // ----------------------------------------------------
  useEffect(() => {
    if (!elderlyId) return;

    const checkHardwareAlerts = async () => {
      try {
        const res = await fetchHardwareData(elderlyId);
        if (res.ok && res.data) {
          const hw = res.data.data || res.data;
          const fallDetected = hw.fall_detected || hw.fallDetected || false;
          const stateName = (hw.stateName || hw.current_status || '').toUpperCase();

          let alertType = null;
          let title = '';
          let subtitle = '';

          if (fallDetected || stateName.includes('FALL DETECTED') || stateName.includes('FALL_DETECTED')) {
            alertType = 'FALL';
            title = '🚨 FALL DETECTED!';
            subtitle = 'Emergency fall recorded from wearable sensor belt.';
          } else if (stateName.includes('PREFALL') || stateName.includes('PRE-FALL')) {
            alertType = 'PREFALL';
            title = '⚠️ PRE-FALL DETECTED!';
            subtitle = 'Unusual loss of balance detected. Please stay still.';
          } else if (stateName.includes('SUDDEN MOVEMENT') || stateName.includes('SUDDEN_MOVEMENT')) {
            alertType = 'SUDDEN';
            title = '⚠️ SUDDEN MOVEMENT DETECTED!';
            subtitle = 'Rapid impact or sudden motion detected on belt.';
          }

          if (alertType && location.pathname !== '/fall-alert' && !alertDismissedRef.current) {
            alertDismissedRef.current = true;
            try {
              await notifyGuardianFall({
                elderly_name: elderlyName || 'Senior',
                elderly_id: elderlyId,
                alert_type: alertType,
                message: title
              });
            } catch (e) {
              console.error('Failed to notify guardian of fall:', e);
            }
            navigate('/fall-alert', { state: { alertType, title } });
          } else if (!alertType) {
            alertDismissedRef.current = false;
          }
        }
      } catch (e) {
        console.error('Error checking hardware alerts:', e);
      }
    };

    checkHardwareAlerts();
    const interval = setInterval(checkHardwareAlerts, 3000);
    return () => clearInterval(interval);
  }, [elderlyId, elderlyName, activeSensorAlert, speakVoiceAlert]);

  // Countdown timer effect for sensor alerts
  useEffect(() => {
    if (!activeSensorAlert) return;

    const timer = setInterval(() => {
      setCountdown((prev) => {
        if (prev <= 1) {
          clearInterval(timer);
          triggerAutoEmergency();
          return 0;
        }
        return prev - 1;
      });
    }, 1000);

    return () => clearInterval(timer);
  }, [activeSensorAlert]);

  const triggerAutoEmergency = async () => {
    setAlertStatusText('🚨 No response received! Contacting guardian & emergency services automatically...');
    speakVoiceAlert('Emergency services contacted! Guardian is being notified automatically.');

    try {
      await triggerEmergency({
        emergency: true,
        auto_triggered: true,
        elderly_id: elderlyId,
        elderly_name: elderlyName
      });
    } catch (e) {
      console.error('Error triggering auto emergency:', e);
    }
  };

  const handleNeedHelpNow = async () => {
    setAlertStatusText('🚨 Emergency help requested! Contacting guardian now...');
    speakVoiceAlert('Emergency help requested! Guardian is being notified immediately.');

    try {
      await triggerEmergency({
        emergency: true,
        user_requested: true,
        elderly_id: elderlyId,
        elderly_name: elderlyName
      });
    } catch (e) {
      console.error('Error requesting immediate help:', e);
    }
  };

  const handleConfirmSafe = async () => {
    alertDismissedRef.current = true;
    setActiveSensorAlert(null);
    setAlertStatusText('');

    try {
      await confirmSafe({
        response: 'safe',
        user_confirmed: true,
        elderly_id: elderlyId
      });
    } catch (e) {
      console.error('Error confirming safe:', e);
    }
  };

  // Render alerts on any Elderly-side page (chatbot, profile, home, health, etc.), strictly excluding guardian routes & landing page
  const isGuardianPage = location.pathname.startsWith('/guardian');
  const isLandingPage = location.pathname === '/' || location.pathname === '/portal';
  const isElderlyPage = !isGuardianPage && !isLandingPage;

  return (
    <>
      {children}

      {/* ---------------------------------------------------- */}
      {/* MEDICINE REMINDER POPUP MODAL                        */}
      {/* ---------------------------------------------------- */}
      {isElderlyPage && activeMedicinePopup && (
        <div style={{
          position: 'fixed',
          top: 0, left: 0, right: 0, bottom: 0,
          backgroundColor: 'rgba(0, 0, 0, 0.65)',
          backdropFilter: 'blur(6px)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 99999,
          padding: '20px',
          animation: 'fadeIn 0.3s ease-out'
        }}>
          <div style={{
            backgroundColor: '#ffffff',
            borderRadius: '24px',
            padding: '28px 24px',
            maxWidth: '420px',
            width: '100%',
            textAlign: 'center',
            boxShadow: '0 25px 50px -12px rgba(0, 0, 0, 0.3)',
            border: '2px solid #3b82f6'
          }}>
            <div style={{
              width: '72px', height: '72px', borderRadius: '50%',
              backgroundColor: '#dbeafe', color: '#1d4ed8', fontSize: '38px',
              display: 'flex', alignItems: 'center', justifyContent: 'center',
              margin: '0 auto 16px auto', border: '4px solid #bfdbfe'
            }}>
              💊
            </div>

            <h2 style={{ margin: '0 0 6px 0', fontSize: '22px', fontWeight: '800', color: '#1e293b' }}>
              {t('active_reminder') || 'Time for Medicine!'}
            </h2>
            <p style={{ margin: '0 0 16px 0', fontSize: '15px', color: '#64748b', fontWeight: '600' }}>
              Scheduled for <strong>{activeMedicinePopup.time}</strong>
            </p>

            <div style={{
              backgroundColor: '#f1f5f9',
              borderRadius: '16px',
              padding: '16px',
              marginBottom: '20px',
              textAlign: 'center',
              border: '1px solid #cbd5e1'
            }}>
              <div style={{ fontSize: '18px', fontWeight: '800', color: '#0f172a' }}>
                {activeMedicinePopup.med.medicine_name || activeMedicinePopup.med.name}
              </div>
              <div style={{ fontSize: '14px', fontWeight: '600', color: '#2563eb', marginTop: '4px' }}>
                Dosage: {activeMedicinePopup.med.dosage || '1 Tablet'}
              </div>
            </div>

            <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
              <button
                onClick={handleMarkTaken}
                style={{
                  backgroundColor: '#10b981', color: '#ffffff', border: 'none',
                  padding: '14px', borderRadius: '14px', fontSize: '16px',
                  fontWeight: '700', cursor: 'pointer',
                  boxShadow: '0 4px 14px rgba(16, 185, 129, 0.3)'
                }}
              >
                {t('mark_taken') || '✓ I Have Taken'}
              </button>

              <button
                onClick={handleSnooze}
                style={{
                  backgroundColor: '#f59e0b', color: '#ffffff', border: 'none',
                  padding: '12px', borderRadius: '14px', fontSize: '14px',
                  fontWeight: '700', cursor: 'pointer'
                }}
              >
                {t('snooze') || '⏰ Snooze (5 min)'}
              </button>
            </div>
          </div>
        </div>
      )}

      {/* ---------------------------------------------------- */}
      {/* SENSOR ALERT MODAL (FALL, SUDDEN MOVEMENT, PREFALL) */}
      {/* ---------------------------------------------------- */}
      {isElderlyPage && activeSensorAlert && (
        <div style={{
          position: 'fixed',
          top: 0, left: 0, right: 0, bottom: 0,
          backgroundColor: '#dc2626',
          color: '#ffffff',
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 999999,
          padding: '24px',
          textAlign: 'center',
          animation: 'pulse 1s infinite'
        }}>
          {/* Top Banner */}
          <div style={{
            display: 'flex', alignItems: 'center', gap: '12px',
            backgroundColor: 'rgba(255, 255, 255, 0.15)',
            padding: '12px 24px', borderRadius: '30px', marginBottom: '24px'
          }}>
            <span style={{ fontSize: '32px' }}>🚨</span>
            <h1 style={{ fontSize: '22px', fontWeight: '900', margin: 0, letterSpacing: '1px' }}>
              {activeSensorAlert.title}
            </h1>
          </div>

          <p style={{ fontSize: '16px', opacity: 0.95, maxWidth: '360px', margin: '0 0 20px 0', fontWeight: '600' }}>
            {activeSensorAlert.subtitle}
          </p>

          {/* Countdown Circle */}
          <div style={{
            width: '120px', height: '120px', borderRadius: '50%',
            backgroundColor: '#ffffff', color: '#dc2626',
            display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center',
            boxShadow: '0 10px 30px rgba(0, 0, 0, 0.3)', margin: '0 0 24px 0', border: '4px solid #fee2e2'
          }}>
            <div style={{ fontSize: '42px', fontWeight: '900', lineHeight: 1 }}>
              {countdown > 0 ? countdown : '0'}
            </div>
            <div style={{ fontSize: '11px', fontWeight: '800', textTransform: 'uppercase', color: '#991b1b', marginTop: '2px' }}>
              SECONDS
            </div>
          </div>

          <p style={{ fontSize: '14px', backgroundColor: 'rgba(0, 0, 0, 0.25)', padding: '10px 16px', borderRadius: '12px', marginBottom: '28px', maxWidth: '380px' }}>
            {alertStatusText || 'We are contacting your guardian automatically when the timer ends. Please confirm your status.'}
          </p>

          {/* Action Buttons */}
          <div style={{ display: 'flex', flexDirection: 'column', gap: '14px', width: '100%', maxWidth: '360px' }}>
            <button
              onClick={handleNeedHelpNow}
              style={{
                backgroundColor: '#7f1d1d', color: '#ffffff', border: '2px solid #f87171',
                padding: '16px', borderRadius: '16px', fontSize: '17px',
                fontWeight: '800', cursor: 'pointer', display: 'flex',
                alignItems: 'center', justifyContent: 'center', gap: '10px',
                boxShadow: '0 6px 20px rgba(0,0,0,0.4)'
              }}
            >
              🚨 NEED HELP NOW
            </button>

            <button
              onClick={handleConfirmSafe}
              style={{
                backgroundColor: '#15803d', color: '#ffffff', border: '2px solid #4ade80',
                padding: '16px', borderRadius: '16px', fontSize: '17px',
                fontWeight: '800', cursor: 'pointer', display: 'flex',
                alignItems: 'center', justifyContent: 'center', gap: '10px',
                boxShadow: '0 6px 20px rgba(0,0,0,0.4)'
              }}
            >
              ✅ I AM SAFE
            </button>
          </div>
        </div>
      )}
    </>
  );
};

export default ElderlyAlertSystem;
