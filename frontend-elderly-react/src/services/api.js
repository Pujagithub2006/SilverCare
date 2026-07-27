import axios from 'axios';

const API_BASE = 'http://localhost:8080/api'; // Will connect to Spring Boot backend

// For now, keep the old Python backend URL for compatibility
const LEGACY_API_BASE = 'http://127.0.0.1:5001';

const api = axios.create({
  baseURL: API_BASE,
  headers: {
    'Content-Type': 'application/json',
  },
});

// Elderly Authentication
export const elderlyLogin = async (name, phone) => {
  try {
    // Try Spring Boot first
    const response = await api.post('/elderly/login', { name, phone });
    return response.data;
  } catch (error) {
    // Fallback to legacy Python backend
    console.log('Falling back to legacy backend');
    const legacyResponse = await axios.post(`${LEGACY_API_BASE}/elderly-login`, { name, phone });
    return legacyResponse.data;
  }
};

export const elderlyRegister = async (elderlyData) => {
  try {
    const response = await api.post('/elderly/register', elderlyData);
    return response.data;
  } catch (error) {
    // Fallback to legacy
    const legacyResponse = await axios.post(`${LEGACY_API_BASE}/elderly-register`, elderlyData);
    return legacyResponse.data;
  }
};

// Elderly Session Management
export const registerElderlySession = async (elderlyId, deviceInfo) => {
  try {
    const response = await api.post('/elderly/session/register', { elderly_id: elderlyId, device_info: deviceInfo });
    return response.data;
  } catch (error) {
    const legacyResponse = await axios.post(`${LEGACY_API_BASE}/elderly/register-session`, { elderly_id: elderlyId, device_info: deviceInfo });
    return legacyResponse.data;
  }
};

export const unregisterElderlySession = async (elderlyId) => {
  try {
    const response = await api.post('/elderly/session/unregister', { elderly_id: elderlyId });
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
    const apiResponse = await api.post('/elderly/notification/clear', { elderly_id: elderlyId, medicine_id: medicineId, response });
    return apiResponse.data;
  } catch (error) {
    const legacyResponse = await axios.post(`${LEGACY_API_BASE}/elderly/clear-notification`, { elderly_id: elderlyId, medicine_id: medicineId, response });
    return legacyResponse.data;
  }
};

// Fall Detection
export const notifyGuardianFall = async (payload) => {
  try {
    const response = await api.post('/fall/notify-guardian', payload);
    return response.data;
  } catch (error) {
    const legacyResponse = await axios.post(`${LEGACY_API_BASE}/notify-guardian-fall`, payload);
    return legacyResponse.data;
  }
};

export const triggerEmergency = async (payload) => {
  try {
    const response = await api.post('/emergency/trigger', payload);
    return response.data;
  } catch (error) {
    const legacyResponse = await axios.post(`${LEGACY_API_BASE}/api/emergency`, payload);
    return legacyResponse.data;
  }
};

export const confirmSafe = async (payload) => {
  try {
    const response = await api.post('/emergency/safe', payload);
    return response.data;
  } catch (error) {
    const legacyResponse = await axios.post(`${LEGACY_API_BASE}/api/response`, payload);
    return legacyResponse.data;
  }
};

// Medicine Management
export const getMedicines = async (elderlyId) => {
  try {
    const response = await api.get(`/medicines/${elderlyId}`);
    return response.data;
  } catch (error) {
    const legacyResponse = await axios.get(`${LEGACY_API_BASE}/medicines/${elderlyId}`);
    return legacyResponse.data;
  }
};

export default api;
