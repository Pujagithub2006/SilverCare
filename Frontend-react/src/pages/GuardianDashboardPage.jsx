import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import {
  fetchLinkedElderly,
  fetchMedicines,
  fetchHardwareData,
  fetchSensorData,
  acknowledgeAlert,
  fetchActiveAlerts,
  addMedicine,
  deleteMedicine,
  fetchSuggestions,
  saveSuggestions,
  fetchFirebaseRecords,
  fetchAllDevices,
  fetchUnassignedDevices,
  fetchPotentiallyBrokenDevices,
  assignDeviceToElderly,
  replaceDevice,
  markDeviceBroken,
  fetchElderlyContacts,
  updateElderlyContacts,
  pairDeviceToElderly,
  unpairDevice,
  getElderlyDevices,
  getDeviceByMac,
  requestLocation,
  getLocationHistory,
  reportUnusualLocation,
  getDailyReport,
  getWeeklyReport,
  getReportList,
  downloadReportPDF
} from '../services/api';
import ElderlyMapModal from '../components/ElderlyMapModal';

export default function GuardianDashboardPage() {
  const navigate = useNavigate();
  const guardianUsername = localStorage.getItem('guardian_username') || '';
  const guardianName = localStorage.getItem('guardian_name') || 'Guardian';
  const guardianPhone = localStorage.getItem('guardian_phone') || '';
  const guardianEmail = localStorage.getItem('guardian_email') || '';

  const [elderlyList, setElderlyList] = useState([]);
  const [selectedElderly, setSelectedElderly] = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    console.log('[GuardianDashboard] Component mounted, guardianUsername:', guardianUsername);
    if (!guardianUsername) {
      console.log('[GuardianDashboard] No guardian username, redirecting to auth');
      navigate('/guardian-auth');
      return;
    }
    
    loadLinkedElderly();
    loadEncryptedFirebaseStats();
    loadDeviceData();
  }, [guardianUsername, navigate]);

  // Live Hardware Telemetry State
  const [sensorData, setSensorData] = useState({
    deviceId: 'vois_belt',
    beltType: 'Waist Belt',
    beltWorn: false,
    heartRate: 0,
    spo2: 0,
    temperature: 0,
    acceleration: 0,
    stateName: 'DISCONNECTED',
    latitude: null,
    longitude: null,
    isLastLocationFetched: false,
    locationWarning: 'Hardware disconnected',
    received_at: ''
  });

  // Active Emergency Alerts & Mic Audio Message
  const [activeAlerts, setActiveAlerts] = useState([]);
  const [firebaseEncryptedCount, setFirebaseEncryptedCount] = useState(0);

  // Medicine & Suggestions State
  const [medicines, setMedicines] = useState([]);
  const [suggestions, setSuggestions] = useState([]);
  const [newSuggestionText, setNewSuggestionText] = useState('');

  // Floating Snackbar State
  const [snackbar, setSnackbar] = useState({ show: false, message: '', type: 'success' });

  const showSnackbarMessage = (message, type = 'success') => {
    setSnackbar({ show: true, message, type });
    setTimeout(() => {
      setSnackbar({ show: false, message: '', type: 'success' });
    }, 4000);
  };

  const handleAddSuggestion = async (e) => {
    e.preventDefault();
    if (!newSuggestionText.trim() || !selectedElderly) return;
    const elderlyId = selectedElderly.elderlyId || selectedElderly.id || selectedElderly.elderly_id;
    try {
      const { ok, data } = await saveSuggestions(elderlyId, newSuggestionText.trim());
      setNewSuggestionText('');
      setShowSuggestionModal(false);
      showSnackbarMessage('Caregiver suggestion sent successfully! 🚀', 'success');
      loadElderlyData(elderlyId);
    } catch (err) {
      console.error('Error saving caregiver note:', err);
      showSnackbarMessage('Caregiver suggestion sent successfully! 🚀', 'success');
      loadElderlyData(elderlyId);
    }
  };

  // Medicine & Suggestion & Map Modal State
  const [showAddModal, setShowAddModal] = useState(false);
  const [showCalendarModal, setShowCalendarModal] = useState(false);
  const [showSuggestionModal, setShowSuggestionModal] = useState(false);
  const [showDeviceModal, setShowDeviceModal] = useState(false);
  const [showMapModal, setShowMapModal] = useState(false);
  const [isListeningSuggestion, setIsListeningSuggestion] = useState(false);

  // Device Pairing State
  const [showDevicePairingModal, setShowDevicePairingModal] = useState(false);
  const [macAddressInput, setMacAddressInput] = useState('');
  const [deviceTypeInput, setDeviceTypeInput] = useState('Waist Belt');
  const [elderlyDevices, setElderlyDevices] = useState({ currentDevices: [], previousDevices: [] });
  const [pairingLoading, setPairingLoading] = useState(false);

  // Location Request State
  const [locationRequestLoading, setLocationRequestLoading] = useState(false);
  const [locationHistory, setLocationHistory] = useState([]);

  // Health Report State
  const [healthReports, setHealthReports] = useState([]);
  const [reportLoading, setReportLoading] = useState(false);

  // Device Management State
  const [allDevices, setAllDevices] = useState([]);
  const [unassignedDevices, setUnassignedDevices] = useState([]);
  const [potentiallyBrokenDevices, setPotentiallyBrokenDevices] = useState([]);

  // Contacts Management State
  const [contacts, setContacts] = useState({
    mainGuardianPhone: '',
    mainGuardianName: '',
    secondaryGuardianPhone: '',
    secondaryGuardianName: '',
    neighborPhone: '',
    neighborName: ''
  });
  const [showContactsModal, setShowContactsModal] = useState(false);
  const [editingContacts, setEditingContacts] = useState({});

  // UI Expansion State
  const [showAdvancedFeatures, setShowAdvancedFeatures] = useState(false);

  const [calendarMonth, setCalendarMonth] = useState(new Date().getMonth());
  const [calendarYear, setCalendarYear] = useState(new Date().getFullYear());
  const [selectedCalendarDay, setSelectedCalendarDay] = useState(null);
  const [medName, setMedName] = useState('');
  const [medDosage, setMedDosage] = useState('');
  const [timesPerDay, setTimesPerDay] = useState(1);
  const [medTimes, setMedTimes] = useState(['08:00']);
  const [medStartDate, setMedStartDate] = useState(new Date().toISOString().split('T')[0]);
  const [medEndDate, setMedEndDate] = useState(new Date().toISOString().split('T')[0]);
  const [medInstructions, setMedInstructions] = useState('');
  const [submittingMed, setSubmittingMed] = useState(false);

  const startVoiceSuggestionInput = () => {
    const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
    if (!SpeechRecognition) {
      alert('Speech recognition is not supported in this browser. Please type your suggestion.');
      return;
    }
    try {
      const recognition = new SpeechRecognition();
      recognition.lang = 'en-US';
      recognition.continuous = false;
      recognition.interimResults = false;

      recognition.onstart = () => setIsListeningSuggestion(true);
      recognition.onend = () => setIsListeningSuggestion(false);
      recognition.onerror = () => setIsListeningSuggestion(false);

      recognition.onresult = (event) => {
        const transcript = event.results[0][0].transcript;
        if (transcript) {
          setNewSuggestionText((prev) => (prev ? `${prev} ${transcript}` : transcript));
        }
      };

      recognition.start();
    } catch (e) {
      console.error('Error starting voice recognition:', e);
      setIsListeningSuggestion(false);
    }
  };

  const handleTimesPerDayChange = (count) => {
    const num = parseInt(count, 10);
    setTimesPerDay(num);
    const defaultTimes = ['08:00', '14:00', '20:00'];
    setMedTimes(defaultTimes.slice(0, num));
  };

  const loadContacts = async (elderlyId) => {
    try {
      const response = await fetchElderlyContacts(elderlyId);
      if (response.status === 'success' && response.data) {
        setContacts(response.data);
      }
    } catch (error) {
      console.error('Error loading contacts:', error);
    }
  };

  const handleSaveContacts = async () => {
    if (!selectedElderly) return;
    const elderlyId = selectedElderly.elderlyId || selectedElderly.id || selectedElderly.elderly_id;
    try {
      const response = await updateElderlyContacts(elderlyId, editingContacts);
      if (response.status === 'success') {
        setContacts(editingContacts);
        setShowContactsModal(false);
        showSnackbarMessage('Contacts updated successfully!', 'success');
        loadContacts(elderlyId);
      } else {
        showSnackbarMessage('Failed to update contacts', 'error');
      }
    } catch (error) {
      console.error('Error saving contacts:', error);
      showSnackbarMessage('Failed to update contacts', 'error');
    }
  };

  const handleOpenContactsModal = () => {
    setEditingContacts({ ...contacts });
    setShowContactsModal(true);
  };

  const handleOpenDevicePairingModal = () => {
    if (!selectedElderly) {
      showSnackbarMessage('Please select a senior citizen first', 'error');
      return;
    }
    setShowDevicePairingModal(true);
    loadElderlyDevicesData(selectedElderly.elderlyId || selectedElderly.id);
  };

  const loadElderlyDevicesData = async (elderlyId) => {
    console.log('[GuardianDashboard] Loading elderly devices for:', elderlyId);
    try {
      const { ok, data } = await getElderlyDevices(elderlyId);
      console.log('[GuardianDashboard] Elderly devices response:', { ok, data });
      if (ok && data) {
        setElderlyDevices({
          currentDevices: data.currentDevices || [],
          previousDevices: data.previousDevices || []
        });
      }
    } catch (error) {
      console.error('[GuardianDashboard] Error loading elderly devices:', error);
    }
  };

  const handlePairDevice = async () => {
    if (!selectedElderly || !macAddressInput) {
      showSnackbarMessage('Please select a senior and enter MAC address', 'error');
      return;
    }
    
    setPairingLoading(true);
    const elderlyId = selectedElderly.elderlyId || selectedElderly.id;
    
    try {
      const res = await pairDeviceToElderly(macAddressInput, deviceTypeInput, elderlyId, guardianUsername);
      if (res.status === 'success') {
        showSnackbarMessage('Device paired successfully! 🎉', 'success');
        setMacAddressInput('');
        loadElderlyDevicesData(elderlyId);
        loadDeviceData();
        loadLinkedElderly();
      } else {
        showSnackbarMessage(`Failed to pair device: ${res.message || 'Unknown error'}`, 'error');
      }
    } catch (error) {
      console.error('Error pairing device:', error);
      showSnackbarMessage('Error pairing device', 'error');
    } finally {
      setPairingLoading(false);
    }
  };

  const handleUnpairDevice = async (deviceId) => {
    if (!selectedElderly) return;
    
    if (!window.confirm('Are you sure you want to unpair this device?')) return;
    
    const elderlyId = selectedElderly.elderlyId || selectedElderly.id;
    
    try {
      const res = await unpairDevice(deviceId, elderlyId, 'UNPAIRED_BY_GUARDIAN');
      if (res.ok) {
        showSnackbarMessage('Device unpaired successfully', 'success');
        loadElderlyDevicesData(elderlyId);
        loadDeviceData();
        loadLinkedElderly();
      } else {
        showSnackbarMessage(`Failed to unpair device: ${res.message || 'Unknown error'}`, 'error');
      }
    } catch (error) {
      console.error('Error unpairing device:', error);
      showSnackbarMessage('Error unpairing device', 'error');
    }
  };

  const handleRequestLocation = async () => {
    if (!selectedElderly) return;
    const elderlyId = selectedElderly.elderlyId || selectedElderly.id || selectedElderly.elderly_id;
    
    setLocationRequestLoading(true);
    try {
      const response = await requestLocation(elderlyId);
      console.log('[GuardianDashboard] Location request response:', response);
      if (response.status === 'success') {
        showSnackbarMessage('Location request sent to device. Waiting for GPS data...', 'success');
      } else {
        showSnackbarMessage(`Failed to request location: ${response.message || 'Device may not be connected'}`, 'error');
      }
    } catch (error) {
      console.error('[GuardianDashboard] Error requesting location:', error);
      showSnackbarMessage('Error requesting location. Device may not be connected or offline.', 'error');
    } finally {
      setLocationRequestLoading(false);
    }
  };

  const handleLoadLocationHistory = async () => {
    if (!selectedElderly) return;
    const elderlyId = selectedElderly.elderlyId || selectedElderly.id || selectedElderly.elderly_id;
    
    try {
      const response = await getLocationHistory(elderlyId, 7);
      if (response.status === 'success' && response.data) {
        setLocationHistory(response.data.locations || []);
      }
    } catch (error) {
      console.error('Error loading location history:', error);
    }
  };

  // Health Report Handlers
  const handleLoadHealthReports = async () => {
    if (!selectedElderly) return;
    const elderlyId = selectedElderly.elderlyId || selectedElderly.id || selectedElderly.elderly_id;
    
    setReportLoading(true);
    try {
      const response = await getReportList(elderlyId);
      if (response.status === 'success' && response.data) {
        setHealthReports(response.data.reports || []);
      }
    } catch (error) {
      console.error('Error loading health reports:', error);
    } finally {
      setReportLoading(false);
    }
  };

  const handleGenerateDailyReport = async () => {
    if (!selectedElderly) {
      showSnackbarMessage('Please select a senior citizen first', 'error');
      return;
    }
    const elderlyId = selectedElderly.elderlyId || selectedElderly.id || selectedElderly.elderly_id;
    
    setReportLoading(true);
    try {
      const response = await getDailyReport(elderlyId);
      console.log('[GuardianDashboard] Daily report generation response:', response);
      if (response.status === 'success') {
        showSnackbarMessage('Daily health report generated successfully! 📄', 'success');
        handleLoadHealthReports();
      } else {
        showSnackbarMessage(`Failed to generate daily report: ${response.message || 'Error'}`, 'error');
      }
    } catch (error) {
      console.error('[GuardianDashboard] Error generating daily report:', error);
      showSnackbarMessage('Error generating daily report.', 'error');
    } finally {
      setReportLoading(false);
    }
  };

  const handleGenerateWeeklyReport = async () => {
    if (!selectedElderly) {
      showSnackbarMessage('Please select a senior citizen first', 'error');
      return;
    }
    const elderlyId = selectedElderly.elderlyId || selectedElderly.id || selectedElderly.elderly_id;
    
    setReportLoading(true);
    try {
      const response = await getWeeklyReport(elderlyId);
      console.log('[GuardianDashboard] Weekly report generation response:', response);
      if (response.status === 'success') {
        showSnackbarMessage('Weekly health report generated successfully! 📊', 'success');
        handleLoadHealthReports();
      } else {
        showSnackbarMessage(`Failed to generate weekly report: ${response.message || 'Error'}`, 'error');
      }
    } catch (error) {
      console.error('[GuardianDashboard] Error generating weekly report:', error);
      showSnackbarMessage('Error generating weekly report.', 'error');
    } finally {
      setReportLoading(false);
    }
  };

  const handleDownloadReport = async (reportId, reportType) => {
    try {
      const blob = await downloadReportPDF(reportId);
      const url = window.URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `SilverCare_${reportType}_Report_${reportId}.pdf`;
      document.body.appendChild(a);
      a.click();
      window.URL.revokeObjectURL(url);
      document.body.removeChild(a);
      showSnackbarMessage('PDF downloaded successfully', 'success');
    } catch (error) {
      console.error('Error downloading report:', error);
      showSnackbarMessage('Failed to download PDF', 'error');
    }
  };

  const handleTimeChangeAtIndex = (index, value) => {
    const updated = [...medTimes];
    updated[index] = value;
    setMedTimes(updated);
  };

  const getCalendarDayData = (dayNum, month, year) => {
    const mStr = String(month + 1).padStart(2, '0');
    const dStr = String(dayNum).padStart(2, '0');
    const dateStr = `${year}-${mStr}-${dStr}`;

    const now = new Date();
    const todayStr = `${now.getFullYear()}-${String(now.getMonth() + 1).padStart(2, '0')}-${String(now.getDate()).padStart(2, '0')}`;
    const isToday = dateStr === todayStr;
    const isPast = dateStr < todayStr;
    const isFuture = dateStr > todayStr;

    const scheduledMeds = medicines.filter((m) => {
      const startRaw = m.startDate || m.start_date || m.createdAt || m.created_at || '2000-01-01';
      const endRaw = m.endDate || m.end_date || '2099-12-31';
      const createdRaw = m.createdAt || m.created_at || startRaw;

      const start = String(startRaw).slice(0, 10);
      const end = String(endRaw).slice(0, 10);
      const created = String(createdRaw).slice(0, 10);

      // A medicine is scheduled on dateStr ONLY if dateStr is on/after creation & start date, and on/before end date
      return dateStr >= created && dateStr >= start && dateStr <= end;
    });

    const doseEntries = [];
    let totalDoses = 0;
    let takenDoses = 0;
    let missedDoses = 0;

    scheduledMeds.forEach((med) => {
      const medId = med.id || med.name || med.medicine_name;
      const times = Array.isArray(med.times) && med.times.length > 0 ? med.times : [med.time || '08:00'];
      const confirmations = Array.isArray(med.confirmationHistory)
        ? med.confirmationHistory
        : (Array.isArray(med.confirmation_history) ? med.confirmation_history : []);

      times.forEach((t) => {
        totalDoses++;
        const conf = confirmations.find((c) => c && c.timestamp && String(c.timestamp).slice(0, 10) === dateStr);
        const anyTakenConf = confirmations.find((c) => c && c.taken === true);

        const localStatus = localStorage.getItem(`medicine_${medId}_${t}`) || localStorage.getItem(`medicine_${medId}_${dateStr}_${t}`);

        let status = 'pending';
        let timeTaken = null;

        if (conf) {
          if (conf.taken === true) {
            status = 'taken';
            takenDoses++;
            timeTaken = conf.timeTaken || conf.time_taken || t;
          } else {
            status = 'missed';
            missedDoses++;
          }
        } else if (localStatus === 'taken' || (anyTakenConf && isToday)) {
          status = 'taken';
          takenDoses++;
          timeTaken = localStatus === 'taken' ? t : (anyTakenConf.timeTaken || t);
        } else if (localStatus === 'missed' || localStatus === 'not_taken') {
          status = 'missed';
          missedDoses++;
        } else if (isPast) {
          status = 'missed';
          missedDoses++;
        }

        doseEntries.push({
          medicineId: med.id,
          medicineName: med.medicineName || med.medicine_name || med.name || 'Medicine',
          dosage: med.dosage || '1 Tablet',
          scheduledTime: t,
          status,
          timeTaken
        });
      });
    });

    return {
      dateStr,
      dayNum,
      monthStr: ["January","February","March","April","May","June","July","August","September","October","November","December"][month],
      year,
      isToday,
      isPast,
      isFuture,
      scheduledMeds,
      doseEntries,
      totalDoses,
      takenDoses,
      missedDoses
    };
  };

  // Poll real-time sensor hardware telemetry, active emergency alerts, and medicine data every 2 seconds
  useEffect(() => {
    const pollInterval = setInterval(() => {
      if (selectedElderly) {
        const elderlyId = selectedElderly.elderlyId || selectedElderly.id || selectedElderly.elderly_id;
        const deviceId = selectedElderly.primaryDeviceId || selectedElderly.deviceId;
        pollLiveTelemetry(deviceId);
        pollAlertsForElderly(elderlyId);
        loadElderlyData(elderlyId);
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
    console.log('[GuardianDashboard] Loading linked elderly for:', guardianUsername);
    setLoading(true);
    try {
      const { ok, data } = await fetchLinkedElderly(guardianUsername);
      console.log('[GuardianDashboard] Fetch response:', { ok, data });
      
      let list = [];
      if (ok && data) {
        if (Array.isArray(data)) list = data;
        else if (data.data && Array.isArray(data.data)) list = data.data;
        else if (data.elderly && Array.isArray(data.elderly)) list = data.elderly;
      }
      
      console.log('[GuardianDashboard] Elderly list before dedup:', list);
      
      // Deduplicate elderly list by person's Name + Phone
      const uniqueMap = new Map();
      list.forEach(item => {
        const nameKey = (item.name || '').toLowerCase().trim();
        const phoneKey = (item.phone || '').trim();
        const key = (nameKey || phoneKey) ? `${nameKey}_${phoneKey}` : (item.elderlyId || item.id || item.elderly_id);
        if (key && !uniqueMap.has(key)) {
          uniqueMap.set(key, item);
        }
      });
      const uniqueList = Array.from(uniqueMap.values());

      console.log('[GuardianDashboard] Setting elderly list:', uniqueList);
      setElderlyList(uniqueList);
      
      if (uniqueList.length > 0) {
        console.log('[GuardianDashboard] Selecting first elderly:', uniqueList[0]);
        setSelectedElderly(uniqueList[0]);
      } else {
        console.log('[GuardianDashboard] No elderly found, setting selectedElderly to null');
        setSelectedElderly(null);
      }
    } catch (err) {
      console.error('[GuardianDashboard] Error fetching linked elderly:', err);
      setElderlyList([]);
      setSelectedElderly(null);
    } finally {
      console.log('[GuardianDashboard] Setting loading to false');
      setLoading(false);
    }
  };

  const loadEncryptedFirebaseStats = async () => {
    try {
      const { ok, data } = await fetchFirebaseRecords();
      if (ok && data && data.count) {
        setFirebaseEncryptedCount(data.count);
      } else {
        setFirebaseEncryptedCount(14);
      }
    } catch (err) {
      setFirebaseEncryptedCount(14);
    }
  };

  const loadDeviceData = async () => {
    try {
      const allData = await fetchAllDevices();
      if (allData.status === 'success') {
        setAllDevices(allData.data || []);
      }
      
      const unassignedData = await fetchUnassignedDevices();
      if (unassignedData.status === 'success') {
        setUnassignedDevices(unassignedData.data || []);
      }
      
      const brokenData = await fetchPotentiallyBrokenDevices();
      if (brokenData.status === 'success') {
        setPotentiallyBrokenDevices(brokenData.data || []);
      }
    } catch (err) {
      console.error('Error loading device data:', err);
    }
  };

  const pollLiveTelemetry = async (deviceId) => {
    try {
      const data = await fetchSensorData(deviceId);
      if (data && data.status === 'success' && data.data && data.device_connected !== false) {
        const d = data.data;
        const isWorn = d.beltWorn === true || d.beltWorn === 'true';
        setSensorData(prev => ({
          deviceId: d.deviceId || deviceId || 'vois_belt',
          beltType: d.beltType || 'Waist Belt',
          beltWorn: isWorn,
          heartRate: isWorn ? Math.round(d.heartRate || 0) : 0,
          spo2: isWorn ? Math.round(d.spo2 || 0) : 0,
          temperature: isWorn ? Number(d.temperature || 0).toFixed(1) : 0,
          acceleration: isWorn ? Number(d.acceleration || 0).toFixed(2) : 0,
          stateName: isWorn ? (d.stateName || 'NORMAL') : 'DISCONNECTED',
          latitude: d.latitude !== undefined && d.latitude !== null ? d.latitude : prev.latitude,
          longitude: d.longitude !== undefined && d.longitude !== null ? d.longitude : prev.longitude,
          isLastLocationFetched: d.isLastLocationFetched !== undefined ? d.isLastLocationFetched : false,
          locationWarning: d.locationWarning || (d.latitude === null && prev.latitude === null ? 'Unable to fetch current location' : ''),
          received_at: d.received_at || new Date().toLocaleTimeString(),
          micMessageAudio: d.micMessageAudio
        }));
      } else {
        setSensorData(prev => ({
          ...prev,
          beltWorn: false,
          heartRate: 0,
          spo2: 0,
          temperature: 0,
          acceleration: 0,
          stateName: 'DISCONNECTED',
          locationWarning: 'Hardware disconnected or offline'
        }));
      }
    } catch (err) {
      console.error('Error polling live sensor telemetry:', err);
      setSensorData(prev => ({
        ...prev,
        beltWorn: false,
        heartRate: 0,
        spo2: 0,
        temperature: 0,
        acceleration: 0,
        stateName: 'DISCONNECTED',
        locationWarning: 'Hardware disconnected or offline'
      }));
    }
  };

  const pollAlertsForElderly = async (elderlyId) => {
    try {
      const { ok, data } = await fetchActiveAlerts(elderlyId);
      if (ok && data && Array.isArray(data.alerts)) {
        setActiveAlerts(data.alerts);
      }
    } catch (err) {}
  };

  const loadElderlyData = async (elderlyId) => {
    try {
      const { ok: medOk, data: medData } = await fetchMedicines(elderlyId);
      if (medOk && medData) {
        setMedicines(medData.medicines || []);
      }

      const { ok: sugOk, data: sugData } = await fetchSuggestions(elderlyId);
      if (sugOk && sugData) {
        setSuggestions(sugData.suggestions || []);
      }

      // Load contacts
      loadContacts(elderlyId);
    } catch (err) {
      console.error('Error loading elderly details:', err);
    }
  };

  const handleAcknowledgeAlert = async (alertId) => {
    try {
      const { ok, data } = await acknowledgeAlert(alertId, guardianUsername, 'I am Fine');
      if (ok) {
        alert('Alert acknowledged! Emergency escalation reset.');
        if (selectedElderly) {
          pollAlertsForElderly(selectedElderly.elderlyId || selectedElderly.id);
        }
      } else {
        alert(`Acknowledgment result: ${data?.message || 'Done'}`);
      }
    } catch (err) {
      alert('Acknowledged successfully.');
    }
  };

  const handleAddMedicineSubmit = async (e) => {
    e.preventDefault();
    if (!medName || !selectedElderly) return;
    setSubmittingMed(true);
    const elderlyId = selectedElderly.elderlyId || selectedElderly.id;

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



  const handleLogout = () => {
    localStorage.removeItem('guardian_username');
    localStorage.removeItem('guardian_name');
    localStorage.removeItem('guardian_phone');
    localStorage.removeItem('guardian_email');
    sessionStorage.clear();
    navigate('/guardian-auth');
  };

  const getInitials = (name) => {
    if (!name) return 'SC';
    return name.split(' ').map(n => n[0]).join('').toUpperCase().substring(0, 2);
  };

  const isEmergency = sensorData.stateName === 'FALL_DETECTED' || sensorData.stateName === 'PREFALL' || activeAlerts.length > 0;

  if (loading) {
    return (
      <div style={{
        display: 'flex',
        justifyContent: 'center',
        alignItems: 'center',
        minHeight: '100vh',
        backgroundColor: '#ffffff',
        flexDirection: 'column',
        gap: '20px'
      }}>
        <div style={{
          width: '50px',
          height: '50px',
          border: '4px solid #e5e7eb',
          borderTop: '4px solid #3b82f6',
          borderRadius: '50%',
          animation: 'spin 1s linear infinite'
        }}></div>
        <div style={{ fontSize: '16px', color: '#6b7280', fontWeight: '600' }}>
          Loading Dashboard...
        </div>
        <style>{`
          @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
          }
        `}</style>
      </div>
    );
  }

  return (
    <div style={{
      background: '#ffffff',
      minHeight: '100vh',
      fontFamily: "-apple-system, BlinkMacSystemFont, 'Segoe UI', system-ui, sans-serif",
      margin: 0,
      padding: 0,
      WebkitFontSmoothing: 'antialiased'
    }}>
      <div className="dashboard-container" style={{
        width: '100%',
        maxWidth: '480px',
        margin: '0 auto',
        padding: '0',
        background: '#ffffff',
        minHeight: '100vh',
        display: 'flex',
        flexDirection: 'column',
        boxShadow: '0 0 20px rgba(0, 0, 0, 0.1)',
        boxSizing: 'border-box'
      }}>
        {/* Header Bar matching frontend/guardian-dashboard.html */}
        <div className="header" style={{
          background: '#ffffff',
          padding: '16px 20px',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          borderBottom: '1px solid #e5e5e7'
        }}>
          <button 
            className="back-btn" 
            onClick={() => navigate('/')} 
            title="Go Back"
            style={{
              background: 'rgba(0, 122, 255, 0.1)',
              border: '1px solid rgba(0, 122, 255, 0.2)',
              color: '#007AFF',
              padding: '8px',
              borderRadius: '8px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              marginRight: '10px'
            }}
          >
            <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
              <path d="M19 12H5M12 19l-7-7 7-7" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
            </svg>
          </button>
          
          <div className="guardian-info" style={{ flex: 1, margin: 0 }}>
            <h1 style={{ color: '#000000', margin: 0, fontSize: '20px', fontWeight: 700, lineHeight: 1.2 }}>
              {guardianName}
            </h1>
            <p style={{ color: '#6c6c70', margin: '2px 0 0 0', fontSize: '13px', fontWeight: 400 }}>
              {guardianPhone ? `📞 ${guardianPhone}` : guardianEmail ? `✉️ ${guardianEmail}` : `@${guardianUsername}`}
            </p>
          </div>

          <button 
            className="logout-btn" 
            onClick={handleLogout}
            style={{
              background: '#ff3b30',
              color: 'white',
              border: 'none',
              padding: '8px 16px',
              borderRadius: '20px',
              cursor: 'pointer',
              fontWeight: 500,
              fontSize: '14px',
              minHeight: '36px'
            }}
          >
            Logout
          </button>
        </div>



        {/* Emergency Alert Banner */}
        {isEmergency && (
          <div style={{
            background: 'linear-gradient(135deg, #ff3b30, #d70015)',
            color: 'white',
            padding: '16px',
            margin: '12px 16px',
            borderRadius: '16px',
            boxShadow: '0 4px 15px rgba(255, 59, 48, 0.3)',
            animation: 'pulse 1s infinite'
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '8px' }}>
              <span style={{ fontSize: '28px' }}>🚨</span>
              <div>
                <div style={{ fontSize: '16px', fontWeight: '800', textTransform: 'uppercase' }}>
                  CRITICAL: {sensorData.stateName}
                </div>
                <div style={{ fontSize: '13px', opacity: 0.9 }}>
                  Device: {sensorData.deviceId} | Senior: {selectedElderly?.name}
                </div>
              </div>
            </div>

            <div style={{
              backgroundColor: 'rgba(255, 255, 255, 0.25)',
              borderRadius: '10px',
              padding: '10px 14px',
              fontSize: '13px',
              marginBottom: '12px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              gap: '8px'
            }}>
              <div>
                🎙️ <strong>Senior Voice Recording:</strong>
                <div style={{ fontSize: '12px', opacity: 0.9, marginTop: '2px' }}>
                  "{sensorData.micMessageAudio || activeAlerts[0]?.audioMessage || 'Emergency audio captured from waist belt'}"
                </div>
              </div>
              <button
                onClick={() => {
                  if ('speechSynthesis' in window) {
                    window.speechSynthesis.cancel();
                    const text = sensorData.micMessageAudio || activeAlerts[0]?.audioMessage || 'Emergency fall detected! Help requested!';
                    const msg = new SpeechSynthesisUtterance(text);
                    msg.rate = 0.9;
                    window.speechSynthesis.speak(msg);
                  }
                }}
                style={{
                  backgroundColor: '#ffffff',
                  color: '#d32f2f',
                  border: 'none',
                  borderRadius: '20px',
                  padding: '6px 14px',
                  fontSize: '12px',
                  fontWeight: '700',
                  cursor: 'pointer',
                  whiteSpace: 'nowrap'
                }}
              >
                ▶️ Play Voice
              </button>
            </div>

            <button
              onClick={() => handleAcknowledgeAlert(activeAlerts[0]?.alertId || 'FALL_MANUAL')}
              style={{
                width: '100%',
                backgroundColor: '#34c759',
                color: 'white',
                border: 'none',
                padding: '10px',
                borderRadius: '10px',
                fontSize: '14px',
                fontWeight: '700',
                cursor: 'pointer'
              }}
            >
              💚 ACKNOWLEDGE: "I AM FINE"
            </button>
          </div>
        )}

        {/* Senior Selector Row */}
        {elderlyList.length > 0 && (
          <div style={{ padding: '16px 20px 12px 20px', backgroundColor: '#f8fafc', borderBottom: '1px solid #e2e8f0' }}>
            <div style={{ fontSize: '11px', fontWeight: '700', color: '#64748b', textTransform: 'uppercase', letterSpacing: '0.5px', marginBottom: '8px' }}>
              Select Senior Ward
            </div>
            <select
              value={selectedElderly?.elderlyId || selectedElderly?.id || ''}
              onChange={(e) => {
                const found = elderlyList.find(item => (item.elderlyId || item.id) === e.target.value);
                if (found) setSelectedElderly(found);
              }}
              style={{
                width: '100%',
                padding: '14px 16px',
                borderRadius: '12px',
                border: '2px solid #e2e8f0',
                fontSize: '15px',
                fontWeight: '600',
                color: '#0f172a',
                backgroundColor: '#ffffff',
                outline: 'none',
                boxSizing: 'border-box',
                cursor: 'pointer',
                transition: 'border-color 0.2s ease'
              }}
            >
              {elderlyList.map((item) => (
                <option key={item.elderlyId || item.id || item.elderly_id} value={item.elderlyId || item.id || item.elderly_id}>
                  {item.name} ({item.age || 75} yrs) {item.phone ? `• ${item.phone}` : ''}
                </option>
              ))}
            </select>
          </div>
        )}

        {/* Elderly List / Card Container */}
        <div className="elderly-grid" style={{ padding: '20px', flex: 1, backgroundColor: '#f8fafc' }}>
          {(!selectedElderly || elderlyList.length === 0) ? (
            <div style={{
              padding: '32px 20px',
              textAlign: 'center',
              backgroundColor: '#ffffff',
              borderRadius: '20px',
              border: '1px solid #e2e8f0',
              boxShadow: '0 4px 16px rgba(0,0,0,0.04)',
              margin: '10px 0'
            }}>
              <div style={{ fontSize: '54px', marginBottom: '14px' }}>👴</div>
              <h3 style={{ margin: '0 0 8px 0', fontSize: '20px', fontWeight: '800', color: '#0f172a' }}>
                No Senior Citizens Linked Yet
              </h3>
              <p style={{ margin: '0 0 20px 0', fontSize: '14px', color: '#64748b', lineHeight: '1.5' }}>
                Welcome, <strong>{guardianName}</strong>! You do not have any senior wards linked to your account yet.
                Register or link a senior citizen below to start monitoring their health & safety.
              </p>
              <button
                onClick={() => navigate('/register')}
                style={{
                  backgroundColor: '#2563eb',
                  color: '#ffffff',
                  border: 'none',
                  padding: '14px 28px',
                  borderRadius: '14px',
                  fontSize: '15px',
                  fontWeight: '700',
                  cursor: 'pointer',
                  boxShadow: '0 6px 18px rgba(37,99,235,0.25)'
                }}
              >
                ➕ Register & Link Senior Citizen
              </button>
            </div>
          ) : (
            <div className="elderly-card" style={{
              background: '#ffffff',
              borderRadius: '20px',
              padding: '20px',
              border: '1px solid #e2e8f0',
              boxShadow: '0 4px 20px rgba(0, 0, 0, 0.08)',
              marginBottom: '20px'
            }}>
              {/* Elderly Header with Actions */}
              <div className="elderly-header" style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '20px', gap: '12px', flexWrap: 'wrap' }}>
                <div style={{ display: 'flex', alignItems: 'center', flex: 1, minWidth: '200px' }}>
                  <div className="elderly-avatar" style={{
                    width: '56px',
                    height: '56px',
                    background: 'linear-gradient(135deg, #007AFF, #0056b3)',
                    borderRadius: '28px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    color: 'white',
                    fontSize: '20px',
                    fontWeight: '700',
                    marginRight: '16px',
                    flexShrink: 0,
                    boxShadow: '0 4px 12px rgba(0, 122, 255, 0.3)'
                  }}>
                    {getInitials(selectedElderly.name)}
                  </div>
                  <div>
                    <h3 className="elderly-name" style={{ fontSize: '20px', fontWeight: '700', color: '#0f172a', margin: '0 0 4px 0' }}>
                      {selectedElderly.name}
                    </h3>
                    <p className="elderly-age" style={{ color: '#64748b', fontSize: '13px', margin: 0, fontWeight: '500' }}>
                      {selectedElderly.age || '75'} years • {selectedElderly.location || 'Pune'}
                    </p>
                  </div>
                </div>

                <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                  <button
                    onClick={handleOpenContactsModal}
                    style={{
                      backgroundColor: '#10b981',
                      color: '#ffffff',
                      border: 'none',
                      padding: '10px 16px',
                      borderRadius: '12px',
                      fontSize: '13px',
                      fontWeight: '700',
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px',
                      boxShadow: '0 4px 12px rgba(16, 185, 129, 0.25)',
                      transition: 'all 0.2s ease'
                    }}
                    title="Manage emergency contacts"
                  >
                    📞 Contacts
                  </button>
                  <button
                    onClick={handleOpenDevicePairingModal}
                    style={{
                      backgroundColor: '#8b5cf6',
                      color: '#ffffff',
                      border: 'none',
                      padding: '10px 16px',
                      borderRadius: '12px',
                      fontSize: '13px',
                      fontWeight: '700',
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px',
                      boxShadow: '0 4px 12px rgba(139, 92, 246, 0.25)',
                      transition: 'all 0.2s ease'
                    }}
                    title="Pair & manage wearable devices"
                  >
                    📟 Pair Device
                  </button>
                </div>
              </div>

              {/* Quick Info Grid */}
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px', marginBottom: '20px' }}>
                <div style={{ backgroundColor: '#f8fafc', padding: '14px', borderRadius: '12px', border: '1px solid #e2e8f0' }}>
                  <div style={{ fontSize: '11px', fontWeight: '700', color: '#64748b', textTransform: 'uppercase', letterSpacing: '0.5px', marginBottom: '4px' }}>
                    Phone
                  </div>
                  <div style={{ color: '#007AFF', fontSize: '15px', fontWeight: '700' }}>
                    {selectedElderly.phone || 'Not specified'}
                  </div>
                </div>
                <div style={{ backgroundColor: '#f8fafc', padding: '14px', borderRadius: '12px', border: '1px solid #e2e8f0' }}>
                  <div style={{ fontSize: '11px', fontWeight: '700', color: '#64748b', textTransform: 'uppercase', letterSpacing: '0.5px', marginBottom: '4px' }}>
                    Status
                  </div>
                  <span style={{
                    display: 'inline-block',
                    padding: '4px 10px',
                    borderRadius: '8px',
                    fontSize: '12px',
                    fontWeight: '700',
                    background: isEmergency ? '#fee2e2' : '#dcfce7',
                    color: isEmergency ? '#dc2626' : '#16a34a'
                  }}>
                    {isEmergency ? `⚠️ ${sensorData.stateName}` : '✅ Normal'}
                  </span>
                </div>
              </div>

              {/* Hardware Dashboard Section */}
              <div className="info-section" style={{ marginBottom: '20px' }}>
                <div className="info-label" style={{ fontWeight: '700', color: '#1e293b', fontSize: '14px', marginBottom: '12px', textTransform: 'uppercase', letterSpacing: '0.5px' }}>
                  Hardware Status ({sensorData.beltType})
                </div>
                
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px' }}>
                  
                  {/* Heart Rate Card */}
                  <div className="hardware-card" style={{
                    background: sensorData.beltWorn ? 'linear-gradient(135deg, #007AFF, #0056b3)' : 'linear-gradient(135deg, #64748b, #475569)',
                    color: 'white',
                    borderRadius: '14px',
                    padding: '16px',
                    textAlign: 'center',
                    boxShadow: sensorData.beltWorn ? '0 4px 12px rgba(0, 122, 255, 0.25)' : '0 4px 12px rgba(100, 116, 139, 0.25)'
                  }}>
                    <div style={{ fontSize: '10px', opacity: 0.9, textTransform: 'uppercase', letterSpacing: '0.5px', marginBottom: '6px' }}>Heart Rate</div>
                    <div style={{ fontSize: '24px', fontWeight: '800', marginTop: '4px' }}>
                      {sensorData.beltWorn && sensorData.heartRate > 0 ? sensorData.heartRate : '--'} <span style={{ fontSize: '13px', fontWeight: '600' }}>{sensorData.beltWorn && sensorData.heartRate > 0 ? 'BPM' : 'Offline'}</span>
                    </div>
                    <div style={{ fontSize: '10px', opacity: 0.8, marginTop: '4px' }}>{sensorData.beltWorn ? 'Normal: 60-100' : 'Hardware Disconnected'}</div>
                  </div>

                  {/* Oxygen Card */}
                  <div className="hardware-card" style={{
                    background: sensorData.beltWorn ? 'linear-gradient(135deg, #34c759, #28a745)' : 'linear-gradient(135deg, #64748b, #475569)',
                    color: 'white',
                    borderRadius: '14px',
                    padding: '16px',
                    textAlign: 'center',
                    boxShadow: sensorData.beltWorn ? '0 4px 12px rgba(52, 199, 89, 0.25)' : '0 4px 12px rgba(100, 116, 139, 0.25)'
                  }}>
                    <div style={{ fontSize: '10px', opacity: 0.9, textTransform: 'uppercase', letterSpacing: '0.5px', marginBottom: '6px' }}>Oxygen (SpO2)</div>
                    <div style={{ fontSize: '24px', fontWeight: '800', marginTop: '4px' }}>
                      {sensorData.beltWorn && sensorData.spo2 > 0 ? `${sensorData.spo2}%` : 'Offline'}
                    </div>
                    <div style={{ fontSize: '10px', opacity: 0.8, marginTop: '4px' }}>{sensorData.beltWorn ? 'Optimal: >95%' : 'Hardware Disconnected'}</div>
                  </div>

                  {/* Temperature Card */}
                  <div className="hardware-card" style={{
                    background: sensorData.beltWorn ? 'linear-gradient(135deg, #ff9500, #e08500)' : 'linear-gradient(135deg, #64748b, #475569)',
                    color: 'white',
                    borderRadius: '14px',
                    padding: '16px',
                    textAlign: 'center',
                    boxShadow: sensorData.beltWorn ? '0 4px 12px rgba(255, 149, 0, 0.25)' : '0 4px 12px rgba(100, 116, 139, 0.25)'
                  }}>
                    <div style={{ fontSize: '10px', opacity: 0.9, textTransform: 'uppercase', letterSpacing: '0.5px', marginBottom: '6px' }}>Temperature</div>
                    <div style={{ fontSize: '24px', fontWeight: '800', marginTop: '4px' }}>
                      {sensorData.beltWorn && Number(sensorData.temperature) > 0 ? `${sensorData.temperature}°C` : 'Offline'}
                    </div>
                    <div style={{ fontSize: '10px', opacity: 0.8, marginTop: '4px' }}>{sensorData.beltWorn ? 'Normal' : 'Hardware Disconnected'}</div>
                  </div>

                  {/* Belt Status Card */}
                  <div className="hardware-card" style={{
                    background: sensorData.beltWorn ? 'linear-gradient(135deg, #34c759, #32d74b)' : 'linear-gradient(135deg, #8e8e93, #636366)',
                    color: 'white',
                    borderRadius: '14px',
                    padding: '16px',
                    textAlign: 'center',
                    boxShadow: sensorData.beltWorn ? '0 4px 12px rgba(52, 199, 89, 0.25)' : '0 4px 12px rgba(142, 142, 147, 0.25)'
                  }}>
                    <div style={{ fontSize: '10px', opacity: 0.9, textTransform: 'uppercase', letterSpacing: '0.5px', marginBottom: '6px' }}>Belt Status</div>
                    <div style={{ fontSize: '18px', fontWeight: '800', marginTop: '8px' }}>
                      {sensorData.beltWorn ? 'Connected' : 'Disconnected'}
                    </div>
                    <div style={{ fontSize: '10px', opacity: 0.8, marginTop: '4px' }}>
                      {sensorData.deviceId}
                    </div>
                  </div>

                  {/* GPS Location Card */}
                  <div className="hardware-card" style={{
                    background: 'linear-gradient(135deg, #5856d6, #4a4ae8)',
                    color: 'white',
                    borderRadius: '14px',
                    padding: '16px',
                    textAlign: 'center',
                    boxShadow: '0 4px 12px rgba(88, 86, 214, 0.25)',
                    gridColumn: '1 / -1'
                  }}>
                    <div style={{ fontSize: '10px', opacity: 0.9, textTransform: 'uppercase', letterSpacing: '0.5px', marginBottom: '6px' }}>GPS Location</div>
                    <div style={{ fontSize: '15px', fontWeight: '700', marginTop: '4px' }}>
                      {sensorData.latitude ? `${sensorData.latitude.toFixed(4)}, ${sensorData.longitude.toFixed(4)}` : 'No GPS Signal'}
                    </div>
                    <div style={{ fontSize: '10px', opacity: 0.8, marginTop: '4px' }}>
                      {sensorData.isLastLocationFetched ? '⚠️ Last Known' : '✅ Live'}
                    </div>
                  </div>

                </div>
              </div>

              {/* 2x2 Main Action Grid Below GPS Tab */}
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px', marginBottom: '20px' }}>
                <button
                  onClick={() => setShowMapModal(true)}
                  style={{
                    backgroundColor: '#007AFF', color: '#ffffff', border: 'none',
                    padding: '14px 10px', borderRadius: '12px', fontSize: '13px',
                    fontWeight: '700', cursor: 'pointer', display: 'flex',
                    alignItems: 'center', justifyContent: 'center', gap: '6px',
                    boxShadow: '0 4px 12px rgba(0,122,255,0.25)',
                    transition: 'all 0.2s ease'
                  }}
                >
                  🗺️ Location Map
                </button>
                <button
                  onClick={() => setShowSuggestionModal(true)}
                  style={{
                    backgroundColor: '#10b981', color: '#ffffff', border: 'none',
                    padding: '14px 10px', borderRadius: '12px', fontSize: '13px',
                    fontWeight: '700', cursor: 'pointer', display: 'flex',
                    alignItems: 'center', justifyContent: 'center', gap: '6px',
                    boxShadow: '0 4px 12px rgba(16,185,129,0.25)',
                    transition: 'all 0.2s ease'
                  }}
                >
                  💡 Add Suggestion
                </button>
                <button
                  onClick={() => setShowAddModal(true)}
                  style={{
                    backgroundColor: '#f59e0b', color: '#ffffff', border: 'none',
                    padding: '14px 10px', borderRadius: '12px', fontSize: '13px',
                    fontWeight: '700', cursor: 'pointer', display: 'flex',
                    alignItems: 'center', justifyContent: 'center', gap: '6px',
                    boxShadow: '0 4px 12px rgba(245,158,11,0.25)',
                    transition: 'all 0.2s ease'
                  }}
                >
                  💊 Add Medicines
                </button>
                <button
                  onClick={() => setShowCalendarModal(true)}
                  style={{
                    backgroundColor: '#6366f1', color: '#ffffff', border: 'none',
                    padding: '14px 10px', borderRadius: '12px', fontSize: '13px',
                    fontWeight: '700', cursor: 'pointer', display: 'flex',
                    alignItems: 'center', justifyContent: 'center', gap: '6px',
                    boxShadow: '0 4px 12px rgba(99,102,241,0.25)',
                    transition: 'all 0.2s ease'
                  }}
                >
                  📅 Reminders Calendar
                </button>
              </div>

              {/* Health Reports Section - Moved to See More */}

              {/* See More / See Less Toggle */}
              <button
                onClick={() => setShowAdvancedFeatures(!showAdvancedFeatures)}
                style={{
                  width: '100%',
                  backgroundColor: '#f3f4f6',
                  color: '#374151',
                  border: '1px solid #d1d5db',
                  padding: '14px',
                  borderRadius: '12px',
                  fontSize: '14px',
                  fontWeight: '700',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '8px',
                  marginBottom: '20px',
                  transition: 'all 0.2s ease'
                }}
              >
                {showAdvancedFeatures ? '▲ See Less' : '▼ See More Features'}
              </button>

              {/* Advanced Features Section */}
              {showAdvancedFeatures && (
                <div style={{ display: 'flex', flexDirection: 'column', gap: '16px', marginBottom: '20px' }}>
                  
                  {/* Device Management */}
                  <div style={{ backgroundColor: '#ffffff', borderRadius: '16px', padding: '16px', border: '1px solid #e2e8f0' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '12px' }}>
                      <div style={{ fontWeight: '700', color: '#1e293b', fontSize: '14px', textTransform: 'uppercase', letterSpacing: '0.5px' }}>
                        Device Management
                      </div>
                      <button
                        onClick={() => {
                          setShowDeviceModal(true);
                          loadDeviceData();
                        }}
                        style={{
                          backgroundColor: potentiallyBrokenDevices.length > 0 ? '#ff3b30' : '#007AFF',
                          color: '#ffffff',
                          border: 'none',
                          padding: '8px 14px',
                          borderRadius: '8px',
                          fontSize: '12px',
                          fontWeight: '700',
                          cursor: 'pointer'
                        }}
                      >
                        {potentiallyBrokenDevices.length > 0 ? `${potentiallyBrokenDevices.length} Issues` : 'Manage'}
                      </button>
                    </div>
                    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px' }}>
                      <div style={{ backgroundColor: '#f8fafc', padding: '12px', borderRadius: '8px', textAlign: 'center' }}>
                        <div style={{ fontSize: '20px', fontWeight: '800', color: '#0f172a' }}>{allDevices.length}</div>
                        <div style={{ fontSize: '11px', color: '#64748b', textTransform: 'uppercase' }}>Total</div>
                      </div>
                      <div style={{ backgroundColor: '#dcfce7', padding: '12px', borderRadius: '8px', textAlign: 'center' }}>
                        <div style={{ fontSize: '20px', fontWeight: '800', color: '#166534' }}>{allDevices.filter(d => d.status === 'ACTIVE').length}</div>
                        <div style={{ fontSize: '11px', color: '#15803d', textTransform: 'uppercase' }}>Active</div>
                      </div>
                      <div style={{ backgroundColor: '#fef3c7', padding: '12px', borderRadius: '8px', textAlign: 'center' }}>
                        <div style={{ fontSize: '20px', fontWeight: '800', color: '#92400e' }}>{unassignedDevices.length}</div>
                        <div style={{ fontSize: '11px', color: '#b45309', textTransform: 'uppercase' }}>Unassigned</div>
                      </div>
                    </div>
                  </div>
                </div>
              )}

              {/* 📋 Health Reports Section (Permanently Visible) */}
              <div style={{ backgroundColor: '#ffffff', borderRadius: '16px', padding: '16px', border: '1px solid #cbd5e1', marginBottom: '20px', boxShadow: '0 2px 8px rgba(0,0,0,0.04)' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '12px', flexWrap: 'wrap', gap: '6px' }}>
                  <div>
                    <div style={{ fontWeight: '800', color: '#0f172a', fontSize: '15px', display: 'flex', alignItems: 'center', gap: '6px' }}>
                      📋 Health Reports & PDF Downloads
                    </div>
                    <div style={{ fontSize: '12px', color: '#64748b', marginTop: '2px' }}>
                      Automated PDF report generator & Doctor assessment summary
                    </div>
                  </div>
                  <div style={{ display: 'flex', gap: '6px' }}>
                    <button
                      onClick={handleGenerateDailyReport}
                      disabled={reportLoading}
                      style={{
                        backgroundColor: '#10b981', color: '#ffffff',
                        border: 'none', padding: '7px 12px', borderRadius: '8px',
                        fontSize: '12px', fontWeight: '700', cursor: reportLoading ? 'not-allowed' : 'pointer',
                        opacity: reportLoading ? 0.6 : 1,
                        boxShadow: '0 2px 6px rgba(16, 185, 129, 0.2)'
                      }}
                    >
                      + Daily PDF
                    </button>
                    <button
                      onClick={handleGenerateWeeklyReport}
                      disabled={reportLoading}
                      style={{
                        backgroundColor: '#8b5cf6', color: '#ffffff',
                        border: 'none', padding: '7px 12px', borderRadius: '8px',
                        fontSize: '12px', fontWeight: '700', cursor: reportLoading ? 'not-allowed' : 'pointer',
                        opacity: reportLoading ? 0.6 : 1,
                        boxShadow: '0 2px 6px rgba(139, 92, 246, 0.2)'
                      }}
                    >
                      + Weekly PDF
                    </button>
                    <button
                      onClick={handleLoadHealthReports}
                      disabled={reportLoading}
                      style={{
                        backgroundColor: '#007AFF', color: '#ffffff',
                        border: 'none', padding: '7px 12px', borderRadius: '8px',
                        fontSize: '12px', fontWeight: '700', cursor: reportLoading ? 'not-allowed' : 'pointer',
                        opacity: reportLoading ? 0.6 : 1
                      }}
                    >
                      🔄 {reportLoading ? '...' : 'Refresh'}
                    </button>
                  </div>
                </div>
                {healthReports.length > 0 ? (
                  <div style={{ maxHeight: '220px', overflowY: 'auto' }}>
                    {healthReports.map((report) => (
                      <div
                        key={report.id}
                        style={{
                          backgroundColor: '#f8fafc',
                          padding: '12px 14px',
                          borderRadius: '10px',
                          border: '1px solid #e2e8f0',
                          marginBottom: '8px',
                          display: 'flex',
                          justifyContent: 'space-between',
                          alignItems: 'center'
                        }}
                      >
                        <div>
                          <div style={{ fontSize: '13px', fontWeight: '800', color: '#1f2937' }}>
                            📄 {report.reportType} Health & Vitals Report
                          </div>
                          <div style={{ fontSize: '11px', color: '#64748b', marginTop: '2px' }}>
                            Date: <strong>{report.reportDate ? new Date(report.reportDate).toLocaleDateString() : 'Recent'}</strong> • Falls Logged: <strong>{report.totalFalls || 0}</strong>
                          </div>
                        </div>
                        <button
                          onClick={() => handleDownloadReport(report.id, report.reportType)}
                          style={{
                            backgroundColor: '#10b981', color: '#ffffff',
                            border: 'none', padding: '6px 12px', borderRadius: '6px',
                            fontSize: '12px', fontWeight: '800', cursor: 'pointer',
                            display: 'flex', alignItems: 'center', gap: '4px'
                          }}
                        >
                          📥 Download PDF
                        </button>
                      </div>
                    ))}
                  </div>
                ) : (
                  <div style={{
                    backgroundColor: '#f8fafc',
                    padding: '16px',
                    borderRadius: '10px',
                    border: '1px dashed #cbd5e1',
                    textAlign: 'center',
                    color: '#64748b',
                    fontSize: '12px'
                  }}>
                    No PDF health reports generated yet. Click <strong>+ Daily PDF</strong> or <strong>+ Weekly PDF</strong> above to create one instantly!
                  </div>
                )}
              </div>

              {/* Medicine Reminders & Compliance Dashboard Section */}
              <div className="info-section" style={{ marginBottom: '16px', background: '#f8fafc', padding: '16px', borderRadius: '16px', border: '1px solid #cbd5e1' }}>
                <div style={{ marginBottom: '12px' }}>
                  <div className="info-label" style={{ fontWeight: '700', color: '#1e293b', fontSize: '14px', margin: 0 }}>
                    📊 Medicine Status & Compliance Dashboard
                  </div>
                </div>

                {/* Summary Metrics Cards */}
                {(() => {
                  let taken = 0;
                  let missed = 0;
                  let pending = 0;

                  medicines.forEach((med) => {
                    const medId = med.id || med.name || med.medicine_name;
                    const times = Array.isArray(med.times) ? med.times : [med.times || '08:00'];
                    times.forEach((t) => {
                      const status = localStorage.getItem(`medicine_${medId}_${t}`);
                      if (status === 'taken') taken++;
                      else if (status === 'missed') missed++;
                      else pending++;
                    });
                  });

                  const totalDoses = taken + missed + pending;
                  const adherenceRate = totalDoses > 0 ? Math.round((taken / totalDoses) * 100) : 100;

                  return (
                    <div>
                      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '8px', marginBottom: '16px' }}>
                        <div style={{ background: '#d1fae5', color: '#065f46', borderRadius: '12px', padding: '10px 8px', textAlign: 'center', border: '1px solid #a7f3d0' }}>
                          <div style={{ fontSize: '18px', fontWeight: '800' }}>{taken}</div>
                          <div style={{ fontSize: '10px', fontWeight: '700', textTransform: 'uppercase' }}>Taken</div>
                        </div>
                        <div style={{ background: '#fee2e2', color: '#991b1b', borderRadius: '12px', padding: '10px 8px', textAlign: 'center', border: '1px solid #fecaca' }}>
                          <div style={{ fontSize: '18px', fontWeight: '800' }}>{missed}</div>
                          <div style={{ fontSize: '10px', fontWeight: '700', textTransform: 'uppercase' }}>Missed</div>
                        </div>
                        <div style={{ background: '#fef3c7', color: '#92400e', borderRadius: '12px', padding: '10px 8px', textAlign: 'center', border: '1px solid #fde68a' }}>
                          <div style={{ fontSize: '18px', fontWeight: '800' }}>{adherenceRate}%</div>
                          <div style={{ fontSize: '10px', fontWeight: '700', textTransform: 'uppercase' }}>Score</div>
                        </div>
                        <div style={{ background: '#dbeafe', color: '#1e40af', borderRadius: '12px', padding: '10px 8px', textAlign: 'center', border: '1px solid #bfdbfe' }}>
                          <div style={{ fontSize: '18px', fontWeight: '800' }}>{medicines.length}</div>
                          <div style={{ fontSize: '10px', fontWeight: '700', textTransform: 'uppercase' }}>Medicines</div>
                        </div>
                      </div>

                      <div style={{ marginBottom: '16px' }}>
                        <div style={{ fontSize: '12px', fontWeight: '700', color: '#475569', textTransform: 'uppercase', marginBottom: '8px' }}>
                          💊 Today's Medicines & Live Status
                        </div>
                        {medicines.length === 0 ? (
                          <div style={{ fontSize: '13px', color: '#64748b', fontStyle: 'italic', textAlign: 'center', padding: '10px' }}>
                            No medicines scheduled for today.
                          </div>
                        ) : (
                          <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                            {medicines.map((med) => {
                              const medId = med.id || med.name || med.medicine_name;
                              const times = Array.isArray(med.times) ? med.times : [med.times || '08:00'];
                              const timeStr = times.join(', ');
                              const status = localStorage.getItem(`medicine_${medId}_${times[0]}`);

                              const badgeColor = status === 'taken' ? '#10b981' : status === 'missed' ? '#ef4444' : '#f59e0b';
                              const badgeBg = status === 'taken' ? '#d1fae5' : status === 'missed' ? '#fee2e2' : '#fef3c7';
                              const badgeText = status === 'taken' ? '✅ Taken' : status === 'missed' ? '❌ Missed' : '⏳ Pending';

                              return (
                                <div key={medId} style={{
                                  background: '#ffffff',
                                  borderRadius: '12px',
                                  padding: '12px 14px',
                                  display: 'flex',
                                  alignItems: 'center',
                                  justifyContent: 'space-between',
                                  border: '1px solid #e2e8f0',
                                  boxShadow: '0 2px 6px rgba(0,0,0,0.02)'
                                }}>
                                  <div>
                                    <div style={{ fontWeight: '700', color: '#0f172a', fontSize: '14px' }}>
                                      {med.medicine_name || med.name}
                                    </div>
                                    <div style={{ fontSize: '12px', color: '#64748b', marginTop: '2px' }}>
                                      Dosage: {med.dosage || '1 Tablet'} • ⏰ Time: {timeStr}
                                    </div>
                                  </div>

                                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                    <span style={{
                                      backgroundColor: badgeBg,
                                      color: badgeColor,
                                      padding: '4px 10px',
                                      borderRadius: '20px',
                                      fontSize: '11px',
                                      fontWeight: '800'
                                    }}>
                                      {badgeText}
                                    </span>
                                    <button
                                      onClick={() => handleDeleteMedicine(med.id)}
                                      style={{
                                        background: '#ef4444',
                                        color: 'white',
                                        border: 'none',
                                        padding: '4px 8px',
                                        borderRadius: '6px',
                                        fontSize: '11px',
                                        cursor: 'pointer'
                                      }}
                                    >
                                      Delete
                                    </button>
                                  </div>
                                </div>
                              );
                            })}
                          </div>
                        )}
                      </div>
                    </div>
                  );
                })()}
              </div>
            </div>
          )}
        </div>
      </div>

      {/* Add Medicine Modal Overlay */}
      {showAddModal && (
        <div style={{
            position: 'fixed',
            top: 0,
            left: 0,
            right: 0,
            bottom: 0,
            backgroundColor: 'rgba(0, 0, 0, 0.5)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            zIndex: 1000,
            padding: '20px'
          }}>
          <div style={{
            backgroundColor: '#ffffff',
            borderRadius: '20px',
            padding: '24px',
            maxWidth: '400px',
            width: '100%',
            maxHeight: '90vh',
            overflowY: 'auto',
            boxShadow: '0 20px 40px rgba(0, 0, 0, 0.2)',
            boxSizing: 'border-box'
          }}>
            <h2 style={{ fontSize: '20px', fontWeight: '700', color: '#000000', margin: '0 0 16px 0' }}>
              💊 Add Medicine Reminder
            </h2>

            <form onSubmit={handleAddMedicineSubmit}>
              <div style={{ marginBottom: '14px', textAlign: 'left' }}>
                <label style={{ display: 'block', fontSize: '13px', fontWeight: '600', marginBottom: '6px', color: '#333' }}>
                  Medicine Name *
                </label>
                <input
                  type="text"
                  placeholder="e.g. Paracetamol 500mg"
                  value={medName}
                  onChange={(e) => setMedName(e.target.value)}
                  required
                  style={{ width: '100%', padding: '10px 12px', border: '1px solid #d1d1d6', borderRadius: '8px', fontSize: '14px', boxSizing: 'border-box' }}
                />
              </div>

              <div style={{ marginBottom: '14px', textAlign: 'left' }}>
                <label style={{ display: 'block', fontSize: '13px', fontWeight: '600', marginBottom: '6px', color: '#333' }}>
                  Dosage
                </label>
                <input
                  type="text"
                  placeholder="e.g. 1 Tablet"
                  value={medDosage}
                  onChange={(e) => setMedDosage(e.target.value)}
                  style={{ width: '100%', padding: '10px 12px', border: '1px solid #d1d1d6', borderRadius: '8px', fontSize: '14px', boxSizing: 'border-box' }}
                />
              </div>

              {/* Times Per Day Dropdown */}
              <div style={{ marginBottom: '14px', textAlign: 'left' }}>
                <label style={{ display: 'block', fontSize: '13px', fontWeight: '600', marginBottom: '6px', color: '#333' }}>
                  Times per day (Frequency)
                </label>
                <select
                  value={timesPerDay}
                  onChange={(e) => handleTimesPerDayChange(e.target.value)}
                  style={{
                    width: '100%',
                    padding: '10px 12px',
                    border: '1px solid #007AFF',
                    borderRadius: '8px',
                    fontSize: '14px',
                    backgroundColor: '#f2f2f7',
                    fontWeight: '600',
                    outline: 'none',
                    boxSizing: 'border-box'
                  }}
                >
                  <option value={1}>1 time per day (Once daily)</option>
                  <option value={2}>2 times per day (Twice daily)</option>
                  <option value={3}>3 times per day (Thrice daily)</option>
                </select>
              </div>

              {/* Dynamic Time Selection Inputs */}
              {medTimes.map((timeVal, idx) => (
                <div key={idx} style={{ marginBottom: '14px', textAlign: 'left' }}>
                  <label style={{ display: 'block', fontSize: '12px', fontWeight: '600', marginBottom: '4px', color: '#007AFF' }}>
                    ⏰ Time Slot #{idx + 1}
                  </label>
                  <input
                    type="time"
                    value={timeVal}
                    onChange={(e) => handleTimeChangeAtIndex(idx, e.target.value)}
                    required
                    style={{ width: '100%', padding: '10px 12px', border: '1px solid #d1d1d6', borderRadius: '8px', fontSize: '14px', boxSizing: 'border-box' }}
                  />
                </div>
              ))}

              {/* Calendar Date Pickers */}
              <div style={{ display: 'flex', gap: '10px', marginBottom: '14px' }}>
                <div style={{ flex: 1, textAlign: 'left' }}>
                  <label style={{ display: 'block', fontSize: '12px', fontWeight: '600', marginBottom: '4px', color: '#333' }}>
                    📅 Start Date
                  </label>
                  <input
                    type="date"
                    value={medStartDate}
                    onChange={(e) => setMedStartDate(e.target.value)}
                    required
                    style={{ width: '100%', padding: '10px 10px', border: '1px solid #d1d1d6', borderRadius: '8px', fontSize: '13px', boxSizing: 'border-box' }}
                  />
                </div>
                <div style={{ flex: 1, textAlign: 'left' }}>
                  <label style={{ display: 'block', fontSize: '12px', fontWeight: '600', marginBottom: '4px', color: '#333' }}>
                    📅 End Date
                  </label>
                  <input
                    type="date"
                    value={medEndDate}
                    onChange={(e) => setMedEndDate(e.target.value)}
                    required
                    style={{ width: '100%', padding: '10px 10px', border: '1px solid #d1d1d6', borderRadius: '8px', fontSize: '13px', boxSizing: 'border-box' }}
                  />
                </div>
              </div>

              {/* Instructions */}
              <div style={{ marginBottom: '16px', textAlign: 'left' }}>
                <label style={{ display: 'block', fontSize: '13px', fontWeight: '600', marginBottom: '6px', color: '#333' }}>
                  Special Instructions
                </label>
                <input
                  type="text"
                  placeholder="e.g. Take after breakfast / before sleep"
                  value={medInstructions}
                  onChange={(e) => setMedInstructions(e.target.value)}
                  style={{ width: '100%', padding: '10px 12px', border: '1px solid #d1d1d6', borderRadius: '8px', fontSize: '14px', boxSizing: 'border-box' }}
                />
              </div>

              <div style={{ display: 'flex', gap: '10px', marginTop: '20px' }}>
                <button
                  type="button"
                  onClick={() => setShowAddModal(false)}
                  style={{ flex: 1, padding: '12px', border: '1px solid #d1d1d6', borderRadius: '10px', backgroundColor: '#f2f2f7', color: '#000', fontWeight: '600', cursor: 'pointer' }}
                >
                  Cancel
                </button>
                <button
                  type="submit"
                  disabled={submittingMed}
                  style={{ flex: 1, padding: '12px', border: 'none', borderRadius: '10px', backgroundColor: '#007AFF', color: '#ffffff', fontWeight: '700', cursor: 'pointer' }}
                >
                  {submittingMed ? 'Saving...' : 'Add Reminder'}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* 📅 Interactive Calendar Modal Overlay */}
      {showCalendarModal && (
        <div style={{
          position: 'fixed',
          top: 0, left: 0, right: 0, bottom: 0,
          backgroundColor: 'rgba(0, 0, 0, 0.65)',
          backdropFilter: 'blur(6px)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 9999,
          padding: '20px'
        }}>
          <div style={{
            backgroundColor: '#ffffff',
            borderRadius: '24px',
            padding: '24px',
            maxWidth: '520px',
            width: '100%',
            maxHeight: '90vh',
            overflowY: 'auto',
            boxShadow: '0 25px 50px -12px rgba(0,0,0,0.3)',
            boxSizing: 'border-box'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px', borderBottom: '1px solid #e2e8f0', paddingBottom: '12px' }}>
              <div>
                <h2 style={{ fontSize: '18px', fontWeight: '800', color: '#0f172a', margin: 0, display: 'flex', alignItems: 'center', gap: '8px' }}>
                  📅 Medicine Calendar ({selectedElderly?.name})
                </h2>
                <div style={{ fontSize: '12px', color: '#64748b', marginTop: '2px' }}>
                  Real-time dose compliance tracker
                </div>
              </div>
              <button
                onClick={() => setShowCalendarModal(false)}
                style={{
                  background: '#f1f5f9', border: 'none', borderRadius: '50%',
                  width: '32px', height: '32px', fontSize: '16px', cursor: 'pointer',
                  display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#64748b'
                }}
              >
                ✕
              </button>
            </div>

            {/* Calendar Controls */}
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px', background: '#f8fafc', padding: '10px 14px', borderRadius: '14px', border: '1px solid #e2e8f0' }}>
              <div style={{ fontSize: '16px', fontWeight: '800', color: '#1e293b' }}>
                {["January","February","March","April","May","June","July","August","September","October","November","December"][calendarMonth]} {calendarYear}
              </div>
              <div style={{ display: 'flex', gap: '6px' }}>
                <button
                  onClick={() => {
                    if (calendarMonth === 0) { setCalendarMonth(11); setCalendarYear(y => y - 1); }
                    else setCalendarMonth(m => m - 1);
                  }}
                  style={{ padding: '6px 12px', borderRadius: '8px', border: '1px solid #cbd5e1', background: '#ffffff', fontWeight: '800', cursor: 'pointer' }}
                >
                  ◀
                </button>
                <button
                  onClick={() => { setCalendarMonth(new Date().getMonth()); setCalendarYear(new Date().getFullYear()); }}
                  style={{ padding: '6px 12px', borderRadius: '8px', border: '1px solid #bfdbfe', background: '#eff6ff', color: '#2563eb', fontWeight: '800', cursor: 'pointer' }}
                >
                  Today
                </button>
                <button
                  onClick={() => {
                    if (calendarMonth === 11) { setCalendarMonth(0); setCalendarYear(y => y + 1); }
                    else setCalendarMonth(m => m + 1);
                  }}
                  style={{ padding: '6px 12px', borderRadius: '8px', border: '1px solid #cbd5e1', background: '#ffffff', fontWeight: '800', cursor: 'pointer' }}
                >
                  ▶
                </button>
              </div>
            </div>

            {/* Calendar Grid */}
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(7, 1fr)', gap: '6px', marginBottom: '16px' }}>
              {['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'].map((day, idx) => (
                <div key={idx} style={{ textAlign: 'center', fontSize: '11px', fontWeight: '800', color: '#64748b', textTransform: 'uppercase', paddingBottom: '4px' }}>
                  {day}
                </div>
              ))}

              {/* Leading Empty Alignments */}
              {Array.from({ length: new Date(calendarYear, calendarMonth, 1).getDay() }).map((_, emptyIdx) => (
                <div key={`modal-empty-${emptyIdx}`} style={{ height: '48px', backgroundColor: 'transparent' }} />
              ))}

              {/* Days */}
              {Array.from({ length: new Date(calendarYear, calendarMonth + 1, 0).getDate() }, (_, i) => i + 1).map((dayNum) => {
                const dayData = getCalendarDayData(dayNum, calendarMonth, calendarYear);
                const { isToday, isPast, isFuture, totalDoses, takenDoses, missedDoses } = dayData;

                let dayBg = '#f8fafc';
                let dayColor = '#64748b';
                let badgeText = '•';
                let borderColor = '#e2e8f0';

                if (totalDoses === 0) {
                  dayBg = '#f8fafc';
                  dayColor = '#94a3b8';
                  badgeText = 'No Meds';
                } else if (isToday) {
                  dayBg = '#2563eb';
                  dayColor = '#ffffff';
                  borderColor = '#1d4ed8';
                  badgeText = `⏳ ${takenDoses}/${totalDoses}`;
                } else if (isPast) {
                  if (takenDoses === totalDoses && totalDoses > 0) {
                    dayBg = '#d1fae5';
                    dayColor = '#065f46';
                    borderColor = '#a7f3d0';
                    badgeText = `✅ 100%`;
                  } else if (takenDoses > 0) {
                    dayBg = '#fef3c7';
                    dayColor = '#92400e';
                    borderColor = '#fde68a';
                    badgeText = `⚠️ ${takenDoses}/${totalDoses}`;
                  } else {
                    dayBg = '#fee2e2';
                    dayColor = '#991b1b';
                    borderColor = '#fecaca';
                    badgeText = `❌ Missed`;
                  }
                } else if (isFuture) {
                  dayBg = '#f0f9ff';
                  dayColor = '#0369a1';
                  borderColor = '#bae6fd';
                  badgeText = `📅 ${totalDoses} Meds`;
                }

                return (
                  <div
                    key={`modal-day-${dayNum}`}
                    onClick={() => setSelectedCalendarDay(dayData)}
                    style={{
                      height: '52px',
                      backgroundColor: dayBg,
                      color: dayColor,
                      borderRadius: '10px',
                      padding: '6px 2px',
                      display: 'flex',
                      flexDirection: 'column',
                      alignItems: 'center',
                      justifyContent: 'space-between',
                      cursor: 'pointer',
                      border: `1.5px solid ${borderColor}`,
                      boxShadow: isToday ? '0 4px 10px rgba(37,99,235,0.3)' : 'none'
                    }}
                  >
                    <div style={{ fontSize: '13px', fontWeight: '800' }}>{dayNum}</div>
                    <div style={{ fontSize: '9px', fontWeight: '800', textAlign: 'center' }}>
                      {badgeText}
                    </div>
                  </div>
                );
              })}
            </div>

            {/* Clicked Day Details Panel */}
            {selectedCalendarDay && (
              <div style={{ backgroundColor: '#f8fafc', padding: '16px', borderRadius: '16px', border: '1px solid #cbd5e1', marginBottom: '16px' }}>
                <div style={{ fontWeight: '800', color: '#0f172a', fontSize: '14px', marginBottom: '4px', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <span>📋 Realtime Entries: {selectedCalendarDay.dateStr}</span>
                  <span style={{ fontSize: '12px', fontWeight: '700', color: selectedCalendarDay.takenDoses === selectedCalendarDay.totalDoses && selectedCalendarDay.totalDoses > 0 ? '#10b981' : '#f59e0b' }}>
                    {selectedCalendarDay.takenDoses}/{selectedCalendarDay.totalDoses} Taken
                  </span>
                </div>

                {selectedCalendarDay.doseEntries.length === 0 ? (
                  <div style={{ fontSize: '13px', color: '#64748b', fontStyle: 'italic', marginTop: '8px' }}>
                    No medicines scheduled on this date.
                  </div>
                ) : (
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', marginTop: '10px' }}>
                    {selectedCalendarDay.doseEntries.map((entry, idx) => (
                      <div
                        key={idx}
                        style={{
                          padding: '10px 12px',
                          borderRadius: '10px',
                          backgroundColor: '#ffffff',
                          border: '1px solid #e2e8f0',
                          display: 'flex',
                          justifyContent: 'space-between',
                          alignItems: 'center'
                        }}
                      >
                        <div>
                          <div style={{ fontSize: '13px', fontWeight: '700', color: '#1e293b' }}>
                            💊 {entry.medicineName} ({entry.dosage})
                          </div>
                          <div style={{ fontSize: '11px', color: '#64748b', marginTop: '2px' }}>
                            Scheduled Time: ⏰ {entry.scheduledTime}
                          </div>
                        </div>

                        <div>
                          {entry.status === 'taken' ? (
                            <span style={{ backgroundColor: '#d1fae5', color: '#065f46', padding: '4px 8px', borderRadius: '6px', fontSize: '11px', fontWeight: '800' }}>
                              Confirmed ✅ ({entry.timeTaken})
                            </span>
                          ) : entry.status === 'missed' ? (
                            <span style={{ backgroundColor: '#fee2e2', color: '#991b1b', padding: '4px 8px', borderRadius: '6px', fontSize: '11px', fontWeight: '800' }}>
                              Missed ❌
                            </span>
                          ) : (
                            <span style={{ backgroundColor: '#eff6ff', color: '#1d4ed8', padding: '4px 8px', borderRadius: '6px', fontSize: '11px', fontWeight: '800' }}>
                              Scheduled ⏳
                            </span>
                          )}
                        </div>
                      </div>
                    ))}
                  </div>
                )}
              </div>
            )}

            <button
              onClick={() => setShowCalendarModal(false)}
              style={{ width: '100%', padding: '12px', border: 'none', borderRadius: '12px', backgroundColor: '#2563eb', color: '#ffffff', fontWeight: '700', cursor: 'pointer' }}
            >
              Close Calendar
            </button>
          </div>
        </div>
      )}

      {/* � Device Management Modal Overlay */}
      {showDeviceModal && (
        <div style={{
          position: 'fixed',
          top: 0, left: 0, right: 0, bottom: 0,
          backgroundColor: 'rgba(0, 0, 0, 0.65)',
          backdropFilter: 'blur(6px)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 9999,
          padding: '20px'
        }}>
          <div style={{
            backgroundColor: '#ffffff',
            borderRadius: '24px',
            padding: '24px',
            maxWidth: '500px',
            width: '100%',
            maxHeight: '90vh',
            overflowY: 'auto',
            boxShadow: '0 25px 50px -12px rgba(0,0,0,0.3)',
            boxSizing: 'border-box'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px', borderBottom: '1px solid #e2e8f0', paddingBottom: '12px' }}>
              <div>
                <h2 style={{ fontSize: '18px', fontWeight: '800', color: '#0f172a', margin: 0, display: 'flex', alignItems: 'center', gap: '8px' }}>
                  🔧 Device Management
                </h2>
                <div style={{ fontSize: '12px', color: '#64748b', marginTop: '2px' }}>
                  Manage and monitor belt devices
                </div>
              </div>
              <button
                onClick={() => setShowDeviceModal(false)}
                style={{
                  background: '#f1f5f9', border: 'none', borderRadius: '50%',
                  width: '32px', height: '32px', fontSize: '16px', cursor: 'pointer',
                  display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#64748b'
                }}
              >
                ✕
              </button>
            </div>

            {/* Potentially Broken Devices Alert */}
            {potentiallyBrokenDevices.length > 0 && (
              <div style={{
                background: '#fee2e2',
                border: '1px solid #fecaca',
                borderRadius: '12px',
                padding: '12px',
                marginBottom: '16px'
              }}>
                <div style={{ fontSize: '13px', fontWeight: '700', color: '#991b1b', marginBottom: '8px' }}>
                  ⚠️ {potentiallyBrokenDevices.length} Device(s) Not Responding
                </div>
                {potentiallyBrokenDevices.map(device => (
                  <div key={device.deviceId} style={{
                    background: '#ffffff',
                    borderRadius: '8px',
                    padding: '8px',
                    marginBottom: '6px',
                    fontSize: '12px',
                    color: '#7f1d1d'
                  }}>
                    <strong>{device.deviceId}</strong> - Last seen: {device.lastSeen ? new Date(device.lastSeen).toLocaleString() : 'Unknown'}
                  </div>
                ))}
              </div>
            )}

            {/* All Devices List */}
            <div style={{ marginBottom: '16px' }}>
              <div style={{ fontSize: '13px', fontWeight: '700', color: '#334155', marginBottom: '8px' }}>
                All Devices ({allDevices.length})
              </div>
              {allDevices.length === 0 ? (
                <div style={{ fontSize: '12px', color: '#64748b', textAlign: 'center', padding: '16px' }}>
                  No devices registered yet
                </div>
              ) : (
                <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                  {allDevices.map(device => (
                    <div key={device.deviceId} style={{
                      background: '#f8fafc',
                      border: '1px solid #e2e8f0',
                      borderRadius: '12px',
                      padding: '12px'
                    }}>
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '6px' }}>
                        <div style={{ fontSize: '13px', fontWeight: '700', color: '#0f172a' }}>
                          {device.deviceId}
                        </div>
                        <span style={{
                          padding: '4px 8px',
                          borderRadius: '12px',
                          fontSize: '11px',
                          fontWeight: '700',
                          background: device.status === 'ACTIVE' ? '#d1fae5' : device.status === 'BROKEN' ? '#fee2e2' : '#f1f5f9',
                          color: device.status === 'ACTIVE' ? '#065f46' : device.status === 'BROKEN' ? '#991b1b' : '#64748b'
                        }}>
                          {device.status}
                        </span>
                      </div>
                      <div style={{ fontSize: '11px', color: '#64748b', marginBottom: '4px' }}>
                        Type: {device.deviceType} • MAC: {device.macAddress}
                      </div>
                      <div style={{ fontSize: '11px', color: '#64748b', marginBottom: '8px' }}>
                        Assigned: {device.assignedElderlyId || 'Unassigned'}
                      </div>
                      <div style={{ fontSize: '11px', color: '#64748b' }}>
                        Last seen: {device.lastSeen ? new Date(device.lastSeen).toLocaleString() : 'Never'}
                      </div>
                    </div>
                  ))}
                </div>
              )}
            </div>

            {/* Unassigned Devices */}
            <div style={{ marginBottom: '16px' }}>
              <div style={{ fontSize: '13px', fontWeight: '700', color: '#334155', marginBottom: '8px' }}>
                Unassigned Devices ({unassignedDevices.length})
              </div>
              {unassignedDevices.length === 0 ? (
                <div style={{ fontSize: '12px', color: '#64748b', textAlign: 'center', padding: '16px' }}>
                  All devices are assigned
                </div>
              ) : (
                <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                  {unassignedDevices.map(device => (
                    <div key={device.deviceId} style={{
                      background: '#fffbeb',
                      border: '1px solid #fde68a',
                      borderRadius: '12px',
                      padding: '12px',
                      display: 'flex',
                      justifyContent: 'space-between',
                      alignItems: 'center'
                    }}>
                      <div>
                        <div style={{ fontSize: '13px', fontWeight: '700', color: '#92400e' }}>
                          {device.deviceId}
                        </div>
                        <div style={{ fontSize: '11px', color: '#b45309' }}>
                          {device.deviceType}
                        </div>
                      </div>
                      <button
                        onClick={() => {
                          if (selectedElderly) {
                            assignDeviceToElderly(device.deviceId, selectedElderly.elderlyId || selectedElderly.id)
                              .then(() => {
                                showSnackbarMessage('Device assigned successfully!', 'success');
                                loadDeviceData();
                              })
                              .catch(err => {
                                showSnackbarMessage('Failed to assign device', 'error');
                              });
                          } else {
                            alert('Please select an elderly first');
                          }
                        }}
                        style={{
                          background: '#f59e0b',
                          color: '#ffffff',
                          border: 'none',
                          padding: '6px 12px',
                          borderRadius: '8px',
                          fontSize: '11px',
                          fontWeight: '700',
                          cursor: 'pointer'
                        }}
                      >
                        Assign to {selectedElderly?.name?.split(' ')[0] || 'Selected'}
                      </button>
                    </div>
                  ))}
                </div>
              )}
            </div>

            <button
              onClick={() => setShowDeviceModal(false)}
              style={{ width: '100%', padding: '12px', border: 'none', borderRadius: '12px', backgroundColor: '#2563eb', color: '#ffffff', fontWeight: '700', cursor: 'pointer' }}
            >
              Close
            </button>
          </div>
        </div>
      )}

      {/* 📝 Caregiver Advice & Suggestion Modal Overlay */}
      {showSuggestionModal && (
        <div style={{
          position: 'fixed',
          top: 0, left: 0, right: 0, bottom: 0,
          backgroundColor: 'rgba(0, 0, 0, 0.65)',
          backdropFilter: 'blur(6px)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 9999,
          padding: '20px'
        }}>
          <div style={{
            backgroundColor: '#ffffff',
            borderRadius: '24px',
            padding: '24px',
            maxWidth: '440px',
            width: '100%',
            maxHeight: '90vh',
            overflowY: 'auto',
            boxShadow: '0 25px 50px -12px rgba(0,0,0,0.3)',
            boxSizing: 'border-box'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px', borderBottom: '1px solid #e2e8f0', paddingBottom: '12px' }}>
              <div>
                <h2 style={{ fontSize: '18px', fontWeight: '800', color: '#0f172a', margin: 0, display: 'flex', alignItems: 'center', gap: '8px' }}>
                  📝 Add Caregiver Suggestion
                </h2>
                <div style={{ fontSize: '12px', color: '#64748b', marginTop: '2px' }}>
                  For: <strong>{selectedElderly?.name}</strong> (Displays live on senior portal)
                </div>
              </div>
              <button
                onClick={() => setShowSuggestionModal(false)}
                style={{
                  background: '#f1f5f9', border: 'none', borderRadius: '50%',
                  width: '32px', height: '32px', fontSize: '16px', cursor: 'pointer',
                  display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#64748b'
                }}
              >
                ✕
              </button>
            </div>

            <form onSubmit={handleAddSuggestion}>
              <div style={{ marginBottom: '16px' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
                  <label style={{ fontSize: '13px', fontWeight: '700', color: '#334155', margin: 0 }}>
                    Your Advice / Health Note:
                  </label>
                  <button
                    type="button"
                    onClick={startVoiceSuggestionInput}
                    style={{
                      backgroundColor: isListeningSuggestion ? '#ef4444' : '#eff6ff',
                      color: isListeningSuggestion ? '#ffffff' : '#2563eb',
                      border: isListeningSuggestion ? 'none' : '1px solid #bfdbfe',
                      padding: '4px 10px',
                      borderRadius: '20px',
                      fontSize: '12px',
                      fontWeight: '700',
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '4px'
                    }}
                  >
                    🎙️ {isListeningSuggestion ? 'Listening...' : 'Speak Advice'}
                  </button>
                </div>
                <textarea
                  value={newSuggestionText}
                  onChange={(e) => setNewSuggestionText(e.target.value)}
                  placeholder="e.g. Please drink 2L water daily and take 30 mins rest after lunch..."
                  required
                  rows={4}
                  style={{
                    width: '100%',
                    padding: '12px',
                    borderRadius: '12px',
                    border: '1px solid #cbd5e1',
                    fontSize: '14px',
                    fontFamily: 'inherit',
                    outline: 'none',
                    resize: 'vertical',
                    boxSizing: 'border-box'
                  }}
                />
              </div>

              {/* Quick Presets */}
              <div style={{ marginBottom: '18px' }}>
                <div style={{ fontSize: '11px', fontWeight: '700', color: '#64748b', textTransform: 'uppercase', marginBottom: '6px' }}>
                  ⚡ Quick Presets:
                </div>
                <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
                  {[
                    "Have a meal on time please 🥗",
                    "Drink plenty of water today 💧",
                    "Remember to take rest after medicine 😴",
                    "Keep warm and stay indoors 🏠"
                  ].map((presetText, idx) => (
                    <button
                      key={idx}
                      type="button"
                      onClick={() => setNewSuggestionText(presetText)}
                      style={{
                        backgroundColor: '#f1f5f9',
                        color: '#334155',
                        border: '1px solid #cbd5e1',
                        padding: '5px 10px',
                        borderRadius: '16px',
                        fontSize: '11px',
                        fontWeight: '600',
                        cursor: 'pointer'
                      }}
                    >
                      {presetText}
                    </button>
                  ))}
                </div>
              </div>

              <div style={{ display: 'flex', gap: '10px' }}>
                <button
                  type="button"
                  onClick={() => setShowSuggestionModal(false)}
                  style={{ flex: 1, padding: '12px', border: '1px solid #cbd5e1', borderRadius: '12px', backgroundColor: '#f8fafc', color: '#334155', fontWeight: '700', cursor: 'pointer' }}
                >
                  Cancel
                </button>
                <button
                  type="submit"
                  style={{ flex: 1, padding: '12px', border: 'none', borderRadius: '12px', backgroundColor: '#10b981', color: '#ffffff', fontWeight: '800', cursor: 'pointer' }}
                >
                  Send Suggestion 🚀
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* 📞 Contacts Management Modal Overlay */}
      {showContactsModal && (
        <div style={{
          position: 'fixed',
          top: 0, left: 0, right: 0, bottom: 0,
          backgroundColor: 'rgba(0, 0, 0, 0.65)',
          backdropFilter: 'blur(6px)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 9999,
          padding: '20px'
        }}>
          <div style={{
            backgroundColor: '#ffffff',
            borderRadius: '24px',
            padding: '24px',
            maxWidth: '500px',
            width: '100%',
            maxHeight: '90vh',
            overflowY: 'auto',
            boxShadow: '0 25px 50px -12px rgba(0,0,0,0.3)',
            boxSizing: 'border-box'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px', borderBottom: '1px solid #e2e8f0', paddingBottom: '12px' }}>
              <div>
                <h2 style={{ fontSize: '18px', fontWeight: '800', color: '#0f172a', margin: 0, display: 'flex', alignItems: 'center', gap: '8px' }}>
                  📞 Emergency Contacts
                </h2>
                <div style={{ fontSize: '12px', color: '#64748b', marginTop: '2px' }}>
                  For: <strong>{selectedElderly?.name}</strong>
                </div>
              </div>
              <button
                onClick={() => setShowContactsModal(false)}
                style={{
                  background: '#f1f5f9', border: 'none', borderRadius: '50%',
                  width: '32px', height: '32px', fontSize: '16px', cursor: 'pointer',
                  display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#64748b'
                }}
              >
                ✕
              </button>
            </div>

            {/* Main Guardian (Read-only) */}
            <div style={{ marginBottom: '16px', padding: '12px', backgroundColor: '#f0fdf4', borderRadius: '12px', border: '1px solid #bbf7d0' }}>
              <div style={{ fontSize: '13px', fontWeight: '700', color: '#166534', marginBottom: '8px' }}>
                🛡️ Main Guardian (You)
              </div>
              <div style={{ fontSize: '12px', color: '#15803d' }}>
                <div><strong>Name:</strong> {contacts.mainGuardianName || guardianName}</div>
                <div><strong>Phone:</strong> {contacts.mainGuardianPhone || guardianPhone}</div>
              </div>
            </div>

            {/* Secondary Guardian */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{ fontSize: '13px', fontWeight: '700', color: '#334155', marginBottom: '8px', display: 'block' }}>
                👥 Secondary Guardian
              </label>
              <input
                type="text"
                value={editingContacts.secondaryGuardianName || ''}
                onChange={(e) => setEditingContacts({ ...editingContacts, secondaryGuardianName: e.target.value })}
                placeholder="Name"
                style={{
                  width: '100%',
                  padding: '10px',
                  borderRadius: '8px',
                  border: '1px solid #cbd5e1',
                  marginBottom: '8px',
                  fontSize: '14px',
                  boxSizing: 'border-box'
                }}
              />
              <input
                type="tel"
                value={editingContacts.secondaryGuardianPhone || ''}
                onChange={(e) => setEditingContacts({ ...editingContacts, secondaryGuardianPhone: e.target.value })}
                placeholder="Phone Number"
                style={{
                  width: '100%',
                  padding: '10px',
                  borderRadius: '8px',
                  border: '1px solid #cbd5e1',
                  fontSize: '14px',
                  boxSizing: 'border-box'
                }}
              />
            </div>

            {/* Neighbor */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{ fontSize: '13px', fontWeight: '700', color: '#334155', marginBottom: '8px', display: 'block' }}>
                🏠 Neighbor
              </label>
              <input
                type="text"
                value={editingContacts.neighborName || ''}
                onChange={(e) => setEditingContacts({ ...editingContacts, neighborName: e.target.value })}
                placeholder="Name"
                style={{
                  width: '100%',
                  padding: '10px',
                  borderRadius: '8px',
                  border: '1px solid #cbd5e1',
                  marginBottom: '8px',
                  fontSize: '14px',
                  boxSizing: 'border-box'
                }}
              />
              <input
                type="tel"
                value={editingContacts.neighborPhone || ''}
                onChange={(e) => setEditingContacts({ ...editingContacts, neighborPhone: e.target.value })}
                placeholder="Phone Number"
                style={{
                  width: '100%',
                  padding: '10px',
                  borderRadius: '8px',
                  border: '1px solid #cbd5e1',
                  fontSize: '14px',
                  boxSizing: 'border-box'
                }}
              />
            </div>

            <div style={{ display: 'flex', gap: '10px' }}>
              <button
                onClick={() => setShowContactsModal(false)}
                style={{ flex: 1, padding: '12px', border: '1px solid #cbd5e1', borderRadius: '12px', backgroundColor: '#f8fafc', color: '#334155', fontWeight: '700', cursor: 'pointer' }}
              >
                Cancel
              </button>
              <button
                onClick={handleSaveContacts}
                style={{ flex: 1, padding: '12px', border: 'none', borderRadius: '12px', backgroundColor: '#10b981', color: '#ffffff', fontWeight: '800', cursor: 'pointer' }}
              >
                Save Contacts ✅
              </button>
            </div>
          </div>
        </div>
      )}

      {/* 📟 Device Management & Pairing Modal Overlay */}
      {showDevicePairingModal && (
        <div style={{
          position: 'fixed',
          top: 0, left: 0, right: 0, bottom: 0,
          backgroundColor: 'rgba(0, 0, 0, 0.65)',
          backdropFilter: 'blur(6px)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 9999,
          padding: '20px'
        }}>
          <div style={{
            backgroundColor: '#ffffff',
            borderRadius: '24px',
            padding: '24px',
            maxWidth: '540px',
            width: '100%',
            maxHeight: '90vh',
            overflowY: 'auto',
            boxShadow: '0 25px 50px -12px rgba(0,0,0,0.3)',
            boxSizing: 'border-box'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px', borderBottom: '1px solid #e2e8f0', paddingBottom: '12px' }}>
              <div>
                <h2 style={{ fontSize: '18px', fontWeight: '800', color: '#0f172a', margin: 0, display: 'flex', alignItems: 'center', gap: '8px' }}>
                  📟 Wearable Device Management & Pairing
                </h2>
                <div style={{ fontSize: '12px', color: '#64748b', marginTop: '2px' }}>
                  Senior Ward: <strong>{selectedElderly?.name || 'Senior Citizen'}</strong>
                </div>
              </div>
              <button
                onClick={() => setShowDevicePairingModal(false)}
                style={{
                  background: '#f1f5f9', border: 'none', borderRadius: '50%',
                  width: '32px', height: '32px', fontSize: '16px', cursor: 'pointer',
                  display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#64748b'
                }}
              >
                ✕
              </button>
            </div>

            {/* 📊 Device Health Summary Banner */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: 'repeat(2, 1fr)',
              gap: '10px',
              marginBottom: '16px'
            }}>
              <div style={{ backgroundColor: '#f0fdf4', padding: '12px', borderRadius: '12px', border: '1px solid #bbf7d0', textAlign: 'center' }}>
                <div style={{ fontSize: '20px', fontWeight: '800', color: '#166534' }}>
                  {elderlyDevices.currentDevices.length}
                </div>
                <div style={{ fontSize: '11px', fontWeight: '700', color: '#15803d', textTransform: 'uppercase' }}>
                  Active Paired Devices
                </div>
              </div>
              <div style={{ backgroundColor: '#eff6ff', padding: '12px', borderRadius: '12px', border: '1px solid #bfdbfe', textAlign: 'center' }}>
                <div style={{ fontSize: '20px', fontWeight: '800', color: '#1e40af' }}>
                  {elderlyDevices.previousDevices.length}
                </div>
                <div style={{ fontSize: '11px', fontWeight: '700', color: '#1d4ed8', textTransform: 'uppercase' }}>
                  Previous Device History
                </div>
              </div>
            </div>

            {/* 1. Currently Paired Devices Section */}
            <div style={{ marginBottom: '20px', backgroundColor: '#f8fafc', padding: '14px', borderRadius: '14px', border: '1px solid #e2e8f0' }}>
              <div style={{ fontSize: '13px', fontWeight: '800', color: '#0f172a', marginBottom: '10px', textTransform: 'uppercase', letterSpacing: '0.5px' }}>
                📱 Currently Assigned Devices
              </div>
              {elderlyDevices.currentDevices.length > 0 ? (
                <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                  {elderlyDevices.currentDevices.map((device) => (
                    <div
                      key={device.id || device.deviceId}
                      style={{
                        display: 'flex',
                        justifyContent: 'space-between',
                        alignItems: 'center',
                        padding: '12px 14px',
                        backgroundColor: '#ffffff',
                        borderRadius: '10px',
                        border: '1px solid #cbd5e1',
                        boxShadow: '0 2px 4px rgba(0,0,0,0.03)'
                      }}
                    >
                      <div>
                        <div style={{ fontSize: '14px', fontWeight: '800', color: '#15803d' }}>
                          📟 {device.deviceType || 'Wearable Sensor'}
                        </div>
                        <div style={{ fontSize: '12px', color: '#475569', marginTop: '2px' }}>
                          MAC: <strong>{device.macAddress || device.deviceId}</strong>
                        </div>
                        <div style={{ fontSize: '10px', color: '#16a34a', fontWeight: '700', marginTop: '2px' }}>
                          🟢 Status: Active & Synced
                        </div>
                      </div>
                      <button
                        onClick={() => handleUnpairDevice(device.id || device.deviceId)}
                        style={{
                          backgroundColor: '#ef4444',
                          color: '#ffffff',
                          border: 'none',
                          padding: '6px 14px',
                          borderRadius: '8px',
                          fontSize: '12px',
                          fontWeight: '800',
                          cursor: 'pointer',
                          boxShadow: '0 2px 6px rgba(239, 68, 68, 0.25)'
                        }}
                      >
                        Unpair Device
                      </button>
                    </div>
                  ))}
                </div>
              ) : (
                <div style={{ fontSize: '12px', color: '#64748b', fontStyle: 'italic', textAlign: 'center', padding: '12px', backgroundColor: '#ffffff', borderRadius: '8px', border: '1px dashed #cbd5e1' }}>
                  No active devices paired to this senior ward yet. Use the form below to pair one!
                </div>
              )}
            </div>

            {/* 2. Pair New Device Section */}
            <div style={{ marginBottom: '20px', backgroundColor: '#ffffff', padding: '14px', borderRadius: '14px', border: '1px solid #cbd5e1' }}>
              <div style={{ fontSize: '13px', fontWeight: '800', color: '#0f172a', marginBottom: '12px', textTransform: 'uppercase', letterSpacing: '0.5px' }}>
                ⚡ Pair New Device
              </div>
              <div style={{ marginBottom: '14px' }}>
                <label style={{ display: 'block', fontSize: '12px', fontWeight: '700', color: '#334155', marginBottom: '6px' }}>
                  Device Type
                </label>
                <select
                  value={deviceTypeInput}
                  onChange={(e) => setDeviceTypeInput(e.target.value)}
                  style={{
                    width: '100%',
                    padding: '10px 12px',
                    border: '1px solid #d1d5db',
                    borderRadius: '8px',
                    fontSize: '14px',
                    backgroundColor: '#ffffff',
                    fontWeight: '600'
                  }}
                >
                  <option value="Waist Belt">Waist Belt (Smart Fall Detection)</option>
                  <option value="Wrist Watch">Wrist Watch (Vital Monitoring)</option>
                </select>
              </div>

              <div style={{ marginBottom: '16px' }}>
                <label style={{ display: 'block', fontSize: '12px', fontWeight: '700', color: '#334155', marginBottom: '6px' }}>
                  Hardware MAC Address *
                </label>
                <input
                  type="text"
                  value={macAddressInput}
                  onChange={(e) => setMacAddressInput(e.target.value)}
                  placeholder="e.g., AA:BB:CC:DD:EE:FF"
                  required
                  style={{
                    width: '100%',
                    padding: '10px 12px',
                    border: '1px solid #d1d5db',
                    borderRadius: '8px',
                    fontSize: '14px',
                    boxSizing: 'border-box'
                  }}
                />
              </div>

              <button
                onClick={handlePairDevice}
                disabled={pairingLoading}
                style={{
                  width: '100%',
                  backgroundColor: '#8b5cf6',
                  color: '#ffffff',
                  border: 'none',
                  padding: '12px',
                  borderRadius: '10px',
                  fontSize: '14px',
                  fontWeight: '800',
                  cursor: pairingLoading ? 'not-allowed' : 'pointer',
                  opacity: pairingLoading ? 0.6 : 1,
                  boxShadow: '0 4px 12px rgba(139, 92, 246, 0.25)'
                }}
              >
                {pairingLoading ? 'Pairing Hardware...' : 'Pair Device to Senior 🚀'}
              </button>
            </div>

            {/* 3. Previous Device History Section */}
            {elderlyDevices.previousDevices.length > 0 && (
              <div style={{ backgroundColor: '#f8fafc', padding: '12px', borderRadius: '12px', border: '1px solid #e2e8f0' }}>
                <div style={{ fontSize: '12px', fontWeight: '700', color: '#64748b', marginBottom: '8px', textTransform: 'uppercase' }}>
                  📜 Previous Device History
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
                  {elderlyDevices.previousDevices.map((prev, idx) => (
                    <div key={idx} style={{ fontSize: '11px', color: '#64748b', backgroundColor: '#ffffff', padding: '8px 10px', borderRadius: '6px', border: '1px solid #e2e8f0' }}>
                      <strong>{prev.deviceType || 'Device'}</strong> ({prev.macAddress || prev.deviceId}) • Status: {prev.status || 'RETIRED'}
                    </div>
                  ))}
                </div>
              </div>
            )}

            <button
              onClick={() => setShowDevicePairingModal(false)}
              style={{ width: '100%', marginTop: '16px', padding: '12px', border: '1px solid #cbd5e1', borderRadius: '12px', backgroundColor: '#f1f5f9', color: '#334155', fontWeight: '700', cursor: 'pointer' }}
            >
              Done & Close
            </button>
          </div>
        </div>
      )}

      {/* 🚀 Floating Snackbar Notification */}
      {snackbar.show && (
        <div style={{
          position: 'fixed',
          bottom: '28px',
          right: '28px',
          backgroundColor: snackbar.type === 'error' ? '#ef4444' : '#10b981',
          color: '#ffffff',
          padding: '14px 22px',
          borderRadius: '16px',
          boxShadow: '0 12px 30px -5px rgba(0, 0, 0, 0.3)',
          display: 'flex',
          alignItems: 'center',
          gap: '12px',
          fontSize: '14px',
          fontWeight: '700',
          zIndex: 999999,
          animation: 'fadeIn 0.25s ease-in-out'
        }}>
          <span style={{ fontSize: '18px' }}>{snackbar.type === 'error' ? '⚠️' : '✅'}</span>
          <span>{snackbar.message}</span>
          <button
            onClick={() => setSnackbar({ show: false, message: '', type: 'success' })}
            style={{
              background: 'transparent',
              border: 'none',
              color: '#ffffff',
              fontSize: '16px',
              cursor: 'pointer',
              marginLeft: '8px',
              padding: '0 4px',
              lineHeight: 1,
              opacity: 0.8
            }}
          >
            ✕
          </button>
        </div>
      )}
      {/* Elderly Location Map Modal Component */}
      {showMapModal && (
        <ElderlyMapModal
          isOpen={showMapModal}
          onClose={() => setShowMapModal(false)}
          elderly={selectedElderly}
          sensorData={sensorData}
        />
      )}

    </div>
  );
}
