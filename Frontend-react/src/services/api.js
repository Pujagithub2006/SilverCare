import axios from 'axios';

const normalizeBaseUrl = (value) => (value || '').replace(/\/$/, '');

const resolveBaseUrl = (fallback) => {
  const configuredValue = import.meta.env?.VITE_API_BASE_URL || import.meta.env?.VITE_BACKEND_URL;
  return normalizeBaseUrl(configuredValue) || fallback;
};

<<<<<<< HEAD
// Backend Base URLs (Defaulting to 5002 matching ESP32 firmware)
const API_BASE = resolveBaseUrl('http://localhost:8081');
const LEGACY_API_BASE = resolveBaseUrl('http://127.0.0.1:8081');
=======
// Backend Base URLs (Defaulting to Spring Boot 8081, Legacy fallback 5002)
const API_BASE = resolveBaseUrl('http://localhost:8081');
const LEGACY_API_BASE = resolveBaseUrl('http://localhost:5002');
>>>>>>> e2913686d02c1cbe8cd6b49f80277aadf8531a24

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

export const fetchSensorData = async () => {
  try {
    const response = await api.get('/api/sensor-data');
    return response.data;
  } catch (error) {
    const legacy = await axios.get(`${LEGACY_API_BASE}/api/sensor-data`);
    return legacy.data;
  }
};

export const fetchDeviceStatus = async () => {
  try {
    const response = await api.get('/api/device-status');
    return response.data;
  } catch (error) {
    const legacy = await axios.get(`${LEGACY_API_BASE}/api/device-status`);
    return legacy.data;
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
    const legacy = await axios.post(`${LEGACY_API_BASE}/api/alerts/acknowledge`, {
      alertId,
      guardianUsername,
      responseMessage,
    });
    return legacy.data;
  }
};

export const fetchActiveAlerts = async (elderlyId) => {
  try {
    const response = await api.get(`/api/alerts/active/${elderlyId}`);
    return response.data;
  } catch (error) {
    const legacy = await axios.get(`${LEGACY_API_BASE}/api/alerts/active/${elderlyId}`);
    return legacy.data;
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
    const legacy = await axios.post(`${LEGACY_API_BASE}/api/alerts/voice-message`, {
      elderlyId,
      deviceId,
      audioData,
      triggerEvent,
    });
    return legacy.data;
  }
};

export const fetchFirebaseRecords = async () => {
  try {
    const response = await api.get('/api/alerts/firebase-encrypted-storage');
    return response.data;
  } catch (error) {
    const legacy = await axios.get(`${LEGACY_API_BASE}/api/alerts/firebase-encrypted-storage`);
    return legacy.data;
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
    const legacyResponse = await axios.post(`${LEGACY_API_BASE}/elderly/register-session`, { elderly_id: elderlyId, device_info: deviceInfo });
    return legacyResponse.data;
  }
};

export const unregisterElderlySession = async (elderlyId) => {
  try {
    const response = await api.post('/elderly/unregister-session', { elderly_id: elderlyId });
    return response.data;
  } catch (error) {
    const legacyResponse = await axios.post(`${LEGACY_API_BASE}/elderly/unregister-session`, { elderly_id: elderlyId });
    return legacyResponse.data;
  }
};

export const getElderlyNotifications = async (elderlyId) => {
  try {
    const response = await api.get(`/elderly/notifications/${elderlyId}`);
    return response.data;
  } catch (error) {
    const legacyResponse = await axios.get(`${LEGACY_API_BASE}/elderly/notifications/${elderlyId}`);
    return legacyResponse.data;
  }
};

export const clearElderlyNotification = async (elderlyId, medicineId, response) => {
  try {
    const apiResponse = await api.post('/elderly/clear-notification', { elderly_id: elderlyId, medicine_id: medicineId, response });
    return apiResponse.data;
  } catch (error) {
    const legacyResponse = await axios.post(`${LEGACY_API_BASE}/elderly/clear-notification`, { elderly_id: elderlyId, medicine_id: medicineId, response });
    return legacyResponse.data;
  }
};

export const notifyGuardianFall = async (payload) => {
  try {
    const response = await api.post('/notify-guardian-fall', normalizeEmergencyPayload(payload));
    return response.data;
  } catch (error) {
    const legacyResponse = await axios.post(`${LEGACY_API_BASE}/notify-guardian-fall`, payload);
    return legacyResponse.data;
  }
};

export const triggerEmergency = async (payload) => {
  try {
    const response = await api.post('/emergency-call', normalizeEmergencyPayload(payload));
    return response.data;
  } catch (error) {
    const legacyResponse = await axios.post(`${LEGACY_API_BASE}/api/emergency`, payload);
    return legacyResponse.data;
  }
};

