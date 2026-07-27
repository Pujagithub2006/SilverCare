import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { registerElderlySession, unregisterElderlySession, getElderlyNotifications, clearElderlyNotification } from '../services/api';

const ElderlyDashboard = () => {
  const navigate = useNavigate();
  const [elderlyId, setElderlyId] = useState(null);
  const [elderlyName, setElderlyName] = useState('');
  const [status, setStatus] = useState('Connecting to SilverCare...');
  const [notifications, setNotifications] = useState({});
  const [notificationCheckInterval, setNotificationCheckInterval] = useState(null);

  useEffect(() => {
    const id = localStorage.getItem('elderly_id');
    const name = localStorage.getItem('elderly_name');
    
    if (!id) {
      setStatus('Please login first');
      navigate('/login');
      return;
    }

    setElderlyId(id);
    setElderlyName(name);
    registerSession(id);

    return () => {
      if (notificationCheckInterval) {
        clearInterval(notificationCheckInterval);
      }
      if (id) {
        unregisterSession(id);
      }
    };
  }, [navigate]);

  const registerSession = async (id) => {
    try {
      const deviceInfo = `${navigator.platform} - ${navigator.userAgent.split(' ')[0]}`;
      const response = await registerElderlySession(id, deviceInfo);

      if (response.status === 'success') {
        setStatus('Connected and monitoring');
        startNotificationChecking();
      } else {
        setStatus('Connection failed');
      }
    } catch (error) {
      console.error('Session registration failed:', error);
      setStatus('Connection error');
    }
  };

  const unregisterSession = async (id) => {
    try {
      await unregisterElderlySession(id);
    } catch (error) {
      console.error('Session unregistration failed:', error);
    }
  };

  const startNotificationChecking = () => {
    const interval = setInterval(checkNotifications, 3000);
    setNotificationCheckInterval(interval);
    checkNotifications();
  };

  const checkNotifications = async () => {
    try {
      console.log('🔍 [ELDERLY] Checking notifications...');
      const response = await getElderlyNotifications(elderlyId);
      
      console.log('📱 [ELDERLY] Response:', response);
      console.log('📊 [ELDERLY] Notifications count:', Object.keys(response.notifications || {}).length);
      
      if (response.status === 'success' && response.notifications) {
        setNotifications(response.notifications);
      } else {
        console.log('⚠️ [ELDERLY] No notifications or error:', response);
      }
    } catch (error) {
      console.error('❌ [ELDERLY] Error checking notifications:', error);
    }
  };

  const handleResponse = async (medicineId, response) => {
    try {
      const result = await clearElderlyNotification(elderlyId, medicineId, response);
      
      if (result.status === 'success') {
        checkNotifications();
        showConfirmation(response);
      }
    } catch (error) {
      console.error('Error handling response:', error);
    }
  };

  const showConfirmation = (response) => {
    const messages = {
      'taken': '✅ Medicine marked as taken',
      'snooze': '⏰ Reminder snoozed for 5 minutes',
      'not_taken': '❌ Medicine marked as not taken'
    };

    const toast = document.createElement('div');
    toast.style.cssText = `
      position: fixed;
      top: 20px;
      right: 20px;
      background: #333;
      color: white;
      padding: 12px 20px;
      border-radius: 8px;
      z-index: 1000;
    `;
    toast.textContent = messages[response] || 'Response recorded';
    
    document.body.appendChild(toast);
    
    setTimeout(() => {
      toast.remove();
    }, 3000);
  };

  const renderNotifications = () => {
    if (Object.keys(notifications).length === 0) {
      return <div className="status">No active reminders</div>;
    }

    return Object.values(notifications).map((notification, index) => {
      const time = new Date(notification.timestamp).toLocaleTimeString();
      
      const buttons = notification.options.map((option, btnIndex) => {
        if (option === 'taken') {
          return (
            <button
              key={btnIndex}
              className="btn btn-taken"
              onClick={() => handleResponse(notification.medicine.id, 'taken')}
            >
              ✓ Taken
            </button>
          );
        } else if (option === 'snooze') {
          return (
            <button
              key={btnIndex}
              className="btn btn-snooze"
              onClick={() => handleResponse(notification.medicine.id, 'snooze')}
            >
              ⏰ Snooze 2 min
            </button>
          );
        } else if (option === 'not_taken') {
          return (
            <button
              key={btnIndex}
              className="btn btn-not-taken"
              onClick={() => handleResponse(notification.medicine.id, 'not_taken')}
            >
              ✗ Not Taken
            </button>
          );
        }
        return null;
      });

      return (
        <div key={index} className="notification-card">
          <div className="notification-time">{time}</div>
          <div className="notification-message">{notification.message}</div>
          <div className="notification-buttons">{buttons}</div>
        </div>
      );
    });
  };

  return (
    <div style={{
      background: '#f5f5f7',
      minHeight: '100vh',
      fontFamily: '-apple-system, BlinkMacSystemFont, "Segoe UI", system-ui, sans-serif',
      margin: 0,
      padding: '20px'
    }}>
      <div className="dashboard-container">
        <div className="dashboard-header">
          <h1>👴 SilverCare</h1>
          <p>Your Health Companion</p>
        </div>
        
        <div className="elderly-info">
          <h3>{elderlyName || 'Loading...'}</h3>
          <p>{status}</p>
        </div>
        
        <div className="notification-area">
          <h3>Medicine Reminders</h3>
          <div id="notificationsList">
            {renderNotifications()}
          </div>
        </div>
      </div>
    </div>
  );
};

export default ElderlyDashboard;
