import React, { useState, useEffect } from 'react';
import { useNavigate, useLocation } from 'react-router-dom';
import { notifyGuardianFall, triggerEmergency, confirmSafe } from '../services/api';
import { useLanguage } from '../context/LanguageContext';

const FallAlert = () => {
  const navigate = useNavigate();
  const location = useLocation();
  const { t } = useLanguage();

  const alertType = location.state?.alertType || 'FALL';
  const alertTitle = location.state?.title || 'FALL DETECTED!';

  const [timeLeft, setTimeLeft] = useState(10);
  const [timerInterval, setTimerInterval] = useState(null);
  const [timerDisplay, setTimerDisplay] = useState('10');
  const [alertMessage, setAlertMessage] = useState('');

  useEffect(() => {
    // Speak voice alert announcement
    if ('speechSynthesis' in window) {
      window.speechSynthesis.cancel();
      const text = `Warning! ${alertTitle}. Please confirm your status.`;
      const utterance = new SpeechSynthesisUtterance(text);
      utterance.rate = 0.9;
      window.speechSynthesis.speak(utterance);
    }

    // Notify guardian immediately when detection occurs
    notifyGuardianOfFall();

    // Start countdown timer
    const interval = setInterval(() => {
      setTimeLeft((prev) => {
        const newTime = prev - 1;
        if (newTime <= 0) {
          clearInterval(interval);
          contactGuardian();
          return 0;
        }
        setTimerDisplay(newTime.toString());
        return newTime;
      });
    }, 1000);

    setTimerInterval(interval);

    return () => {
      if (interval) clearInterval(interval);
    };
  }, []);

  const notifyGuardianOfFall = async () => {
    try {
      const elderlyId = localStorage.getItem('elderly_id') || 'isha_amit';
      const elderlyName = localStorage.getItem('elderly_name') || 'Senior Citizen';

      const payload = {
        elderly_name: elderlyName,
        elderly_id: elderlyId,
        alert_type: alertType,
        device_id: 'belt_001',
        location: 'Home'
      };
      const response = await notifyGuardianFall(payload);
      console.log('[GUARDIAN NOTIFICATION] Alert detected - Guardian notified:', response);
    } catch (error) {
      console.log('[GUARDIAN NOTIFICATION] Error notifying guardian:', error);
    }
  };

  const requestHelpNow = async () => {
    console.log('🚨 [FALL ALERT] User requested immediate help!');
    
    if (timerInterval) clearInterval(timerInterval);
    
    setTimerDisplay('HELP!');
    setAlertMessage('Emergency help requested! Contacting guardian now...');
    
    try {
      const response = await triggerEmergency({
        emergency: true,
        user_requested: true,
        alert_type: alertType,
        elderly_id: localStorage.getItem('elderly_id') || ''
      });
      console.log('🚨 [FALL ALERT] Emergency triggered:', response);
      
      setAlertMessage('Emergency services contacted! Guardian is being notified immediately.');
      
      if ('vibrate' in navigator) {
        navigator.vibrate([200, 100, 200, 100, 200]);
      }
    } catch (error) {
      console.error('❌ [FALL ALERT] Error triggering emergency:', error);
      setAlertMessage('Error contacting emergency services. Please call for help manually!');
    }
  };

  const confirmSafeStatus = async () => {
    console.log('✅ [FALL ALERT] User confirmed they are safe!');
    
    if (timerInterval) clearInterval(timerInterval);
    
    setAlertMessage('Great! You confirmed you are safe. Redirecting to home...');
    
    try {
      const response = await confirmSafe({
        response: 'safe',
        user_confirmed: true,
        alert_type: alertType,
        elderly_id: localStorage.getItem('elderly_id') || ''
      });
      console.log('✅ [FALL ALERT] Safe confirmation sent:', response);
      
      setTimeout(() => {
        navigate('/home');
      }, 2000);
    } catch (error) {
      console.error('❌ [FALL ALERT] Error confirming safe:', error);
      setTimeout(() => {
        navigate('/home');
      }, 2000);
    }
  };

  const contactGuardian = async () => {
    console.log('🚨 [FALL ALERT] Timer expired - Auto-contacting guardian!');
    
    setAlertMessage('No response received! Contacting guardian automatically...');
    setTimerDisplay('EMERGENCY');
    
    try {
      const response = await triggerEmergency({
        emergency: true,
        auto_triggered: true,
        alert_type: alertType,
        elderly_id: localStorage.getItem('elderly_id') || ''
      });
      console.log('🚨 [FALL ALERT] Auto-emergency triggered:', response);
      
      setAlertMessage('Emergency services contacted! Guardian is being notified automatically.');
      
      document.body.style.backgroundColor = '#ff4444';
    } catch (error) {
      console.error('❌ [FALL ALERT] Error in auto-emergency:', error);
      setAlertMessage('Error contacting emergency services. Please call for help manually!');
    }
  };

  return (
    <div className="container fall-alert-page">
      {/* Alert Banner */}
      <div className="alert-banner">
        <svg width="40" height="40" viewBox="0 0 40 40" fill="white">
          <path d="M20 4L4 36h32L20 4z" fill="white" opacity="0.9"/>
          <text x="20" y="28" textAnchor="middle" fill="#DC2626" fontSize="20" fontWeight="bold">!</text>
        </svg>
        <h1 className="alert-title">{alertTitle}</h1>
      </div>

      {/* Main Content */}
      <div className="fall-alert-content">
        <h2 className="alert-heading">Sending alert in...</h2>
        <p className="alert-subheading">Please confirm your status.</p>

        {/* Timer */}
        <div className="timer-container">
          <div className="timer-circle">
            <div className="timer-number">{timerDisplay}</div>
            <div className="timer-label">SECONDS</div>
          </div>
        </div>

        {/* Info Message */}
        <div className="info-message">
          <svg width="32" height="32" viewBox="0 0 32 32" fill="#6B7280">
            <path d="M16 4C9.4 4 4 9.4 4 16s5.4 12 12 12 12-5.4 12-12S22.6 4 16 4zm2 18h-4v-8h4v8zm0-10h-4V8h4v4z"/>
          </svg>
          <p className="info-text">We are contacting your guardian automatically when the timer ends.</p>
        </div>

        {alertMessage && (
          <div className="info-message" style={{ marginTop: '20px' }}>
            <p className="info-text">{alertMessage}</p>
          </div>
        )}
      </div>

      {/* Action Buttons */}
      <div className="alert-actions">
        <button className="alert-button primary-alert" onClick={requestHelpNow}>
          <svg width="28" height="28" viewBox="0 0 28 28" fill="white">
            <path d="M14 2v10M14 16v10M4 14h10M18 14h10M6 6l7 7M22 6l-7 7M6 22l7-7M22 22l-7-7" stroke="white" strokeWidth="3" strokeLinecap="round"/>
          </svg>
          <span>{t('fallalert.need_help_now') || 'Need Help Now'}</span>
        </button>
        
        <button className="alert-button safe-alert" onClick={confirmSafeStatus}>
          <svg width="28" height="28" viewBox="0 0 28 28" fill="currentColor">
            <circle cx="14" cy="14" r="14" fill="currentColor"/>
            <path d="M8 14l4 4 8-8" stroke="white" strokeWidth="3" fill="none" strokeLinecap="round"/>
          </svg>
          <span>{t('fallalert.i_am_safe') || 'I am Safe'}</span>
        </button>
      </div>
    </div>
  );
};

export default FallAlert;
