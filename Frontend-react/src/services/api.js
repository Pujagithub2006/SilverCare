import axios from 'axios';

const normalizeBaseUrl = (value) => (value || '').replace(/\/$/, '');

const resolveBaseUrl = (fallback) => {
  const configuredValue =
    import.meta.env?.VITE_API_BASE_URL ||
    import.meta.env?.VITE_BACKEND_URL;

  return normalizeBaseUrl(configuredValue) || fallback;
};

// Backend Base URLs
const API_BASE = resolveBaseUrl('http://localhost:8081');
const LEGACY_API_BASE = resolveBaseUrl('http://127.0.0.1:8081');

const api = axios.create({
  baseURL: API_BASE,
  headers: {
    'Content-Type': 'application/json',
  },
});

const normalizeEmergencyPayload = (payload = {}) => {
  const normalized = { ...payload };
  if (!normalized.elderly_name && !normalized.elderlyName && !normalized.name) {
    normalized.elderly_name = 'User';
  }
  if (!normalized.guardian_username && !normalized.guardianUsername) {
    normalized.guardian_username = localStorage.getItem('guardian_username') || '';
  }
  if (!normalized.location) {
    normalized.location = 'Unknown location';
  }
  return normalized;
};

// ==================== SENSOR & ALERT HARDWARE API SERVICES ====================

export const fetchSensorData = async (deviceId) => {
  try {
    const response = await api.get('/api/sensor-data', { params: { deviceId } });
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacy = await axios.get(`${LEGACY_API_BASE}/api/sensor-data`, { params: { deviceId } });
      return legacy.data;
    } catch (e) {
      return { status: 'error', message: 'Sensor data unavailable' };
    }
  }
};

export const sendSensorTelemetry = async (telemetryPayload) => {
  try {
    const response = await api.post('/api/sensor-data', telemetryPayload);
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacy = await axios.post(`${LEGACY_API_BASE}/api/sensor-data`, telemetryPayload);
      return legacy.data;
    } catch (e) {
      return { status: 'error', message: 'Sensor data submission failed' };
    }
  }
};

export const fetchDeviceStatus = async () => {
  try {
    const response = await api.get('/api/device-status');
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacy = await axios.get(`${LEGACY_API_BASE}/api/device-status`);
      return legacy.data;
    } catch (e) {
      return { status: 'error', message: 'Device status unavailable' };
    }
  }
};

export const uploadElderlyVoiceMessage = async (elderlyId, deviceId, audioData) => {
  try {
    const response = await api.post('/api/alerts/voice-message', {
      elderlyId,
      deviceId,
      audioData,
    });
    return response.data;
  } catch (error) {
    return { status: 'error', message: 'Voice message upload failed' };
  }
};

export const fetchEEPROMConfig = async (elderlyId) => {
  try {
    const response = await api.get(`/api/elderly/${elderlyId}/contacts`);
    return response.data;
  } catch (error) {
    return { status: 'error', message: 'EEPROM config fetch failed' };
  }
};

export const submitMedicalHistoryCalibration = async (elderlyId, healthHistory) => {
  try {
    const response = await api.post('/api/elderly/calibrate', {
      elderlyId,
      healthHistory,
    });
    return response.data;
  } catch (error) {
    return { status: 'error', message: 'Calibration submission failed' };
  }
};

export const updateHealthHistory = async (elderlyId, payload) => {
  const targetId = elderlyId || 'default_senior';
  console.log('[API] updateHealthHistory called with elderlyId:', elderlyId);
  console.log('[API] Using targetId:', targetId);
  console.log('[API] Payload:', payload);
  try {
    const response = await api.post(`/api/elderly/${targetId}/health-history`, payload);
    console.log('[API] updateHealthHistory success:', response.data);
    return response.data;
  } catch (error) {
    console.log('[API] updateHealthHistory primary error:', error.response?.data);
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacy = await axios.post(`${LEGACY_API_BASE}/api/elderly/${targetId}/health-history`, payload);
      console.log('[API] updateHealthHistory legacy success:', legacy.data);
      return legacy.data;
    } catch (e) {
      console.log('[API] updateHealthHistory legacy error:', e);
      return { status: 'error', message: error.message || 'Health history update failed' };
    }
  }
};

