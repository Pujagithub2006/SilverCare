import axios from 'axios';

const normalizeBaseUrl = (value) => (value || '').replace(/\/$/, '');

const resolveBaseUrl = (fallback) => {
  const configuredValue = import.meta.env?.VITE_API_BASE_URL || import.meta.env?.VITE_BACKEND_URL;
  return normalizeBaseUrl(configuredValue) || fallback;
};

// Backend Base URLs
const API_BASE = resolveBaseUrl('http://localhost:5001');
const LEGACY_API_BASE = resolveBaseUrl('http://127.0.0.1:5001');

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

// ==================== ELDERLY API SERVICES ====================

// Elderly Authentication
export const elderlyLogin = async (name, phone) => {
  try {
    const response = await api.post('/elderly/login', { name, phone });
    return response.data;
  } catch (error) {
    console.log('Falling back to legacy backend');
    const legacyResponse = await axios.post(`${LEGACY_API_BASE}/elderly-login`, { name, phone });
    return legacyResponse.data;
  }
};

export const elderlyRegister = async (elderlyData) => {
  try {
    const response = await api.post('/elderly-register', elderlyData);
    return response.data;
  } catch (error) {
    const legacyResponse = await axios.post(`${LEGACY_API_BASE}/elderly-register`, elderlyData);
    return legacyResponse.data;
  }
};

// Elderly Session Management
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

// Notifications
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

// Fall Detection & Emergency
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

// Medicine Management (Elderly)
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
  try {
    const response = await api.post('/medicine/add', medicineData);
    return { ok: true, data: response.data };
  } catch (error) {
    const legacyResponse = await fetch(`${LEGACY_API_BASE}/medicine/add`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(medicineData),
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
  try {
    const response = await api.post(`/medicine/suggestions/${elderlyId}`, { notes });
    return { ok: true, data: response.data };
  } catch (error) {
    const legacyResponse = await fetch(`${LEGACY_API_BASE}/medicine/suggestions/${elderlyId}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ notes }),
    });
    return { ok: legacyResponse.ok, data: await legacyResponse.json() };
  }
}

export { API_BASE, LEGACY_API_BASE };
export default api;
