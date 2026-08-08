import React, { useEffect, useRef, useState } from 'react';
import L from 'leaflet';
import 'leaflet/dist/leaflet.css';
import { sendSensorTelemetry } from '../services/api';

// Fix default marker icon issues in Leaflet with Vite
delete L.Icon.Default.prototype._getIconUrl;
L.Icon.Default.mergeOptions({
  iconRetinaUrl: 'https://unpkg.com/leaflet@1.9.4/dist/images/marker-icon-2x.png',
  iconUrl: 'https://unpkg.com/leaflet@1.9.4/dist/images/marker-icon.png',
  shadowUrl: 'https://unpkg.com/leaflet@1.9.4/dist/images/marker-shadow.png',
});

// Known city center coordinates mapping for default registered locations
const CITY_COORDINATES = {
  pune: { lat: 18.5204, lng: 73.8567 },
  mumbai: { lat: 19.0760, lng: 72.8777 },
  delhi: { lat: 28.6139, lng: 77.2090 },
  newdelhi: { lat: 28.6139, lng: 77.2090 },
  bangalore: { lat: 12.9716, lng: 77.5946 },
  bengaluru: { lat: 12.9716, lng: 77.5946 },
  hyderabad: { lat: 17.3850, lng: 78.4867 },
  chennai: { lat: 13.0827, lng: 80.2707 },
  kolkata: { lat: 22.5726, lng: 88.3639 },
  ahmedabad: { lat: 23.0225, lng: 72.5714 },
  pimpri: { lat: 18.6298, lng: 73.7997 },
  chinchwad: { lat: 18.6298, lng: 73.7997 }
};

// Haversine formula to compute distance in km between 2 lat/lng points
function calculateDistance(lat1, lon1, lat2, lon2) {
  const R = 6371; // Earth radius in km
  const dLat = ((lat2 - lat1) * Math.PI) / 180;
  const dLon = ((lon2 - lon1) * Math.PI) / 180;
  const a =
    Math.sin(dLat / 2) * Math.sin(dLat / 2) +
    Math.cos((lat1 * Math.PI) / 180) *
      Math.cos((lat2 * Math.PI) / 180) *
      Math.sin(dLon / 2) *
      Math.sin(dLon / 2);
  const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
  return R * c;
}