export const fetchHealthHistory = async (elderlyId) => {
  const targetId = elderlyId || 'default_senior';
  try {
    const response = await api.get(`/api/elderly/${targetId}/health-history`);
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacy = await axios.get(`${LEGACY_API_BASE}/api/elderly/${targetId}/health-history`);
      return legacy.data;
    } catch (e) {
      return { status: 'error', message: 'Failed to fetch health history' };
    }
  }
};

export const pairDeviceToElderly = async (macAddress, deviceType, elderlyId, guardianUsername) => {
  try {
    const response = await api.post('/api/device/pair', {
      macAddress,
      deviceType,
      elderlyId,
      guardianUsername
    });
    console.log('[API] Device pairing success:', response.data);
    return response.data;
  } catch (error) {
    console.log('[API] Device pairing error:', error);
    if (error.response && error.response.data) return error.response.data;
    return { status: 'error', message: 'Device pairing failed' };
  }
};

export const unpairDevice = async (arg1, arg2, reason = 'OTHER') => {
  let elderlyId, deviceId;
  if (arg2 && typeof arg2 === 'string' && (arg2.startsWith('DEV_') || arg2.includes(':') || arg2.length > 5)) {
    elderlyId = arg1;
    deviceId = arg2;
  } else {
    deviceId = arg1;
    reason = arg2 || 'OTHER';
  }
  try {
    const response = await api.post('/api/devices/unpair', {
      deviceId,
      elderlyId,
      reason,
    });
    return { ok: true, data: response.data?.data || response.data, message: response.data?.message };
  } catch (error) {
    const errorMsg = error.response?.data?.message || 'Device unpairing failed';
    return { ok: false, message: errorMsg, data: null };
  }
};

export const getElderlyDevices = async (elderlyId) => {
  try {
    const response = await api.get(`/api/devices/elderly/${elderlyId}`);
    return { ok: true, data: response.data?.data || response.data };
  } catch (error) {
    return { ok: false, message: 'Failed to fetch devices', data: { currentDevices: [], previousDevices: [] } };
  }
};

export const getDeviceByMac = async (macAddress) => {
  try {
    const response = await api.get(`/api/devices/mac/${macAddress}`);
    return { ok: true, data: response.data?.data || response.data };
  } catch (error) {
    return { ok: false, message: 'Device not found', data: null };
  }
};

export const saveMitraChat = async (elderlyId, message, sender, mlState, mlConfidence, heartRate, spo2) => {
  try {
    const response = await api.post('/api/mitra/chat', {
      elderlyId,
      message,
      sender,
      mlState,
      mlConfidence,
      heartRate,
      spo2,
    });
    return response.data;
  } catch (error) {
    return { status: 'error', message: 'Failed to save chat' };
  }
};

export const getMitraChats = async (elderlyId) => {
  try {
    const response = await api.get(`/api/mitra/chat/${elderlyId}`);
    return response.data;
  } catch (error) {
    return { status: 'error', message: 'Failed to fetch chats' };
  }
};

export const getChatsForML = async (elderlyId) => {
  try {
    const response = await api.get(`/api/mitra/chat/${elderlyId}/ml-data`);
    return response.data;
  } catch (error) {
    return { status: 'error', message: 'Failed to fetch ML data' };
  }
};

export const markChatAsLearned = async (chatId) => {
  try {
    const response = await api.post(`/api/mitra/chat/${chatId}/mark-learned`);
    return response.data;
  } catch (error) {
    return { status: 'error', message: 'Failed to mark chat as learned' };
  }
};

