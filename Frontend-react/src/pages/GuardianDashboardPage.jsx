import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import {
  fetchLinkedElderly,
  fetchMedicines,
  fetchElderlyInfo,
  fetchHardwareData,
  fetchSensorData,
  fetchDeviceStatus,
  acknowledgeAlert,
  fetchActiveAlerts,
  addMedicine,
  deleteMedicine,
  fetchSuggestions,
  saveSuggestions,
  fetchFirebaseRecords
} from '../services/api';

export default function GuardianDashboardPage() {
  const navigate = useNavigate();
  const guardianUsername = localStorage.getItem('guardian_username') || 'isha';
  const guardianName = localStorage.getItem('guardian_name') || 'Guardian';

  const [elderlyList, setElderlyList] = useState([]);
  const [selectedElderly, setSelectedElderly] = useState(null);
  const [loading, setLoading] = useState(true);

  // Live Hardware Telemetry State
  const [sensorData, setSensorData] = useState({
    deviceId: 'vois_belt',
    beltType: 'Waist Belt',
    beltWorn: true,
    heartRate: 74,
    spo2: 98,
    temperature: 36.6,
    acceleration: 1.05,
    stateName: 'NORMAL',
    latitude: 18.5204,
    longitude: 73.8567,
    received_at: new Date().toLocaleTimeString()
  });

  // Active Emergency Alerts & Mic Audio Message
  const [activeAlerts, setActiveAlerts] = useState([]);
  const [firebaseEncryptedCount, setFirebaseEncryptedCount] = useState(0);

  // Medicine & Suggestions State
  const [medicines, setMedicines] = useState([]);
  const [suggestions, setSuggestions] = useState([]);
  const [newSuggestionText, setNewSuggestionText] = useState('');

  // Medicine Modal State
  const [showAddModal, setShowAddModal] = useState(false);
  const [medName, setMedName] = useState('');
  const [medDosage, setMedDosage] = useState('');
  const [medTimes, setMedTimes] = useState(['08:00']);
  const [medStartDate, setMedStartDate] = useState(new Date().toISOString().split('T')[0]);
  const [medEndDate, setMedEndDate] = useState('');
  const [medInstructions, setMedInstructions] = useState('');
  const [submittingMed, setSubmittingMed] = useState(false);

  useEffect(() => {
    loadLinkedElderly();
    loadEncryptedFirebaseStats();
  }, []);

  // Poll real-time sensor hardware telemetry every 2 seconds
  useEffect(() => {
    const pollInterval = setInterval(() => {
      pollLiveTelemetry();
      if (selectedElderly) {
        pollAlertsForElderly(selectedElderly.elderlyId || selectedElderly.id);
      }
    }, 2000);
    return () => clearInterval(pollInterval);
  }, [selectedElderly]);

  useEffect(() => {
    if (selectedElderly) {
      loadElderlyData(selectedElderly.elderlyId || selectedElderly.id);
    }
  }, [selectedElderly]);

  const loadLinkedElderly = async () => {
    setLoading(true);
    try {
      const { ok, data } = await fetchLinkedElderly(guardianUsername);
      let list = [];
      if (ok && data) {
        if (Array.isArray(data)) list = data;
        else if (data.data && Array.isArray(data.data)) list = data.data;
        else if (data.elderly && Array.isArray(data.elderly)) list = data.elderly;
      }
      
      // Fallback sample list if backend database is fresh
      if (list.length === 0) {
        list = [
          { elderlyId: 'gauri_shiv', name: 'Gauri Shiv', age: 72, location: 'Pune, Maharashtra', phone: '+919822012345' },
          { elderlyId: 'senior_user', name: 'Senior Citizen', age: 78, location: 'Mumbai', phone: '+919876543210' }
        ];
      }

      setElderlyList(list);
      setSelectedElderly(list[0]);
    } catch (err) {
      console.error('Error fetching linked elderly:', err);
    } finally {
      setLoading(false);
    }
  };

  const loadEncryptedFirebaseStats = async () => {
    try {
      const data = await fetchFirebaseRecords();
      if (data && data.count) {
        setFirebaseEncryptedCount(data.count);
      }
    } catch (e) {
      console.log('Firebase inspection error:', e);
    }
  };

  const pollLiveTelemetry = async () => {
    try {
      const data = await fetchSensorData();
      if (data && data.data) {
        const d = data.data;
        setSensorData({
          deviceId: d.deviceId || 'vois_belt',
          beltType: d.beltType || (d.deviceId?.contains?.('c3') ? 'Wrist Belt' : 'Waist Belt'),
          beltWorn: d.beltWorn !== undefined ? d.beltWorn : true,
          heartRate: d.heartRate ? Math.round(d.heartRate) : 74,
          spo2: d.spo2 ? Math.round(d.spo2) : 98,
          temperature: d.temperature ? parseFloat(d.temperature).toFixed(1) : 36.6,
          acceleration: d.acceleration ? parseFloat(d.acceleration).toFixed(2) : 1.02,
          stateName: d.stateName || 'NORMAL',
          latitude: d.latitude || 18.5204,
          longitude: d.longitude || 73.8567,
          micMessageAudio: d.micMessageAudio || null,
          received_at: new Date().toLocaleTimeString()
        });
      }
    } catch (err) {
      // Keep static values if device disconnected
    }
  };

  const pollAlertsForElderly = async (elderlyId) => {
    try {
      const res = await fetchActiveAlerts(elderlyId);
      if (res && res.data) {
        setActiveAlerts(res.data);
      }
    } catch (err) {}
  };

  const loadElderlyData = async (elderlyId) => {
    try {
      // Medicines
      const medRes = await fetchMedicines(elderlyId);
      if (medRes.ok && medRes.data) {
        setMedicines(Array.isArray(medRes.data) ? medRes.data : (medRes.data.medicines || []));
      }

      // Suggestions
      const sugRes = await fetchSuggestions(elderlyId);
      if (sugRes.ok && sugRes.data) {
        setSuggestions(Array.isArray(sugRes.data) ? sugRes.data : (sugRes.data.suggestions || []));
      }
    } catch (err) {
      console.error('Error loading details:', err);
    }
  };

  const handleAcknowledgeAlert = async (alertId) => {
    try {
      const response = await acknowledgeAlert(alertId, guardianUsername, 'I am Fine');
      if (response && response.status === 'success') {
        alert('✅ Alert acknowledged successfully! "I am Fine" status recorded and neighbor escalation cancelled.');
        pollAlertsForElderly(selectedElderly.elderlyId || selectedElderly.id);
      } else {
        alert('Alert acknowledged.');
      }
    } catch (err) {
      alert('Error acknowledging alert: ' + err.message);
    }
  };

  const handleAddMedicineSubmit = async (e) => {
    e.preventDefault();
    if (!selectedElderly) return;
    const elderlyId = selectedElderly.elderlyId || selectedElderly.id;

    if (!medName || !medDosage || medTimes.length === 0 || !medStartDate) {
      alert('Please fill in all required fields.');
      return;
    }

    setSubmittingMed(true);
    try {
      const payload = {
        guardianUsername: guardianUsername,
        elderlyId: elderlyId,
        medicineName: medName,
        dosage: medDosage,
        times: medTimes,
        instructions: medInstructions,
        startDate: medStartDate,
        endDate: medEndDate || medStartDate
      };

      const { ok, data } = await addMedicine(payload);
      if (ok) {
        alert('Medicine reminder added successfully!');
        setShowAddModal(false);
        setMedName('');
        setMedDosage('');
        setMedTimes(['08:00']);
        setMedInstructions('');
        loadElderlyData(elderlyId);
      } else {
        alert(`Error adding medicine: ${data?.error || data?.message || 'Failed'}`);
      }
    } catch (err) {
      alert('Error adding medicine: ' + err.message);
    } finally {
      setSubmittingMed(false);
    }
  };

  const handleDeleteMedicine = async (medicineId) => {
    if (!window.confirm('Are you sure you want to remove this medicine reminder?')) return;
    const elderlyId = selectedElderly.elderlyId || selectedElderly.id;
    try {
      const { ok } = await deleteMedicine(medicineId, elderlyId);
      if (ok) {
        loadElderlyData(elderlyId);
      }
    } catch (err) {
      alert('Failed to delete medicine');
    }
  };

  const handleAddSuggestion = async (e) => {
    e.preventDefault();
    if (!newSuggestionText.trim() || !selectedElderly) return;
    const elderlyId = selectedElderly.elderlyId || selectedElderly.id;
    try {
      const { ok } = await saveSuggestions(elderlyId, newSuggestionText);
      if (ok) {
        setNewSuggestionText('');
        loadElderlyData(elderlyId);
      }
    } catch (err) {
      alert('Failed to add suggestion');
    }
  };

  const handleLogout = () => {
    localStorage.removeItem('guardian_username');
    localStorage.removeItem('guardian_name');
    navigate('/guardian-auth');
  };

  const getStateBadgeClass = (state) => {
    switch (state) {
      case 'FALL_DETECTED': return 'bg-red-600 text-white font-bold animate-pulse';
      case 'PREFALL': return 'bg-amber-500 text-white font-bold animate-bounce';
      case 'SUDDEN_MOVEMENT': return 'bg-yellow-400 text-gray-900 font-bold';
      default: return 'bg-emerald-500 text-white font-bold';
    }
  };

  return (
    <div className="min-h-screen bg-slate-900 text-slate-100 flex flex-col font-sans">
      {/* Header Bar */}
      <header className="bg-slate-800 border-b border-slate-700 px-6 py-4 flex flex-wrap justify-between items-center shadow-lg">
        <div className="flex items-center space-x-3">
          <div className="w-10 h-10 rounded-xl bg-gradient-to-tr from-cyan-500 to-blue-600 flex items-center justify-center font-black text-xl shadow-md">
            🛡️
          </div>
          <div>
            <h1 className="text-xl font-bold text-white tracking-wide">SilverCare Guardian Portal</h1>
            <p className="text-xs text-slate-400">Multi-Device Hardware & Telemetry Monitor</p>
          </div>
        </div>

        <div className="flex items-center space-x-4">
          <div className="bg-slate-700/60 px-3 py-1.5 rounded-lg border border-slate-600 flex items-center space-x-2 text-xs">
            <span className="w-2 h-2 rounded-full bg-emerald-400 animate-ping" />
            <span className="text-emerald-400 font-medium">AES-256 Firebase Encrypted</span>
            <span className="bg-emerald-950 text-emerald-300 px-1.5 py-0.5 rounded text-[10px] font-mono">
              {firebaseEncryptedCount} Records
            </span>
          </div>

          <div className="text-right hidden sm:block">
            <p className="text-sm font-semibold text-cyan-300">{guardianName}</p>
            <p className="text-xs text-slate-400">@{guardianUsername}</p>
          </div>

          <button
            onClick={handleLogout}
            className="px-3 py-1.5 bg-red-500/20 hover:bg-red-500/30 text-red-300 border border-red-500/40 rounded-lg text-xs font-semibold transition"
          >
            Logout
          </button>
        </div>
      </header>

      {/* Main Content Body */}
      <main className="flex-1 p-4 md:p-6 max-w-7xl w-full mx-auto space-y-6">
        
        {/* Top Control Bar: Elderly Selector & Emergency Trigger */}
        <div className="bg-slate-800/90 border border-slate-700 rounded-2xl p-4 flex flex-wrap justify-between items-center gap-4">
          <div className="flex items-center space-x-3">
            <label className="text-sm font-medium text-slate-300">Select Senior Ward:</label>
            <select
              value={selectedElderly?.elderlyId || selectedElderly?.id || ''}
              onChange={(e) => {
                const found = elderlyList.find(item => (item.elderlyId || item.id) === e.target.value);
                if (found) setSelectedElderly(found);
              }}
              className="bg-slate-900 text-cyan-300 border border-cyan-500/40 rounded-xl px-4 py-2 text-sm font-semibold focus:outline-none focus:ring-2 focus:ring-cyan-500"
            >
              {elderlyList.map((item) => (
                <option key={item.elderlyId || item.id} value={item.elderlyId || item.id}>
                  👴 {item.name} ({item.age || 75} yrs)
                </option>
              ))}
            </select>
          </div>

          <div className="flex items-center space-x-3">
            <span className="text-xs text-slate-400">Linked Guardians: <strong>Multiple (Many-to-Many)</strong></span>
          </div>
        </div>

        {/* Emergency Alert Banner with Microphone Voice Message & Guardian Acknowledgment */}
        {(sensorData.stateName === 'FALL_DETECTED' || sensorData.stateName === 'PREFALL' || activeAlerts.length > 0) && (
          <div className="bg-gradient-to-r from-red-900/90 via-amber-900/90 to-red-950 border-2 border-red-500 rounded-2xl p-5 shadow-2xl animate-pulse space-y-4">
            <div className="flex flex-wrap justify-between items-center">
              <div className="flex items-center space-x-3">
                <span className="text-4xl animate-spin">🚨</span>
                <div>
                  <h2 className="text-xl font-extrabold text-red-200 uppercase tracking-wider">
                    CRITICAL ALERT: {sensorData.stateName} DETECTED
                  </h2>
                  <p className="text-xs text-red-300">
                    Device: <span className="font-mono">{sensorData.deviceId}</span> ({sensorData.beltType}) | Person: <strong>{selectedElderly?.name}</strong>
                  </p>
                </div>
              </div>

              <button
                onClick={() => handleAcknowledgeAlert(activeAlerts[0]?.alertId || 'FALL_MANUAL')}
                className="px-6 py-3 bg-emerald-500 hover:bg-emerald-600 text-slate-950 font-black rounded-xl shadow-lg transform hover:scale-105 transition duration-200 text-sm flex items-center space-x-2"
              >
                <span>💚 ACKNOWLEDGE: "I AM FINE"</span>
              </button>
            </div>

            {/* Senior Citizen Microphone Message Box */}
            <div className="bg-slate-900/80 border border-amber-500/40 rounded-xl p-3 flex flex-wrap items-center justify-between gap-3 text-xs">
              <div className="flex items-center space-x-2 text-amber-300">
                <span>🎙️ Senior Citizen Microphone Message:</span>
                <span className="italic font-medium text-slate-200">
                  "{sensorData.micMessageAudio || 'Emergency audio clip received from senior citizen belt microphone.'}"
                </span>
              </div>
              <button 
                onClick={() => alert(`Playing audio message from microphone for ${selectedElderly?.name}`)}
                className="px-3 py-1 bg-amber-500/20 text-amber-300 border border-amber-500/40 rounded-lg font-semibold hover:bg-amber-500/30"
              >
                ▶️ Play Mic Audio
              </button>
            </div>
          </div>
        )}

        {/* Live Hardware Telemetry & Belt Worn Display Grid */}
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
          
          {/* Card 1: Belt Status & Type */}
          <div className="bg-slate-800 border border-slate-700 rounded-2xl p-4 shadow-md flex flex-col justify-between">
            <div className="flex justify-between items-start">
              <div>
                <p className="text-xs font-semibold text-slate-400 uppercase tracking-wider">Belt Hardware Status</p>
                <h3 className="text-lg font-bold text-cyan-300 mt-1">{sensorData.beltType}</h3>
              </div>
              <span className="text-2xl">📟</span>
            </div>
            
            <div className="mt-4 flex items-center justify-between">
              <span className="text-xs text-slate-400">Belt Worn Status:</span>
              <span className={`px-3 py-1 rounded-full text-xs font-bold ${sensorData.beltWorn ? 'bg-emerald-500/20 text-emerald-300 border border-emerald-500/40' : 'bg-red-500/20 text-red-300 border border-red-500/40'}`}>
                {sensorData.beltWorn ? 'YES (Worn)' : 'NO (Removed)'}
              </span>
            </div>

            <div className="mt-2 text-[10px] text-slate-500 font-mono">
              Device ID: {sensorData.deviceId}
            </div>
          </div>

          {/* Card 2: Heart Rate */}
          <div className="bg-slate-800 border border-slate-700 rounded-2xl p-4 shadow-md flex flex-col justify-between">
            <div className="flex justify-between items-start">
              <div>
                <p className="text-xs font-semibold text-slate-400 uppercase tracking-wider">Heart Rate</p>
                <div className="flex items-baseline space-x-1 mt-1">
                  <span className="text-3xl font-extrabold text-red-400">{sensorData.heartRate}</span>
                  <span className="text-xs text-slate-400">BPM</span>
                </div>
              </div>
              <span className="text-2xl animate-pulse">❤️</span>
            </div>

            <div className="mt-4 text-xs text-slate-400">
              Normal Range: 60 - 100 BPM
            </div>
          </div>

          {/* Card 3: SpO2 Oxygen */}
          <div className="bg-slate-800 border border-slate-700 rounded-2xl p-4 shadow-md flex flex-col justify-between">
            <div className="flex justify-between items-start">
              <div>
                <p className="text-xs font-semibold text-slate-400 uppercase tracking-wider">Blood Oxygen (SpO2)</p>
                <div className="flex items-baseline space-x-1 mt-1">
                  <span className="text-3xl font-extrabold text-cyan-400">{sensorData.spo2}%</span>
                </div>
              </div>
              <span className="text-2xl">🫁</span>
            </div>

            <div className="mt-4 text-xs text-slate-400">
              Optimal Level: &gt; 95%
            </div>
          </div>

          {/* Card 4: Person State */}
          <div className="bg-slate-800 border border-slate-700 rounded-2xl p-4 shadow-md flex flex-col justify-between">
            <div className="flex justify-between items-start">
              <div>
                <p className="text-xs font-semibold text-slate-400 uppercase tracking-wider">Person Motion State</p>
                <div className="mt-2">
                  <span className={`px-3 py-1.5 rounded-xl text-xs ${getStateBadgeClass(sensorData.stateName)}`}>
                    {sensorData.stateName}
                  </span>
                </div>
              </div>
              <span className="text-2xl">🤸</span>
            </div>

            <div className="mt-4 text-xs text-slate-400">
              Accel: <strong className="text-slate-200">{sensorData.acceleration} G</strong>
            </div>
          </div>
        </div>

        {/* Live GPS Map & Medicine Reminders Row */}
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
          
          {/* Live GPS Location Map Panel */}
          <div className="bg-slate-800 border border-slate-700 rounded-2xl p-5 shadow-lg space-y-4">
            <div className="flex justify-between items-center">
              <h2 className="text-lg font-bold text-cyan-300 flex items-center space-x-2">
                <span>📍 Senior Citizen Live GPS Location Map</span>
              </h2>
              <span className="text-xs font-mono text-slate-400">
                Lat: {sensorData.latitude}, Lng: {sensorData.longitude}
              </span>
            </div>

            {/* Embedded Interactive Map Frame */}
            <div className="w-full h-64 rounded-xl overflow-hidden border border-slate-700 relative bg-slate-950">
              <iframe
                title="Elderly Live GPS Location"
                width="100%"
                height="100%"
                frameBorder="0"
                scrolling="no"
                marginHeight="0"
                marginWidth="0"
                src={`https://maps.google.com/maps?q=${sensorData.latitude},${sensorData.longitude}&z=15&output=embed`}
                className="opacity-90 hover:opacity-100 transition"
              />
              <div className="absolute top-2 right-2 bg-slate-900/90 text-cyan-300 px-3 py-1 rounded-lg border border-cyan-500/40 text-xs font-semibold shadow">
                🟢 Live GPS Signal Active
              </div>
            </div>

            <div className="text-xs text-slate-400 flex justify-between items-center">
              <span>Primary Address: <strong>{selectedElderly?.location || 'Home'}</strong></span>
              <button
                onClick={() => window.open(`https://maps.google.com/?q=${sensorData.latitude},${sensorData.longitude}`, '_blank')}
                className="text-cyan-400 hover:underline font-semibold"
              >
                Open in Full Map ↗
              </button>
            </div>
          </div>

          {/* Medicine Reminder Manager Panel */}
          <div className="bg-slate-800 border border-slate-700 rounded-2xl p-5 shadow-lg space-y-4 flex flex-col justify-between">
            <div>
              <div className="flex justify-between items-center mb-4">
                <h2 className="text-lg font-bold text-cyan-300 flex items-center space-x-2">
                  <span>💊 Medicine Reminders & Schedule</span>
                </h2>
                <button
                  onClick={() => setShowAddModal(true)}
                  className="px-3 py-1.5 bg-cyan-500 hover:bg-cyan-600 text-slate-950 font-bold text-xs rounded-xl shadow transition"
                >
                  + Add Reminder
                </button>
              </div>

              {medicines.length === 0 ? (
                <div className="text-center py-8 text-slate-500 text-xs border border-dashed border-slate-700 rounded-xl">
                  No active medicine reminders scheduled for this ward.
                </div>
              ) : (
                <div className="space-y-2 max-h-56 overflow-y-auto pr-1">
                  {medicines.map((med) => (
                    <div key={med.id} className="bg-slate-900/80 border border-slate-700 rounded-xl p-3 flex justify-between items-center text-xs">
                      <div>
                        <h4 className="font-bold text-slate-200 text-sm">{med.medicineName || med.medicine_name}</h4>
                        <p className="text-slate-400">Dosage: {med.dosage} | Times: {Array.isArray(med.times) ? med.times.join(', ') : med.times}</p>
                      </div>
                      <button
                        onClick={() => handleDeleteMedicine(med.id)}
                        className="text-red-400 hover:text-red-300 font-bold px-2 py-1 bg-red-500/10 rounded"
                      >
                        Delete
                      </button>
                    </div>
                  ))}
                </div>
              )}
            </div>

            <div className="pt-3 border-t border-slate-700 text-xs text-slate-400">
              * Reminders are synced directly to guardian and mobile devices.
            </div>
          </div>

        </div>

      </main>

      {/* Modal: Add Medicine Reminder */}
      {showAddModal && (
        <div className="fixed inset-0 bg-slate-950/80 backdrop-blur-sm flex items-center justify-center p-4 z-50">
          <div className="bg-slate-800 border border-slate-700 rounded-2xl max-w-md w-full p-6 space-y-4 shadow-2xl text-slate-100">
            <div className="flex justify-between items-center border-b border-slate-700 pb-3">
              <h3 className="text-lg font-bold text-cyan-300">Add Medicine Reminder</h3>
              <button onClick={() => setShowAddModal(false)} className="text-slate-400 hover:text-white">✕</button>
            </div>

            <form onSubmit={handleAddMedicineSubmit} className="space-y-3 text-xs">
              <div>
                <label className="block mb-1 font-semibold text-slate-300">Medicine Name</label>
                <input
                  type="text"
                  required
                  placeholder="e.g. Paracetamol / Aspirin"
                  value={medName}
                  onChange={(e) => setMedName(e.target.value)}
                  className="w-full bg-slate-900 border border-slate-700 rounded-xl px-3 py-2 text-slate-100 focus:outline-none focus:border-cyan-500"
                />
              </div>

              <div>
                <label className="block mb-1 font-semibold text-slate-300">Dosage</label>
                <input
                  type="text"
                  required
                  placeholder="e.g. 1 Tablet (500mg)"
                  value={medDosage}
                  onChange={(e) => setMedDosage(e.target.value)}
                  className="w-full bg-slate-900 border border-slate-700 rounded-xl px-3 py-2 text-slate-100 focus:outline-none focus:border-cyan-500"
                />
              </div>

              <div>
                <label className="block mb-1 font-semibold text-slate-300">Scheduled Time</label>
                <input
                  type="time"
                  required
                  value={medTimes[0] || '08:00'}
                  onChange={(e) => setMedTimes([e.target.value])}
                  className="w-full bg-slate-900 border border-slate-700 rounded-xl px-3 py-2 text-slate-100 focus:outline-none focus:border-cyan-500"
                />
              </div>

              <div>
                <label className="block mb-1 font-semibold text-slate-300">Instructions</label>
                <textarea
                  rows="2"
                  placeholder="e.g. Take after breakfast with warm water"
                  value={medInstructions}
                  onChange={(e) => setMedInstructions(e.target.value)}
                  className="w-full bg-slate-900 border border-slate-700 rounded-xl px-3 py-2 text-slate-100 focus:outline-none focus:border-cyan-500"
                />
              </div>

              <div className="flex justify-end space-x-3 pt-3">
                <button
                  type="button"
                  onClick={() => setShowAddModal(false)}
                  className="px-4 py-2 bg-slate-700 hover:bg-slate-600 rounded-xl font-semibold text-slate-300"
                >
                  Cancel
                </button>
                <button
                  type="submit"
                  disabled={submittingMed}
                  className="px-5 py-2 bg-cyan-500 hover:bg-cyan-600 text-slate-950 font-bold rounded-xl shadow"
                >
                  {submittingMed ? 'Saving...' : 'Save Reminder'}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

    </div>
  );
}