export const confirmSafe = async (payload) => {
  try {
    const response = await api.post('/notify-guardian-safe', normalizeEmergencyPayload(payload));
    return response.data;
  } catch (error) {
    const legacyResponse = await axios.post(`${LEGACY_API_BASE}/api/response`, payload);
    return legacyResponse.data;
  }
};

export const getMedicines = async (elderlyId) => {
  try {
    const response = await api.get(`/medicines/${elderlyId}`);
    return response.data;
  } catch (error) {
    const legacyResponse = await axios.get(`${LEGACY_API_BASE}/medicines/${elderlyId}`);
    return legacyResponse.data;
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
    const legacyResponse = await axios.post(`${LEGACY_API_BASE}/medicine/confirm`, {
      medicine_id: medicineId,
      elderly_id: elderlyId,
      time_taken: timeTaken,
      taken,
    });
    return legacyResponse.data;
  }
};

// ==================== GUARDIAN API SERVICES ====================

export async function guardianLogin(username, password) {
  try {
    const response = await api.post('/guardian-login', { username, password });
    return { ok: true, data: response.data };
  } catch (error) {
    const legacyResponse = await fetch(`${LEGACY_API_BASE}/guardian-login`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username, password }),
    });
    return { ok: legacyResponse.ok, data: await legacyResponse.json() };
  }
}

export async function guardianRegister(userData) {
  try {
    const response = await api.post('/guardian-register', userData);
    return { ok: true, data: response.data };
  } catch (error) {
    const legacyResponse = await fetch(`${LEGACY_API_BASE}/guardian-register`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(userData),
    });
    return { ok: legacyResponse.ok, data: await legacyResponse.json() };
  }
}

export async function fetchLinkedElderly(guardianUsername) {
  try {
    const response = await api.get(`/guardian-elderly/${guardianUsername}`);
    return { ok: true, data: response.data };
  } catch (error) {
    const legacyResponse = await fetch(`${LEGACY_API_BASE}/guardian-elderly/${guardianUsername}`);
    return { ok: legacyResponse.ok, data: await legacyResponse.json() };
  }
}

export async function fetchMedicines(elderlyId) {
  try {
    const response = await api.get(`/medicines/${elderlyId}`);
    return { ok: true, data: response.data };
  } catch (error) {
    const legacyResponse = await fetch(`${LEGACY_API_BASE}/medicines/${elderlyId}`);
    return { ok: legacyResponse.ok, data: await legacyResponse.json() };
  }
}

export async function fetchElderlyInfo(elderlyId) {
  try {
    const response = await api.get(`/elderly-info/${elderlyId}`);
    return { ok: true, data: response.data };
  } catch (error) {
    const legacyResponse = await fetch(`${LEGACY_API_BASE}/elderly-info/${elderlyId}`);
    return { ok: legacyResponse.ok, data: await legacyResponse.json() };
  }
}

export async function fetchHardwareData(elderlyId) {
  try {
    const response = await api.get(`/hardware-data/${elderlyId}`);
    return { ok: true, data: response.data };
  } catch (error) {
    const legacyResponse = await fetch(`${LEGACY_API_BASE}/hardware-data/${elderlyId}`);
    return { ok: legacyResponse.ok, data: await legacyResponse.json() };
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
    const legacyResponse = await fetch(`${LEGACY_API_BASE}/medicine/add`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload),
    });
    return { ok: legacyResponse.ok, data: await legacyResponse.json() };
  }
}

export async function deleteMedicine(medicineId, elderlyId) {
  try {
    const response = await api.post(`/medicine/delete/${medicineId}`, { elderly_id: elderlyId });
    return { ok: true, data: response.data };
  } catch (error) {
    const legacyResponse = await fetch(`${LEGACY_API_BASE}/medicine/delete/${medicineId}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ elderly_id: elderlyId }),
    });
    return { ok: legacyResponse.ok, data: await legacyResponse.json() };
  }
}

export async function fetchSuggestions(elderlyId) {
  try {
    const response = await api.get(`/medicine/suggestions/${elderlyId}`);
    return { ok: true, data: response.data };
  } catch (error) {
    const legacyResponse = await fetch(`${LEGACY_API_BASE}/medicine/suggestions/${elderlyId}`);
    return { ok: legacyResponse.ok, data: await legacyResponse.json() };
  }
}

export async function saveSuggestions(elderlyId, notes) {
  const payload = {
    guardian_username: localStorage.getItem('guardian_username') || 'john_guardian',
    suggestion: notes,
    notes: notes
  };
  try {
    const response = await api.post(`/medicine/suggestions/${elderlyId}`, payload);
    return { ok: true, data: response.data };
  } catch (error) {
    const legacyResponse = await fetch(`${LEGACY_API_BASE}/medicine/suggestions/${elderlyId}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload),
    });
    return { ok: legacyResponse.ok, data: await legacyResponse.json() };
  }
}

export { API_BASE, LEGACY_API_BASE };
export default api;