export const requestLocation = async (elderlyId) => {
  try {
    const response = await api.post(`/api/location/request/${elderlyId}`);
    return response.data;
  } catch (error) {
    return { status: 'error', message: 'Failed to request location' };
  }
};

export const getLocationHistory = async (elderlyId, days = 7) => {
  try {
    const response = await api.get(`/api/location/history/${elderlyId}?days=${days}`);
    return response.data;
  } catch (error) {
    return { status: 'error', message: 'Failed to fetch location history' };
  }
};

export const updateElderlyGeofence = async (elderlyId, geofenceData) => {
  try {
    const response = await api.post(`/api/elderly/${elderlyId}/geofence`, geofenceData);
    return response.data;
  } catch (error) {
    return { status: 'error', message: 'Failed to update geofence settings' };
  }
};

export const reportUnusualLocation = async (elderlyId, latitude, longitude, alertType) => {
  try {
    const response = await api.post(`/api/location/unusual-alert/${elderlyId}`, {
      latitude,
      longitude,
      alertType
    });
    return response.data;
  } catch (error) {
    return { status: 'error', message: 'Failed to report unusual location' };
  }
};

// Emergency notification APIs
export const notifyHospital = async (elderlyId, alertData) => {
  try {
    const response = await api.post(`/api/emergency/notify-hospital/${elderlyId}`, alertData);
    return response.data;
  } catch (error) {
    return { status: 'error', message: 'Failed to notify hospital' };
  }
};

// Health report APIs
export const getDailyReport = async (elderlyId) => {
  try {
    const response = await api.get(`/api/reports/daily/${elderlyId}`);
    return response.data;
  } catch (error) {
    return { status: 'error', message: 'Failed to fetch daily report' };
  }
};

export const getWeeklyReport = async (elderlyId) => {
  try {
    const response = await api.get(`/api/reports/weekly/${elderlyId}`);
    return response.data;
  } catch (error) {
    return { status: 'error', message: 'Failed to fetch weekly report' };
  }
};

export const getReportList = async (elderlyId) => {
  try {
    const response = await api.get(`/api/reports/list/${elderlyId}`);
    return response.data;
  } catch (error) {
    return { status: 'error', message: 'Failed to fetch report list' };
  }
};

export const downloadReportPDF = async (reportId) => {
  try {
    const response = await api.get(`/api/reports/download/${reportId}`, {
      responseType: 'blob'
    });
    return response.data;
  } catch (error) {
    throw new Error('Failed to download PDF');
  }
};

export const acknowledgeAlert = async (alertId, guardianUsername, responseMessage = 'I am Fine') => {
  try {
    const response = await api.post('/api/alerts/acknowledge', {
      alertId,
      guardianUsername,
      responseMessage,
    });
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacy = await axios.post(`${LEGACY_API_BASE}/api/alerts/acknowledge`, {
        alertId,
        guardianUsername,
        responseMessage,
      });
      return legacy.data;
    } catch (e) {
      return { status: 'error', message: 'Alert acknowledgment failed' };
    }
  }
};

export const fetchElderlyContacts = async (elderlyId) => {
  try {
    const response = await api.get(`/api/elderly/${elderlyId}/contacts`);
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacy = await axios.get(`${LEGACY_API_BASE}/api/elderly/${elderlyId}/contacts`);
      return legacy.data;
    } catch (e) {
      return { status: 'error', message: 'Failed to fetch contacts' };
    }
  }
};

export const getElderlyInfo = async (elderlyId) => {
  try {
    const response = await api.get(`/api/elderly/${elderlyId}`);
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacy = await axios.get(`${LEGACY_API_BASE}/api/elderly/${elderlyId}`);
      return legacy.data;
    } catch (e) {
      return { status: 'error', message: 'Failed to fetch elderly info' };
    }
  }
};