export default function ElderlyMapModal({ isOpen, onClose, elderly, sensorData }) {
  const mapRef = useRef(null);
  const mapInstanceRef = useRef(null);
  const elderlyMarkerRef = useRef(null);
  const guardianMarkerRef = useRef(null);
  const routePolylineRef = useRef(null);

  const [guardianCoords, setGuardianCoords] = useState(null);
  const [isNavigating, setIsNavigating] = useState(false);
  const [distanceKm, setDistanceKm] = useState(null);
  const [geoError, setGeoError] = useState('');
  const [isSimulating, setIsSimulating] = useState(false);
  const [manualCoords, setManualCoords] = useState(null);

  const elderlyId = elderly?.elderlyId || elderly?.id || elderly?.elderly_id || 'default';
  const deviceId = sensorData?.deviceId || elderly?.primaryDeviceId || 'vois_belt';
  const elderlyName = elderly?.name || 'Senior Person';

  // Local Storage keys for caching last fetched hardware location
  const storageKeyElderly = `silvercare_last_loc_${elderlyId}`;
  const storageKeyDevice = `silvercare_last_loc_${deviceId}`;

  // 1. Check live telemetry coordinates
  const rawLat = sensorData?.latitude;
  const rawLng = sensorData?.longitude;
  const hasLiveCoords = rawLat !== null && rawLat !== undefined && rawLng !== null && rawLng !== undefined;

  // Save valid live telemetry coordinates to localStorage automatically
  useEffect(() => {
    if (hasLiveCoords) {
      const locPayload = JSON.stringify({
        latitude: Number(rawLat),
        longitude: Number(rawLng),
        timestamp: sensorData?.received_at || new Date().toLocaleString(),
        device: deviceId
      });
      localStorage.setItem(storageKeyElderly, locPayload);
      localStorage.setItem(storageKeyDevice, locPayload);
    }
  }, [hasLiveCoords, rawLat, rawLng, storageKeyElderly, storageKeyDevice, deviceId, sensorData?.received_at]);

  // 2. Resolve final coordinates to display (Priority: Manual Override -> Live Hardware -> Stored Cache -> Backend Entity -> Profile City -> Default)
  let lat = 18.5204;
  let lng = 73.8567;
  let locationStatusType = 'DEFAULT'; // 'MANUAL', 'LIVE', 'STORED', 'PROFILE', 'DEFAULT'
  let locationStatusLabel = '📍 Standard Base Location';
  let timestampLabel = null;

  if (manualCoords) {
    lat = manualCoords.lat;
    lng = manualCoords.lng;
    locationStatusType = 'MANUAL';
    locationStatusLabel = '📌 Custom Set Pin';
  } else if (hasLiveCoords) {
    lat = Number(rawLat);
    lng = Number(rawLng);
    locationStatusType = 'LIVE';
    locationStatusLabel = '📡 Live Hardware GPS Telemetry';
    timestampLabel = sensorData?.received_at;
  } else {
    // Check localStorage cache
    const stored = localStorage.getItem(storageKeyElderly) || localStorage.getItem(storageKeyDevice);
    if (stored) {
      try {
        const parsed = JSON.parse(stored);
        if (parsed.latitude && parsed.longitude) {
          lat = Number(parsed.latitude);
          lng = Number(parsed.longitude);
          locationStatusType = 'STORED';
          timestampLabel = parsed.timestamp;
          locationStatusLabel = `🕒 Last Fetched Hardware Location (${parsed.timestamp || 'Saved'})`;
        }
      } catch (e) {
        console.error('Error reading cached location:', e);
      }
    }

    if (locationStatusType === 'DEFAULT') {
      // Check if elderly backend entity has lat/lng
      if (elderly?.latitude && elderly?.longitude) {
        lat = Number(elderly.latitude);
        lng = Number(elderly.longitude);
        locationStatusType = 'PROFILE';
        locationStatusLabel = '🏠 Registered Home Location';
      } else if (elderly?.location) {
        const cityKey = String(elderly.location).toLowerCase().replace(/[^a-z]/g, '');
        if (CITY_COORDINATES[cityKey]) {
          lat = CITY_COORDINATES[cityKey].lat;
          lng = CITY_COORDINATES[cityKey].lng;
          locationStatusType = 'PROFILE';
          locationStatusLabel = `📍 Registered Location (${elderly.location})`;
        }
      }
    }
  }

  const stateName = sensorData?.stateName || 'NORMAL';
  const isEmergency = stateName.includes('FALL') || stateName.includes('PREFALL');

  // Initialize & Update Leaflet Map Instance
  useEffect(() => {
    if (!isOpen) return;

    const timer = setTimeout(() => {
      if (!mapRef.current) return;

      // Create map instance
      if (!mapInstanceRef.current) {
        const map = L.map(mapRef.current, {
          center: [lat, lng],
          zoom: 15,
          zoomControl: true
        });

        L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
          maxZoom: 19,
          attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
        }).addTo(map);

        // Allow clicking on map to place pin manually
        map.on('click', (e) => {
          const { lat: clickLat, lng: clickLng } = e.latlng;
          setManualCoords({ lat: Number(clickLat.toFixed(5)), lng: Number(clickLng.toFixed(5)) });
        });

        mapInstanceRef.current = map;
      } else {
        if (!guardianCoords) {
          mapInstanceRef.current.setView([lat, lng], 15, { animate: true });
        }
        mapInstanceRef.current.invalidateSize();
      }

      // Elderly Custom Marker Icon
      const customElderlyIcon = L.divIcon({
        className: 'custom-elderly-marker',
        html: `
          <div style="
            position: relative;
            width: 46px;
            height: 46px;
            background: ${isEmergency ? '#ff3b30' : '#007AFF'};
            border: 3px solid #ffffff;
            border-radius: 50%;
            box-shadow: 0 6px 16px rgba(0,0,0,0.35);
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 24px;
            color: white;
            cursor: pointer;
            ${isEmergency ? 'animation: pulse 1s infinite alternate;' : ''}
          ">
            👴
            <div style="
              position: absolute;
              bottom: -7px;
              left: 50%;
              transform: translateX(-50%);
              width: 0;
              height: 0;
              border-left: 7px solid transparent;
              border-right: 7px solid transparent;
              border-top: 9px solid ${isEmergency ? '#ff3b30' : '#007AFF'};
            "></div>
          </div>
        `,
        iconSize: [46, 46],
        iconAnchor: [23, 52],
        popupAnchor: [0, -50]
      });

      if (elderlyMarkerRef.current) {
        elderlyMarkerRef.current.setLatLng([lat, lng]);
        elderlyMarkerRef.current.setIcon(customElderlyIcon);
      } else {
        const marker = L.marker([lat, lng], { icon: customElderlyIcon }).addTo(mapInstanceRef.current);
        elderlyMarkerRef.current = marker;
      }

      const popupContent = `
        <div style="font-family: system-ui, -apple-system, sans-serif; padding: 4px; text-align: center; min-width: 190px;">
          <h4 style="margin: 0 0 6px 0; font-size: 15px; font-weight: 800; color: #1c1c1e;">
            👴 ${elderlyName}
          </h4>
          <div style="
            display: inline-block;
            padding: 3px 10px;
            border-radius: 12px;
            font-size: 12px;
            font-weight: 700;
            margin-bottom: 8px;
            background: ${isEmergency ? '#ffebee' : '#e8f5e8'};
            color: ${isEmergency ? '#ff3b30' : '#34c759'};
          ">
            ${isEmergency ? `⚠️ ${stateName}` : '✅ Status: Safe & Normal'}
          </div>
          <div style="font-size: 12px; color: #475569; margin-bottom: 4px;">
            📍 <strong>GPS:</strong> ${lat.toFixed(4)}°, ${lng.toFixed(4)}°
          </div>
          <div style="font-size: 11px; color: #64748b;">
            ${locationStatusLabel}
          </div>
        </div>
      `;
      elderlyMarkerRef.current.bindPopup(popupContent);

    }, 200);

    return () => clearTimeout(timer);
  }, [isOpen, lat, lng, elderlyName, stateName, isEmergency, locationStatusLabel, guardianCoords]);

  // Clean up when modal closes
  useEffect(() => {
    if (!isOpen) {
      if (mapInstanceRef.current) {
        mapInstanceRef.current.remove();
        mapInstanceRef.current = null;
        elderlyMarkerRef.current = null;
        guardianMarkerRef.current = null;
        routePolylineRef.current = null;
      }
      setIsNavigating(false);
      setGuardianCoords(null);
      setDistanceKm(null);
      setGeoError('');
      setManualCoords(null);
    }
  }, [isOpen]);

  // Trigger Hardware GPS Telemetry Simulation POST to backend
  const handleSimulateHardwareGps = async () => {
    setIsSimulating(true);
    setGeoError('');

    try {
      // Use current browser position if available, else standard test coords near Pune/Mumbai
      let simLat = 18.5204 + (Math.random() - 0.5) * 0.01;
      let simLng = 73.8567 + (Math.random() - 0.5) * 0.01;

      if (navigator.geolocation) {
        navigator.geolocation.getCurrentPosition(
          async (pos) => {
            simLat = pos.coords.latitude;
            simLng = pos.coords.longitude;
            await sendSimulatedTelemetry(simLat, simLng);
          },
          async () => {
            await sendSimulatedTelemetry(simLat, simLng);
          }
        );
      } else {
        await sendSimulatedTelemetry(simLat, simLng);
      }
    } catch (err) {
      console.error('Simulation error:', err);
      setIsSimulating(false);
    }
  };

  const sendSimulatedTelemetry = async (simLat, simLng) => {
    try {
      const payload = {
        deviceId: deviceId,
        beltType: sensorData?.beltType || 'Waist Belt',
        heartRate: 75,
        spo2: 98,
        temperature: 36.6,
        beltWorn: true,
        stateName: 'NORMAL',
        latitude: simLat,
        longitude: simLng,
        timestamp: SystemTime()
      };

      const res = await sendSensorTelemetry(payload);
      setIsSimulating(false);
      
      // Save locally to cache immediately
      const locPayload = JSON.stringify({
        latitude: simLat,
        longitude: simLng,
        timestamp: new Date().toLocaleString(),
        device: deviceId
      });
      localStorage.setItem(storageKeyElderly, locPayload);
      localStorage.setItem(storageKeyDevice, locPayload);

      setManualCoords({ lat: simLat, lng: simLng });
      alert(`📡 Live Telemetry GPS fix transmitted to backend successfully! Location updated to: ${simLat.toFixed(4)}°, ${simLng.toFixed(4)}°`);
    } catch (e) {
      setIsSimulating(false);
    }
  };

  function SystemTime() {
    return Date.now();
  }

  // Handle "Start Traveling / Get Route Directions"
  const handleStartTraveling = () => {
    setGeoError('');
    setIsNavigating(true);

    if (!navigator.geolocation) {
      setGeoError('Geolocation is not supported by your browser.');
      window.open(`https://www.google.com/maps/dir/?api=1&destination=${lat},${lng}&travelmode=driving`, '_blank');
      return;
    }

    navigator.geolocation.getCurrentPosition(
      (position) => {
        const gLat = position.coords.latitude;
        const gLng = position.coords.longitude;
        setGuardianCoords({ lat: gLat, lng: gLng });

        const dist = calculateDistance(gLat, gLng, lat, lng);
        setDistanceKm(dist.toFixed(2));

        const map = mapInstanceRef.current;
        if (map) {
          const customGuardianIcon = L.divIcon({
            className: 'custom-guardian-marker',
            html: `
              <div style="
                position: relative;
                width: 44px;
                height: 44px;
                background: #10b981;
                border: 3px solid #ffffff;
                border-radius: 50%;
                box-shadow: 0 6px 16px rgba(16,185,129,0.4);
                display: flex;
                align-items: center;
                justify-content: center;
                font-size: 22px;
                color: white;
                cursor: pointer;
              ">
                🚗
                <div style="
                  position: absolute;
                  bottom: -6px;
                  left: 50%;
                  transform: translateX(-50%);
                  width: 0;
                  height: 0;
                  border-left: 6px solid transparent;
                  border-right: 6px solid transparent;
                  border-top: 8px solid #10b981;
                "></div>
              </div>
            `,
            iconSize: [44, 44],
            iconAnchor: [22, 50],
            popupAnchor: [0, -46]
          });

          if (guardianMarkerRef.current) {
            guardianMarkerRef.current.setLatLng([gLat, gLng]);
          } else {
            const gMarker = L.marker([gLat, gLng], { icon: customGuardianIcon }).addTo(map);
            gMarker.bindPopup(`
              <div style="font-family: system-ui; text-align: center; padding: 4px;">
                <strong>🚗 Your Location (Guardian)</strong><br/>
                <span style="font-size: 12px; color: #64748b;">${gLat.toFixed(4)}°, ${gLng.toFixed(4)}°</span>
              </div>
            `);
            guardianMarkerRef.current = gMarker;
          }

          const polylineCoords = [[gLat, gLng], [lat, lng]];
          if (routePolylineRef.current) {
            routePolylineRef.current.setLatLngs(polylineCoords);
          } else {
            const polyline = L.polyline(polylineCoords, {
              color: '#007AFF',
              weight: 5,
              opacity: 0.85,
              dashArray: '10, 10'
            }).addTo(map);
            routePolylineRef.current = polyline;
          }

          map.fitBounds(polylineCoords, { padding: [60, 60], animate: true });
        }

        // Launch Google Maps navigation in new tab
        const googleMapsNavUrl = `https://www.google.com/maps/dir/?api=1&origin=${gLat},${gLng}&destination=${lat},${lng}&travelmode=driving`;
        window.open(googleMapsNavUrl, '_blank');
      },
      (error) => {
        console.warn('Geolocation error:', error);
        setGeoError('Could not fetch your current GPS location. Opening directions in Google Maps...');
        window.open(`https://www.google.com/maps/dir/?api=1&destination=${lat},${lng}&travelmode=driving`, '_blank');
      },
      { enableHighAccuracy: true, timeout: 10000, maximumAge: 0 }
    );
  };

  const handleRecenter = () => {
    if (mapInstanceRef.current) {
      if (guardianCoords) {
        mapInstanceRef.current.fitBounds([[guardianCoords.lat, guardianCoords.lng], [lat, lng]], { padding: [50, 50] });
      } else {
        mapInstanceRef.current.setView([lat, lng], 16, { animate: true });
      }
      if (elderlyMarkerRef.current) {
        elderlyMarkerRef.current.openPopup();
      }
    }
  };

  if (!isOpen) return null;

  return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0, 0, 0, 0.65)',
      backdropFilter: 'blur(8px)',
      WebkitBackdropFilter: 'blur(8px)',
      zIndex: 9999,
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      padding: '16px',
      boxSizing: 'border-box'
    }}>
      <div style={{
        backgroundColor: '#ffffff',
        width: '100%',
        maxWidth: '920px',
        maxHeight: '95vh',
        borderRadius: '24px',
        boxShadow: '0 20px 40px rgba(0, 0, 0, 0.25)',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        border: '1px solid rgba(255, 255, 255, 0.2)'
      }}>
        
        {/* Header Bar */}
        <div style={{
          padding: '16px 24px',
          background: '#ffffff',
          borderBottom: '1px solid #e5e5e7',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center'
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <div style={{
              width: '42px',
              height: '42px',
              borderRadius: '12px',
              backgroundColor: isEmergency ? '#ffebee' : 'rgba(0, 122, 255, 0.1)',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontSize: '22px'
            }}>
              📍
            </div>
            <div>
              <h2 style={{ margin: 0, fontSize: '19px', fontWeight: '800', color: '#1c1c1e' }}>
                See Elderly Person - Location Tracker
              </h2>
              <p style={{ margin: '2px 0 0 0', fontSize: '13px', color: '#6c6c70' }}>
                Tracking senior ward <strong>{elderlyName}</strong>
              </p>
            </div>
          </div>

          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <button
              onClick={handleSimulateHardwareGps}
              disabled={isSimulating}
              style={{
                backgroundColor: 'rgba(0, 122, 255, 0.1)',
                color: '#007AFF',
                border: '1px solid rgba(0, 122, 255, 0.25)',
                padding: '6px 12px',
                borderRadius: '10px',
                fontSize: '12px',
                fontWeight: '700',
                cursor: 'pointer'
              }}
              title="Send a live GPS fix simulation to backend API"
            >
              {isSimulating ? '⌛ Simulating...' : '📡 Test Hardware GPS Fix'}
            </button>

            <button
              onClick={onClose}
              style={{
                background: '#f2f2f7',
                border: 'none',
                width: '36px',
                height: '36px',
                borderRadius: '18px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                fontSize: '18px',
                color: '#6c6c70',
                cursor: 'pointer',
                fontWeight: '700'
              }}
              title="Close Map"
            >
              ✕
            </button>
          </div>
        </div>

        {/* Status Metrics Cards Grid */}
        <div style={{
          padding: '12px 20px',
          backgroundColor: '#f8fafc',
          borderBottom: '1px solid #e2e8f0',
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fit, minmax(180px, 1fr))',
          gap: '10px'
        }}>
          {/* Coordinates Card */}
          <div style={{
            background: '#ffffff',
            padding: '10px 14px',
            borderRadius: '12px',
            border: '1px solid #e2e8f0'
          }}>
            <div style={{ fontSize: '11px', color: '#64748b', fontWeight: '600', textTransform: 'uppercase' }}>
              📍 Coordinates
            </div>
            <div style={{ fontSize: '14px', fontWeight: '700', color: '#0f172a', marginTop: '2px' }}>
              {lat.toFixed(4)}° N, {lng.toFixed(4)}° E
            </div>
            <div style={{
              fontSize: '11px',
              fontWeight: '700',
              marginTop: '2px',
              color: locationStatusType === 'LIVE' ? '#16a34a' : locationStatusType === 'STORED' ? '#d97706' : '#2563eb'
            }}>
              {locationStatusLabel}
            </div>
          </div>

          {/* Safety Status */}
          <div style={{
            background: '#ffffff',
            padding: '10px 14px',
            borderRadius: '12px',
            border: '1px solid #e2e8f0'
          }}>
            <div style={{ fontSize: '11px', color: '#64748b', fontWeight: '600', textTransform: 'uppercase' }}>
              🛡️ Safety Status
            </div>
            <div style={{
              display: 'inline-block',
              marginTop: '4px',
              padding: '2px 8px',
              borderRadius: '8px',
              fontSize: '13px',
              fontWeight: '800',
              backgroundColor: isEmergency ? '#ffebee' : '#e8f5e8',
              color: isEmergency ? '#ff3b30' : '#34c759'
            }}>
              {isEmergency ? `🚨 ${stateName}` : '✅ Safe & Normal'}
            </div>
          </div>

          {/* Connected Device */}
          <div style={{
            background: '#ffffff',
            padding: '10px 14px',
            borderRadius: '12px',
            border: '1px solid #e2e8f0'
          }}>
            <div style={{ fontSize: '11px', color: '#64748b', fontWeight: '600', textTransform: 'uppercase' }}>
              📟 Wearable Device
            </div>
            <div style={{ fontSize: '14px', fontWeight: '700', color: '#0f172a', marginTop: '2px' }}>
              {deviceId}
            </div>
            <div style={{ fontSize: '11px', color: sensorData?.beltWorn ? '#16a34a' : '#dc2626', fontWeight: '600', marginTop: '2px' }}>
              {sensorData?.beltWorn ? '🟢 Belt Worn' : '🔴 Belt Off'}
            </div>
          </div>

          {/* Travel Distance Card */}
          <div style={{
            background: isNavigating ? 'linear-gradient(135deg, #10b981, #059669)' : '#ffffff',
            color: isNavigating ? '#ffffff' : '#0f172a',
            padding: '10px 14px',
            borderRadius: '12px',
            border: '1px solid #e2e8f0'
          }}>
            <div style={{ fontSize: '11px', color: isNavigating ? 'rgba(255,255,255,0.9)' : '#64748b', fontWeight: '600', textTransform: 'uppercase' }}>
              🚗 Travel Route Distance
            </div>
            <div style={{ fontSize: '14px', fontWeight: '800', marginTop: '2px' }}>
              {distanceKm ? `~${distanceKm} km away` : 'Not navigating'}
            </div>
            <div style={{ fontSize: '11px', opacity: 0.9, marginTop: '2px' }}>
              {isNavigating ? '🟢 Live Route Plotted' : 'Click "Start Traveling" below'}
            </div>
          </div>
        </div>

        {/* Location Information Banner */}
        <div style={{
          backgroundColor: '#f0f9ff',
          borderBottom: '1px solid #bae6fd',
          padding: '8px 20px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          fontSize: '12px',
          color: '#0369a1',
          fontWeight: '600'
        }}>
          <span>💡 Tip: Click anywhere on the map to manually set senior pin, or use "Test Hardware GPS Fix" to simulate hardware stream.</span>
        </div>

        {geoError && (
          <div style={{
            backgroundColor: '#fef2f2',
            borderBottom: '1px solid #fecaca',
            padding: '8px 20px',
            fontSize: '12px',
            color: '#dc2626',
            fontWeight: '600'
          }}>
            ⚠️ {geoError}
          </div>
        )}

        {/* Leaflet Map Container */}
        <div style={{ flex: 1, minHeight: '380px', position: 'relative', width: '100%' }}>
          <div ref={mapRef} style={{ width: '100%', height: '100%', minHeight: '380px' }} />
        </div>

        {/* Footer Action Bar with Navigation Button */}
        <div style={{
          padding: '14px 20px',
          background: '#ffffff',
          borderTop: '1px solid #e5e5e7',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          gap: '12px',
          flexWrap: 'wrap'
        }}>
          <div style={{ fontSize: '12px', color: '#6c6c70' }}>
            Location for <strong>{elderlyName}</strong> ({lat.toFixed(4)}°, {lng.toFixed(4)}°)
          </div>

          <div style={{ display: 'flex', gap: '10px', flexWrap: 'wrap' }}>
            <button
              onClick={handleRecenter}
              style={{
                backgroundColor: 'rgba(0, 122, 255, 0.1)',
                color: '#007AFF',
                border: '1px solid rgba(0, 122, 255, 0.2)',
                padding: '10px 16px',
                borderRadius: '12px',
                fontSize: '13px',
                fontWeight: '700',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '6px'
              }}
            >
              🎯 Recenter Pin
            </button>

            {/* 🚗 Primary Button to Start Traveling towards Senior */}
            <button
              onClick={handleStartTraveling}
              style={{
                backgroundColor: '#10b981',
                color: '#ffffff',
                border: 'none',
                padding: '10px 20px',
                borderRadius: '12px',
                fontSize: '14px',
                fontWeight: '800',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                boxShadow: '0 4px 14px rgba(16, 185, 129, 0.35)',
                transition: 'all 0.2s ease'
              }}
            >
              🚗 Start Traveling (Get Directions)
            </button>
          </div>
        </div>

      </div>
    </div>
  );
}
