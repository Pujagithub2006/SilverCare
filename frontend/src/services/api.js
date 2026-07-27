const API_BASE = "http://127.0.0.1:5001";

export async function guardianLogin(username, password) {
  const response = await fetch(`${API_BASE}/guardian-login`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ username, password }),
  });
  return { ok: response.ok, data: await response.json() };
}

export async function guardianRegister(userData) {
  const response = await fetch(`${API_BASE}/guardian-register`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(userData),
  });
  return { ok: response.ok, data: await response.json() };
}

export async function fetchLinkedElderly(guardianUsername) {
  const response = await fetch(`${API_BASE}/guardian-elderly/${guardianUsername}`);
  return { ok: response.ok, data: await response.json() };
}

export async function fetchMedicines(elderlyId) {
  const response = await fetch(`${API_BASE}/medicines/${elderlyId}`);
  return { ok: response.ok, data: await response.json() };
}

export async function fetchElderlyInfo(elderlyId) {
  const response = await fetch(`${API_BASE}/elderly-info/${elderlyId}`);
  return { ok: response.ok, data: await response.json() };
}

export async function fetchHardwareData(elderlyId) {
  const response = await fetch(`${API_BASE}/hardware-data/${elderlyId}`);
  return { ok: response.ok, data: await response.json() };
}

export async function addMedicine(medicineData) {
  const response = await fetch(`${API_BASE}/medicine/add`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(medicineData),
  });
  return { ok: response.ok, data: await response.json() };
}

export async function deleteMedicine(medicineId, elderlyId) {
  const response = await fetch(`${API_BASE}/medicine/delete/${medicineId}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ elderly_id: elderlyId }),
  });
  return { ok: response.ok, data: await response.json() };
}

export async function fetchSuggestions(elderlyId) {
  const response = await fetch(`${API_BASE}/medicine/suggestions/${elderlyId}`);
  return { ok: response.ok, data: await response.json() };
}

export async function saveSuggestions(elderlyId, notes) {
  const response = await fetch(`${API_BASE}/medicine/suggestions/${elderlyId}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ notes }),
  });
  return { ok: response.ok, data: await response.json() };
}

export { API_BASE };