export const updateElderlyContacts = async (elderlyId, contactData) => {
  try {
    const response = await api.post(`/api/elderly/${elderlyId}/contacts`, contactData);
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacy = await axios.post(`${LEGACY_API_BASE}/api/elderly/${elderlyId}/contacts`, contactData);
      return legacy.data;
    } catch (e) {
      return { status: 'error', message: 'Failed to update contacts' };
    }
  }
};

export const fetchActiveAlerts = async (elderlyId) => {
  try {
    const response = await api.get(`/api/alerts/active/${elderlyId}`);
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacy = await axios.get(`${LEGACY_API_BASE}/api/alerts/active/${elderlyId}`);
      return legacy.data;
    } catch (e) {
      return { status: 'error', alerts: [] };
    }
  }
};

export const uploadVoiceMessage = async (elderlyId, deviceId, audioData, triggerEvent = 'MANUAL') => {
  try {
    const response = await api.post('/api/alerts/voice-message', {
      elderlyId,
      deviceId,
      audioData,
      triggerEvent,
    });
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacy = await axios.post(`${LEGACY_API_BASE}/api/alerts/voice-message`, {
        elderlyId,
        deviceId,
        audioData,
        triggerEvent,
      });
      return legacy.data;
    } catch (e) {
      return { status: 'error', message: 'Voice upload failed' };
    }
  }
};

export const fetchFirebaseRecords = async () => {
  try {
    const response = await api.get('/api/alerts/firebase-encrypted-storage');
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacy = await axios.get(`${LEGACY_API_BASE}/api/alerts/firebase-encrypted-storage`);
      return legacy.data;
    } catch (e) {
      return { status: 'error', records: [] };
    }
  }
};

// ==================== ELDERLY API SERVICES ====================

export const elderlyLogin = async (name, phone) => {
  try {
    const response = await api.post('/elderly/login', { name, phone });
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) {
      return error.response.data;
    }
    try {
      const legacyResponse = await axios.post(`${LEGACY_API_BASE}/elderly-login`, { name, phone });
      return legacyResponse.data;
    } catch (e) {
      return { status: 'error', message: 'Login failed. Please check your name and phone.' };
    }
  }
};

export const elderlyRegister = async (elderlyData) => {
  try {
    const response = await api.post('/elderly-register', elderlyData);
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) {
      return error.response.data;
    }
    try {
      const legacyResponse = await axios.post(`${LEGACY_API_BASE}/elderly-register`, elderlyData);
      return legacyResponse.data;
    } catch (e) {
      return e.response?.data || { status: 'error', message: 'Registration failed. Check guardian username & password.' };
    }
  }
};

export const registerElderlySession = async (elderlyId, deviceInfo) => {
  try {
    const response = await api.post('/elderly/register-session', { elderly_id: elderlyId, device_info: deviceInfo });
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacyResponse = await axios.post(`${LEGACY_API_BASE}/elderly/register-session`, { elderly_id: elderlyId, device_info: deviceInfo });
      return legacyResponse.data;
    } catch (e) {
      return { status: 'error' };
    }
  }
};

export const unregisterElderlySession = async (elderlyId) => {
  try {
    const response = await api.post('/elderly/unregister-session', { elderly_id: elderlyId });
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacyResponse = await axios.post(`${LEGACY_API_BASE}/elderly/unregister-session`, { elderly_id: elderlyId });
      return legacyResponse.data;
    } catch (e) {
      return { status: 'error' };
    }
  }
};

export const getElderlyNotifications = async (elderlyId) => {
  try {
    const response = await api.get(`/elderly/notifications/${elderlyId}`);
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacyResponse = await axios.get(`${LEGACY_API_BASE}/elderly/notifications/${elderlyId}`);
      return legacyResponse.data;
    } catch (e) {
      return { status: 'error', notifications: [] };
    }
  }
};

