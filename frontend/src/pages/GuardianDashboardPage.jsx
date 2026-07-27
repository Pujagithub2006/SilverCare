import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import {
  fetchLinkedElderly,
  fetchMedicines,
  fetchElderlyInfo,
  fetchHardwareData,
  addMedicine,
  deleteMedicine,
  fetchSuggestions,
  saveSuggestions,
  API_BASE
} from '../services/api';

export default function GuardianDashboardPage() {
  const navigate = useNavigate();
  const guardianUsername = localStorage.getItem('guardian_username') || 'isha';
  const guardianName = localStorage.getItem('guardian_name') || 'Guardian';

  const [elderlyList, setElderlyList] = useState([]);
  const [selectedElderly, setSelectedElderly] = useState(null);
  const [loading, setLoading] = useState(true);

  // Elderly Details & Telemetry
  const [elderlyInfo, setElderlyInfo] = useState(null);
  const [hardwareData, setHardwareData] = useState({
    heartRate: 75,
    oxygenLevel: 98,
    temperature: 36.8,
    beltConnected: true,
    fallStatus: 'normal'
  });
  const [medicines, setMedicines] = useState([]);
  const [suggestions, setSuggestions] = useState([]);
  const [newSuggestionText, setNewSuggestionText] = useState('');

  // Modal State
  const [showAddModal, setShowAddModal] = useState(false);
  const [showLinkModal, setShowLinkModal] = useState(false);
  const [medName, setMedName] = useState('');
  const [medDosage, setMedDosage] = useState('');
  const [medTimes, setMedTimes] = useState(['08:00']);
  const [medStartDate, setMedStartDate] = useState(new Date().toISOString().split('T')[0]);
  const [medEndDate, setMedEndDate] = useState('');
  const [medInstructions, setMedInstructions] = useState('');
  const [submittingMed, setSubmittingMed] = useState(false);

  useEffect(() => {
    loadLinkedElderly();
  }, []);

  useEffect(() => {
    if (selectedElderly) {
      loadElderlyData(selectedElderly.id || selectedElderly.elderly_id);
    }
  }, [selectedElderly]);

  const loadLinkedElderly = async () => {
    setLoading(true);
    try {
      console.log('🔍 Fetching linked elderly for guardian:', guardianUsername);
      const { ok, data } = await fetchLinkedElderly(guardianUsername);
      if (ok && data.status === 'success' && data.elderly) {
        setElderlyList(data.elderly);
        if (data.elderly.length > 0) {
          setSelectedElderly(data.elderly[0]);
        }
      } else {
        setElderlyList([]);
      }
    } catch (err) {
      console.error('Error fetching linked elderly:', err);
    } finally {
      setLoading(false);
    }
  };

  const loadElderlyData = async (elderlyId) => {
    try {
      // Medicines
      const medRes = await fetchMedicines(elderlyId);
      if (medRes.ok && medRes.data.medicines) {
        setMedicines(medRes.data.medicines);
      } else {
        setMedicines([]);
      }

      // Info
      const infoRes = await fetchElderlyInfo(elderlyId);
      if (infoRes.ok && infoRes.data) {
        setElderlyInfo(infoRes.data);
      }

      // Hardware
      const hwRes = await fetchHardwareData(elderlyId);
      if (hwRes.ok && hwRes.data && hwRes.data.data) {
        setHardwareData(hwRes.data.data);
      }

      // Suggestions
      const sugRes = await fetchSuggestions(elderlyId);
      if (sugRes.ok && sugRes.data && sugRes.data.suggestions) {
        setSuggestions(sugRes.data.suggestions);
      }
    } catch (err) {
      console.error('Error loading elderly details:', err);
    }
  };

  const handleAddMedicineSubmit = async (e) => {
    e.preventDefault();
    if (!selectedElderly) return;
    const elderlyId = selectedElderly.id || selectedElderly.elderly_id;

    if (!medName || !medDosage || medTimes.length === 0 || !medStartDate) {
      alert('Please fill in all required fields.');
      return;
    }

    setSubmittingMed(true);
    try {
      const payload = {
        guardian_username: guardianUsername,
        elderly_id: elderlyId,
        medicine_name: medName,
        dosage: medDosage,
        times: medTimes,
        instructions: medInstructions,
        start_date: medStartDate,
        end_date: medEndDate || medStartDate
      };

      const { ok, data } = await addMedicine(payload);
      if (ok) {
        alert('Medicine added successfully!');
        setShowAddModal(false);
        setMedName('');
        setMedDosage('');
        setMedTimes(['08:00']);
        setMedInstructions('');
        loadElderlyData(elderlyId);
      } else {
        alert(`Error adding medicine: ${data.error || data.message || 'Failed'}`);
      }
    } catch (err) {
      console.error('Error adding medicine:', err);
      alert('Network error while adding medicine');
    } finally {
      setSubmittingMed(false);
    }
  };

  const handleDeleteMedicine = async (medicineId) => {
    if (!confirm('Are you sure you want to delete this medicine?')) return;
    if (!selectedElderly) return;
    const elderlyId = selectedElderly.id || selectedElderly.elderly_id;

    try {
      const { ok, data } = await deleteMedicine(medicineId, elderlyId);
      if (ok) {
        alert('Medicine deleted successfully!');
        loadElderlyData(elderlyId);
      } else {
        alert(`Error deleting medicine: ${data.error || data.message || 'Failed'}`);
      }
    } catch (err) {
      console.error('Error deleting medicine:', err);
      alert('Network error while deleting medicine');
    }
  };

  const handleAddSuggestion = async (e) => {
    e.preventDefault();
    if (!newSuggestionText.trim() || !selectedElderly) return;
    const elderlyId = selectedElderly.id || selectedElderly.elderly_id;

    try {
      const { ok, data } = await saveSuggestions(elderlyId, newSuggestionText.trim());
      if (ok) {
        alert('Suggestion submitted successfully!');
        setNewSuggestionText('');
        loadElderlyData(elderlyId);
      } else {
        alert(`Error submitting suggestion: ${data.error || data.message || 'Failed'}`);
      }
    } catch (err) {
      console.error('Error submitting suggestion:', err);
      alert('Network error');
    }
  };

  const addTimeField = () => {
    setMedTimes([...medTimes, '12:00']);
  };

  const updateTimeField = (index, value) => {
    const updated = [...medTimes];
    updated[index] = value;
    setMedTimes(updated);
  };

  const removeTimeField = (index) => {
    if (medTimes.length === 1) return;
    setMedTimes(medTimes.filter((_, i) => i !== index));
  };

  const logout = () => {
    localStorage.clear();
    navigate('/guardian-auth');
  };

  return (
    <div style={{ background: '#f5f5f7', minHeight: '100vh', fontFamily: "-apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif" }}>
      <style>{`
        .g-dashboard {
          max-width: 480px;
          margin: 0 auto;
          background: #ffffff;
          min-height: 100vh;
          box-shadow: 0 0 20px rgba(0, 0, 0, 0.08);
          display: flex;
          flex-direction: column;
        }

        .g-header {
          padding: 16px 20px;
          background: #ffffff;
          border-bottom: 1px solid #e5e5e7;
          display: flex;
          justify-content: space-between;
          align-items: center;
          position: sticky;
          top: 0;
          z-index: 100;
        }

        .g-title h1 {
          font-size: 20px;
          font-weight: 700;
          color: #000;
          margin: 0;
        }

        .g-title p {
          font-size: 13px;
          color: #6c6c70;
          margin: 2px 0 0 0;
        }

        .g-btn-logout {
          background: #ff3b30;
          color: white;
          border: none;
          padding: 6px 14px;
          border-radius: 16px;
          font-size: 13px;
          font-weight: 600;
          cursor: pointer;
          transition: all 0.2s;
        }

        .g-btn-logout:hover {
          background: #d70015;
        }

        .g-section {
          padding: 20px;
          border-bottom: 1px solid #f0f0f2;
        }

        .g-section-title {
          font-size: 16px;
          font-weight: 700;
          color: #1c1c1e;
          margin-bottom: 12px;
          display: flex;
          justify-content: space-between;
          align-items: center;
        }

        .elderly-tabs {
          display: flex;
          gap: 10px;
          overflow-x: auto;
          padding-bottom: 5px;
        }

        .elderly-tab {
          padding: 10px 16px;
          border-radius: 12px;
          background: #f2f2f7;
          border: 1px solid #e5e5ea;
          cursor: pointer;
          white-space: nowrap;
          font-size: 14px;
          font-weight: 600;
          color: #3a3a3c;
          transition: all 0.2s;
        }

        .elderly-tab.active {
          background: #007AFF;
          color: white;
          border-color: #007AFF;
        }

        .vitals-grid {
          display: grid;
          grid-template-columns: 1fr 1fr;
          gap: 12px;
          margin-top: 12px;
        }

        .vital-card {
          background: #f8f9fa;
          border-radius: 12px;
          padding: 14px;
          border: 1px solid #e9ecef;
        }

        .vital-label {
          font-size: 12px;
          color: #6c6c70;
          font-weight: 600;
          text-transform: uppercase;
        }

        .vital-val {
          font-size: 22px;
          font-weight: 700;
          color: #007AFF;
          margin-top: 4px;
        }

        .med-card {
          background: #ffffff;
          border: 1px solid #e5e5ea;
          border-radius: 12px;
          padding: 14px;
          margin-bottom: 10px;
          display: flex;
          justify-content: space-between;
          align-items: center;
          box-shadow: 0 2px 6px rgba(0,0,0,0.03);
        }

        .med-info h4 {
          margin: 0;
          font-size: 16px;
          color: #1c1c1e;
        }

        .med-info p {
          margin: 4px 0 0 0;
          font-size: 13px;
          color: #6c6c70;
        }

        .btn-add {
          background: #007AFF;
          color: white;
          border: none;
          padding: 8px 14px;
          border-radius: 8px;
          font-size: 13px;
          font-weight: 600;
          cursor: pointer;
        }

        .btn-del {
          background: #ff3b30;
          color: white;
          border: none;
          padding: 6px 12px;
          border-radius: 6px;
          font-size: 12px;
          cursor: pointer;
        }

        .modal-overlay {
          position: fixed;
          top: 0; left: 0; right: 0; bottom: 0;
          background: rgba(0,0,0,0.5);
          display: flex;
          align-items: center;
          justify-content: center;
          z-index: 1000;
          padding: 20px;
        }

        .modal-body-card {
          background: white;
          border-radius: 16px;
          padding: 24px;
          max-width: 400px;
          width: 100%;
          max-height: 90vh;
          overflow-y: auto;
        }

        .modal-body-card h3 {
          margin-top: 0;
          color: #007AFF;
        }

        .form-row {
          margin-bottom: 12px;
        }

        .form-row label {
          display: block;
          font-size: 13px;
          font-weight: 600;
          color: #333;
          margin-bottom: 4px;
        }

        .form-row input, .form-row textarea {
          width: 100%;
          padding: 10px;
          border: 1px solid #d1d1d6;
          border-radius: 8px;
          font-size: 14px;
          box-sizing: border-box;
        }

        .time-row {
          display: flex;
          gap: 8px;
          margin-bottom: 6px;
        }
      `}</style>

      <div className="g-dashboard">
        {/* Header */}
        <header className="g-header">
          <div className="g-title">
            <h1>SilverCare Guardian</h1>
            <p>Welcome back, <strong>{guardianName}</strong> (@{guardianUsername})</p>
          </div>
          <button className="g-btn-logout" onClick={logout}>Logout</button>
        </header>

        {/* Linked Elderly Section */}
        <div className="g-section">
          <div className="g-section-title">
            <span>Linked Family Members</span>
            <button
              style={{ background: 'none', border: 'none', color: '#007AFF', cursor: 'pointer', fontSize: '13px', fontWeight: '600' }}
              onClick={() => setShowLinkModal(true)}
            >
              + Link Guide
            </button>
          </div>

          {loading ? (
            <p style={{ color: '#6c6c70', fontSize: '14px' }}>Loading elderly list...</p>
          ) : elderlyList.length === 0 ? (
            <div style={{ background: '#f8f9fa', padding: '20px', borderRadius: '12px', textAlign: 'center' }}>
              <p style={{ margin: 0, color: '#6c6c70', fontSize: '14px' }}>No elderly members linked yet.</p>
              <button
                className="btn-add"
                style={{ marginTop: '10px' }}
                onClick={() => setShowLinkModal(true)}
              >
                View Linking Instructions
              </button>
            </div>
          ) : (
            <div className="elderly-tabs">
              {elderlyList.map((eItem, idx) => {
                const eId = eItem.id || eItem.elderly_id;
                const isSelected = selectedElderly && (selectedElderly.id || selectedElderly.elderly_id) === eId;
                return (
                  <button
                    key={eId || idx}
                    className={`elderly-tab ${isSelected ? 'active' : ''}`}
                    onClick={() => setSelectedElderly(eItem)}
                  >
                    👴 {eItem.name || eItem.elderly_name || eId}
                  </button>
                );
              })}
            </div>
          )}
        </div>

        {selectedElderly && (
          <>
            {/* Health & Hardware Telemetry */}
            <div className="g-section">
              <div className="g-section-title">
                <span>📡 Live Vitals & Belt Telemetry</span>
                <span style={{ fontSize: '12px', color: hardwareData.beltConnected ? '#34c759' : '#ff3b30', fontWeight: 'bold' }}>
                  {hardwareData.beltConnected ? '● Connected' : '○ Disconnected'}
                </span>
              </div>

              <div className="vitals-grid">
                <div className="vital-card">
                  <div className="vital-label">❤️ Heart Rate</div>
                  <div className="vital-val">{hardwareData.heartRate || 75} <span style={{ fontSize: '12px', color: '#666' }}>BPM</span></div>
                </div>

                <div className="vital-card">
                  <div className="vital-label">🫁 SpO2 (Oxygen)</div>
                  <div className="vital-val">{hardwareData.oxygenLevel || 98} <span style={{ fontSize: '12px', color: '#666' }}>%</span></div>
                </div>

                <div className="vital-card">
                  <div className="vital-label">🌡️ Temperature</div>
                  <div className="vital-val">{hardwareData.temperature || 36.8} <span style={{ fontSize: '12px', color: '#666' }}>°C</span></div>
                </div>

                <div className="vital-card">
                  <div className="vital-label">🚨 Fall Status</div>
                  <div className="vital-val" style={{ fontSize: '16px', color: hardwareData.fallStatus === 'fall' ? '#ff3b30' : '#34c759' }}>
                    {hardwareData.fallStatus === 'fall' ? 'FALL DETECTED!' : 'Normal'}
                  </div>
                </div>
              </div>
            </div>

            {/* Medicine Management */}
            <div className="g-section">
              <div className="g-section-title">
                <span>💊 Medicine Schedule</span>
                <button className="btn-add" onClick={() => setShowAddModal(true)}>+ Add Medicine</button>
              </div>

              {medicines.length === 0 ? (
                <p style={{ color: '#6c6c70', fontSize: '14px' }}>No medicines scheduled for this user.</p>
              ) : (
                medicines.map((m, idx) => (
                  <div key={m.id || idx} className="med-card">
                    <div className="med-info">
                      <h4>{m.medicine_name || m.name}</h4>
                      <p>Dosage: <strong>{m.dosage}</strong></p>
                      <p>Time: <strong>{Array.isArray(m.times) ? m.times.join(', ') : m.times}</strong></p>
                      {m.instructions && <p style={{ fontStyle: 'italic', fontSize: '12px' }}>Note: {m.instructions}</p>}
                    </div>
                    <button className="btn-del" onClick={() => handleDeleteMedicine(m.id || m.medicine_name)}>
                      Delete
                    </button>
                  </div>
                ))
              )}
            </div>

            {/* Guardian Suggestions / Notes */}
            <div className="g-section">
              <div className="g-section-title">
                <span>📝 Notes & Instructions for Elderly</span>
              </div>

              <form onSubmit={handleAddSuggestion} style={{ display: 'flex', gap: '8px', marginBottom: '12px' }}>
                <input
                  type="text"
                  placeholder="Enter a care note or reminder..."
                  value={newSuggestionText}
                  onChange={(e) => setNewSuggestionText(e.target.value)}
                  style={{ flex: 1, padding: '10px', borderRadius: '8px', border: '1px solid #d1d1d6', fontSize: '14px' }}
                />
                <button className="btn-add" type="submit">Post</button>
              </form>

              {suggestions.length === 0 ? (
                <p style={{ color: '#6c6c70', fontSize: '13px' }}>No custom suggestions added yet.</p>
              ) : (
                suggestions.map((s, idx) => (
                  <div key={idx} style={{ background: '#f8f9fa', borderLeft: '3px solid #007AFF', padding: '10px 14px', borderRadius: '6px', marginBottom: '8px', fontSize: '14px' }}>
                    {typeof s === 'string' ? s : s.text || s.notes || JSON.stringify(s)}
                  </div>
                ))
              )}
            </div>
          </>
        )}

        {/* Modal: Add Medicine */}
        {showAddModal && (
          <div className="modal-overlay">
            <div className="modal-body-card">
              <h3>💊 Add Medicine Schedule</h3>

              <form onSubmit={handleAddMedicineSubmit}>
                <div className="form-row">
                  <label>Medicine Name *</label>
                  <input
                    type="text"
                    placeholder="e.g., Paracetamol 500mg"
                    value={medName}
                    onChange={(e) => setMedName(e.target.value)}
                    required
                  />
                </div>

                <div className="form-row">
                  <label>Dosage *</label>
                  <input
                    type="text"
                    placeholder="e.g., 1 tablet after food"
                    value={medDosage}
                    onChange={(e) => setMedDosage(e.target.value)}
                    required
                  />
                </div>

                <div className="form-row">
                  <label>Scheduled Times *</label>
                  {medTimes.map((timeVal, idx) => (
                    <div key={idx} className="time-row">
                      <input
                        type="time"
                        value={timeVal}
                        onChange={(e) => updateTimeField(idx, e.target.value)}
                        required
                      />
                      {medTimes.length > 1 && (
                        <button type="button" className="btn-del" onClick={() => removeTimeField(idx)}>✕</button>
                      )}
                    </div>
                  ))}
                  <button type="button" style={{ background: 'none', border: 'none', color: '#007AFF', cursor: 'pointer', fontSize: '13px', fontWeight: '600', marginTop: '4px' }} onClick={addTimeField}>
                    + Add Another Time
                  </button>
                </div>

                <div className="form-row">
                  <label>Start Date *</label>
                  <input
                    type="date"
                    value={medStartDate}
                    onChange={(e) => setMedStartDate(e.target.value)}
                    required
                  />
                </div>

                <div className="form-row">
                  <label>End Date</label>
                  <input
                    type="date"
                    value={medEndDate}
                    onChange={(e) => setMedEndDate(e.target.value)}
                  />
                </div>

                <div className="form-row">
                  <label>Special Instructions</label>
                  <textarea
                    rows={2}
                    placeholder="Take with warm water"
                    value={medInstructions}
                    onChange={(e) => setMedInstructions(e.target.value)}
                  />
                </div>

                <div style={{ display: 'flex', gap: '10px', marginTop: '16px' }}>
                  <button type="button" style={{ flex: 1, padding: '10px', background: '#e5e5ea', border: 'none', borderRadius: '8px', cursor: 'pointer', fontWeight: '600' }} onClick={() => setShowAddModal(false)}>
                    Cancel
                  </button>
                  <button type="submit" className="btn-add" style={{ flex: 1, padding: '10px' }} disabled={submittingMed}>
                    {submittingMed ? 'Saving...' : 'Save Medicine'}
                  </button>
                </div>
              </form>
            </div>
          </div>
        )}

        {/* Modal: Link Guide */}
        {showLinkModal && (
          <div className="modal-overlay">
            <div className="modal-body-card">
              <h3>🔗 How to Link Elderly Member</h3>
              <p style={{ fontSize: '14px', color: '#444', lineHeight: '1.5' }}>
                To link an elderly person to your guardian account:
              </p>
              <ol style={{ fontSize: '13px', color: '#555', paddingLeft: '20px', lineHeight: '1.6' }}>
                <li>Ask them to visit the Elderly Registration page.</li>
                <li>They will enter their name, age, and phone number.</li>
                <li>In the <strong>Guardian Connection</strong> section, they must enter your Guardian Username (<strong>{guardianUsername}</strong>) and password.</li>
                <li>Once submitted, their profile will appear here automatically!</li>
              </ol>
              <button
                className="btn-add"
                style={{ width: '100%', marginTop: '12px', padding: '10px' }}
                onClick={() => setShowLinkModal(false)}
              >
                Got it
              </button>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