export const clearElderlyNotification = async (elderlyId, medicineId, response) => {
  try {
    const apiResponse = await api.post('/elderly/clear-notification', { elderly_id: elderlyId, medicine_id: medicineId, response });
    return apiResponse.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacyResponse = await axios.post(`${LEGACY_API_BASE}/elderly/clear-notification`, { elderly_id: elderlyId, medicine_id: medicineId, response });
      return legacyResponse.data;
    } catch (e) {
      return { status: 'error' };
    }
  }
};

export const notifyGuardianFall = async (payload) => {
  try {
    const response = await api.post('/notify-guardian-fall', normalizeEmergencyPayload(payload));
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacyResponse = await axios.post(`${LEGACY_API_BASE}/notify-guardian-fall`, payload);
      return legacyResponse.data;
    } catch (e) {
      return { status: 'error' };
    }
  }
};

export const triggerEmergency = async (payload) => {
  try {
    const response = await api.post('/emergency-call', normalizeEmergencyPayload(payload));
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacyResponse = await axios.post(`${LEGACY_API_BASE}/api/emergency`, payload);
      return legacyResponse.data;
    } catch (e) {
      return { status: 'error' };
    }
  }
};

export const confirmSafe = async (payload) => {
  try {
    const response = await api.post('/notify-guardian-safe', normalizeEmergencyPayload(payload));
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacyResponse = await axios.post(`${LEGACY_API_BASE}/api/response`, payload);
      return legacyResponse.data;
    } catch (e) {
      return { status: 'error' };
    }
  }
};

export const getMedicines = async (elderlyId) => {
  try {
    const response = await api.get(`/medicines/${elderlyId}`);
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacyResponse = await axios.get(`${LEGACY_API_BASE}/medicines/${elderlyId}`);
      return legacyResponse.data;
    } catch (e) {
      return { status: 'error', medicines: [] };
    }
  }
};

export const confirmMedicineTaken = async ({ medicineId, elderlyId, timeTaken, taken = true }) => {
  try {
    const response = await api.post('/medicine/confirm', {
      medicine_id: medicineId,
      elderly_id: elderlyId,
      time_taken: timeTaken,
      taken,
    });
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    try {
      const legacyResponse = await axios.post(`${LEGACY_API_BASE}/medicine/confirm`, {
        medicine_id: medicineId,
        elderly_id: elderlyId,
        time_taken: timeTaken,
        taken,
      });
      return legacyResponse.data;
    } catch (e) {
      return { status: 'error' };
    }
  }
};

export const confirmMedicine = confirmMedicineTaken;

// ==================== GUARDIAN API SERVICES ====================

export async function guardianLogin(username, password) {
  try {
    const response = await api.post('/guardian-login', { username, password });
    return { ok: true, data: response.data };
  } catch (error) {
    if (error.response && error.response.data) {
      return { ok: false, data: error.response.data };
    }
    try {
      const legacyResponse = await fetch(`${LEGACY_API_BASE}/guardian-login`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ username, password }),
      });
      return { ok: legacyResponse.ok, data: await legacyResponse.json() };
    } catch (e) {
      return { ok: false, data: { status: 'error', message: 'Backend connection failed' } };
    }
  }
}

export async function guardianRegister(userData) {
  try {
    const response = await api.post('/guardian-register', userData);
    return { ok: true, data: response.data };
  } catch (error) {
    if (error.response && error.response.data) {
      return { ok: false, data: error.response.data };
    }
    try {
      const legacyResponse = await fetch(`${LEGACY_API_BASE}/guardian-register`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(userData),
      });
      return { ok: legacyResponse.ok, data: await legacyResponse.json() };
    } catch (e) {
      return { ok: false, data: { status: 'error', message: 'Backend connection failed' } };
    }
  }
}

export async function fetchLinkedElderly(guardianUsername) {
  try {
    const response = await api.get(`/guardian-elderly/${guardianUsername}`);
    return { ok: true, data: response.data };
  } catch (error) {
    if (error.response && error.response.data) {
      return { ok: false, data: error.response.data };
    }
    try {
      const legacyResponse = await fetch(`${LEGACY_API_BASE}/guardian-elderly/${guardianUsername}`);
      return { ok: legacyResponse.ok, data: await legacyResponse.json() };
    } catch (e) {
      return { ok: false, data: { status: 'error', message: 'Backend connection failed' } };
    }
  }
}

export async function fetchMedicines(elderlyId) {
  try {
    const response = await api.get(`/medicines/${elderlyId}`);
    return { ok: true, data: response.data };
  } catch (error) {
    if (error.response && error.response.data) {
      return { ok: false, data: error.response.data };
    }
    try {
      const legacyResponse = await fetch(`${LEGACY_API_BASE}/medicines/${elderlyId}`);
      return { ok: legacyResponse.ok, data: await legacyResponse.json() };
    } catch (e) {
      return { ok: false, data: { status: 'error', message: 'Backend connection failed' } };
    }
  }
}

export async function fetchElderlyInfo(elderlyId) {
  try {
    const response = await api.get(`/elderly-info/${elderlyId}`);
    return { ok: true, data: response.data };
  } catch (error) {
    if (error.response && error.response.data) {
      return { ok: false, data: error.response.data };
    }
    try {
      const legacyResponse = await fetch(`${LEGACY_API_BASE}/elderly-info/${elderlyId}`);
      return { ok: legacyResponse.ok, data: await legacyResponse.json() };
    } catch (e) {
      return { ok: false, data: { status: 'error', message: 'Backend connection failed' } };
    }
  }
}

export async function fetchHardwareData(elderlyId) {
  try {
    const response = await api.get(`/hardware-data/${elderlyId}`);
    return { ok: true, data: response.data };
  } catch (error) {
    if (error.response && error.response.data) {
      return { ok: false, data: error.response.data };
    }
    try {
      const legacyResponse = await fetch(`${LEGACY_API_BASE}/hardware-data/${elderlyId}`);
      return { ok: legacyResponse.ok, data: await legacyResponse.json() };
    } catch (e) {
      return { ok: false, data: { status: 'error', message: 'Backend connection failed' } };
    }
  }
}

export async function addMedicine(medicineData) {
  const payload = {
    guardian_username: medicineData.guardian_username || medicineData.guardianUsername || localStorage.getItem('guardian_username') || '',
    elderly_id: medicineData.elderly_id || medicineData.elderlyId || '',
    medicine_name: medicineData.medicine_name || medicineData.medicineName || '',
    dosage: medicineData.dosage || '1 Tablet',
    times: Array.isArray(medicineData.times) ? medicineData.times : [medicineData.times || '08:00'],
    instructions: medicineData.instructions || '',
    start_date: medicineData.start_date || medicineData.startDate || new Date().toISOString().split('T')[0],
    end_date: medicineData.end_date || medicineData.endDate || medicineData.start_date || medicineData.startDate || new Date().toISOString().split('T')[0]
  };

  try {
    const response = await api.post('/medicine/add', payload);
    return { ok: true, data: response.data };
  } catch (error) {
    if (error.response && error.response.data) {
      return { ok: false, data: error.response.data };
    }
    try {
      const legacyResponse = await fetch(`${LEGACY_API_BASE}/medicine/add`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
      });
      return { ok: legacyResponse.ok, data: await legacyResponse.json() };
    } catch (e) {
      return { ok: false, data: { status: 'error', message: 'Backend connection failed' } };
    }
  }
}

export async function deleteMedicine(medicineId, elderlyId) {
  try {
    const response = await api.post(`/medicine/delete/${medicineId}`, { elderly_id: elderlyId });
    return { ok: true, data: response.data };
  } catch (error) {
    if (error.response && error.response.data) {
      return { ok: false, data: error.response.data };
    }
    try {
      const legacyResponse = await fetch(`${LEGACY_API_BASE}/medicine/delete/${medicineId}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ elderly_id: elderlyId }),
      });
      return { ok: legacyResponse.ok, data: await legacyResponse.json() };
    } catch (e) {
      return { ok: false, data: { status: 'error', message: 'Backend connection failed' } };
    }
  }
}

export async function fetchSuggestions(elderlyId) {
  try {
    const response = await api.get(`/medicine/suggestions/${elderlyId}`);
    return { ok: true, data: response.data };
  } catch (error) {
    if (error.response && error.response.data) {
      return { ok: false, data: error.response.data };
    }
    try {
      const legacyResponse = await fetch(`${LEGACY_API_BASE}/medicine/suggestions/${elderlyId}`);
      return { ok: legacyResponse.ok, data: await legacyResponse.json() };
    } catch (e) {
      return { ok: false, data: { status: 'error', message: 'Backend connection failed' } };
    }
  }
}

export async function saveSuggestions(elderlyId, notes) {
  const payload = {
    guardian_username: localStorage.getItem('guardian_username') || '',
    suggestion: notes,
    notes: notes
  };
  try {
    const response = await api.post(`/medicine/suggestions/${elderlyId}`, payload);
    return { ok: true, data: response.data };
  } catch (error) {
    if (error.response && error.response.data) {
      return { ok: false, data: error.response.data };
    }
    try {
      const legacyResponse = await fetch(`${LEGACY_API_BASE}/medicine/suggestions/${elderlyId}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
      });
      return { ok: legacyResponse.ok, data: await legacyResponse.json() };
    } catch (e) {
      return { ok: false, data: { status: 'error', message: 'Backend connection failed' } };
    }
  }
}

export async function sendChatMessage(userMessage, elderlyId, elderlyName, history = []) {
  const payload = {
    message: userMessage,
    prompt: userMessage,
    text: userMessage,
    elderly_id: elderlyId || 'default_senior',
    elderly_name: elderlyName || '',
    name: elderlyName || '',
    history: Array.isArray(history) ? history : []
  };

  try {
    const response = await api.post('/chat', payload);
    return response.data;
  } catch (error) {
    try {
      const legacyResponse = await fetch(`${LEGACY_API_BASE}/chat`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });
      if (legacyResponse.ok) {
        return await legacyResponse.json();
      }
    } catch (e) {
      console.warn('Legacy chat API failed:', e);
    }
    throw error;
  }
}

// ==================== DEVICE MANAGEMENT API SERVICES ====================

export const fetchAllDevices = async () => {
  try {
    const response = await api.get('/api/devices');
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    return { status: 'error', message: 'Failed to fetch devices' };
  }
};

export const fetchUnassignedDevices = async () => {
  try {
    const response = await api.get('/api/devices/unassigned');
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    return { status: 'error', message: 'Failed to fetch unassigned devices' };
  }
};

export const fetchPotentiallyBrokenDevices = async () => {
  try {
    const response = await api.get('/api/devices/potentially-broken');
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    return { status: 'error', message: 'Failed to check device status' };
  }
};

export const assignDeviceToElderly = async (deviceId, elderlyId) => {
  try {
    const response = await api.post(`/api/devices/${deviceId}/assign`, { elderlyId });
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    return { status: 'error', message: 'Failed to assign device' };
  }
};

export const replaceDevice = async (oldDeviceId, newDeviceId) => {
  try {
    const response = await api.post('/api/devices/replace', { oldDeviceId, newDeviceId });
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    return { status: 'error', message: 'Failed to replace device' };
  }
};

export const markDeviceBroken = async (deviceId, notes) => {
  try {
    const response = await api.post(`/api/devices/${deviceId}/mark-broken`, { notes });
    return response.data;
  } catch (error) {
    if (error.response && error.response.data) return error.response.data;
    return { status: 'error', message: 'Failed to mark device as broken' };
  }
};

export { API_BASE, LEGACY_API_BASE };
export default api;
