package com.silvercare.service;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.silvercare.dto.SensorDataRequest;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;

import java.time.LocalDateTime;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;

@Service
public class SensorDataService {

    @Autowired
    private FallDetectionService fallDetectionService;

    @Autowired
    private FirebaseEncryptionService firebaseEncryptionService;

    private final ObjectMapper objectMapper = new ObjectMapper();

    public static class DeviceStatus {
        private String deviceId;
        private String beltType;
        private LocalDateTime lastSeen;
        private String state;
        private Boolean beltWorn;

        public DeviceStatus(String deviceId, String beltType, String state, Boolean beltWorn) {
            this.deviceId = deviceId;
            this.beltType = beltType;
            this.state = state;
            this.beltWorn = beltWorn;
            this.lastSeen = LocalDateTime.now();
        }

        public String getDeviceId() { return deviceId; }
        public String getBeltType() { return beltType; }
        public LocalDateTime getLastSeen() { return lastSeen; }
        public String getState() { return state; }
        public Boolean getBeltWorn() { return beltWorn; }
        public void setLastSeen(LocalDateTime lastSeen) { this.lastSeen = lastSeen; }
        public void setBeltType(String beltType) { this.beltType = beltType; }
        public void setState(String state) { this.state = state; }
        public void setBeltWorn(Boolean beltWorn) { this.beltWorn = beltWorn; }
    }

    private final Map<String, Map<String, Object>> latestSensorDataContainer = new ConcurrentHashMap<>();
    private final Map<String, DeviceStatus> connectedDevices = new ConcurrentHashMap<>();
    private final Map<String, double[]> lastKnownCoordinatesMap = new ConcurrentHashMap<>();

    public double[] getLastKnownCoordinates(String deviceId) {
        if (deviceId != null && lastKnownCoordinatesMap.containsKey(deviceId)) {
            return lastKnownCoordinatesMap.get(deviceId);
        }
        return null;
    }

    public Map<String, Object> receiveSensorData(SensorDataRequest request) {
        String deviceId = request.getDeviceId();
        if (deviceId == null || deviceId.trim().isEmpty()) {
            deviceId = "vois_belt";
        }
        System.out.println("[SENSOR DATA] Received data from deviceId: " + deviceId + " at " + LocalDateTime.now());
        String stateName = request.getStateName() != null ? request.getStateName() : "NORMAL";

        // Determine Belt Type: ESP32 waist belt vs ESP32C3 wrist belt
        String beltType = request.getBeltType();
        if (beltType == null || beltType.isEmpty()) {
            if (deviceId.toLowerCase().contains("c3") || deviceId.toLowerCase().contains("wrist")) {
                beltType = "Wrist Belt";
            } else {
                beltType = "Waist Belt";
            }
        }

        Double lat = request.getLatitude();
        Double lng = request.getLongitude();
        boolean isLastLocationFetched = false;
        String locationWarning = null;

        if (lat != null && lng != null) {
            lastKnownCoordinatesMap.put(deviceId, new double[]{lat, lng});
        } else if (lastKnownCoordinatesMap.containsKey(deviceId)) {
            double[] lastCoords = lastKnownCoordinatesMap.get(deviceId);
            lat = lastCoords[0];
            lng = lastCoords[1];
            isLastLocationFetched = true;
            locationWarning = "Unable to fetch current location, last location is fetched";
        } else {
            isLastLocationFetched = false;
            locationWarning = "Unable to fetch location: No GPS data available";
        }

        Map<String, Object> sensorData = new LinkedHashMap<>();
        sensorData.put("deviceId", deviceId);
        sensorData.put("beltType", beltType);
        sensorData.put("state", request.getState() != null ? request.getState() : 0);
        sensorData.put("stateName", stateName);
        
        // Heart rate: use from request, or simulate if wrist band not connected
        Double heartRate = request.getHeartRate();
        boolean bandConnected = request.getBandConnected() != null ? request.getBandConnected() : (request.getWristBandWorn() != null ? request.getWristBandWorn() : true);
        
        if (heartRate == null || !bandConnected) {
            // Simulate heart rate if wrist band data is missing
            heartRate = 70.0 + (Math.random() * 20); // Random between 70-90
            sensorData.put("heartRateSimulated", true);
        } else {
            sensorData.put("heartRateSimulated", false);
        }
        sensorData.put("heartRate", heartRate);
        
        // SpO2: use from request, or simulate if wrist band not connected
        Double spo2 = request.getSpo2();
        if (spo2 == null || !bandConnected) {
            spo2 = 97.0 + (Math.random() * 3); // Random between 97-100
            sensorData.put("spo2Simulated", true);
        } else {
            sensorData.put("spo2Simulated", false);
        }
        sensorData.put("spo2", spo2);
        
        sensorData.put("temperature", request.getTemperature() != null ? request.getTemperature() : 36.6);
        boolean beltWornVal = request.getBeltWorn() != null ? request.getBeltWorn() : true;
        sensorData.put("beltWorn", beltWornVal);
        sensorData.put("bandConnected", bandConnected);
        sensorData.put("wristBandWorn", bandConnected);
        sensorData.put("acceleration", request.getAcceleration() != null ? request.getAcceleration() : 1.0);
        sensorData.put("latitude", lat);
        sensorData.put("longitude", lng);
        sensorData.put("isLastLocationFetched", isLastLocationFetched);
        sensorData.put("locationWarning", locationWarning);
        sensorData.put("micMessageAudio", request.getMicMessageAudio());
        sensorData.put("timestamp", request.getTimestamp() != null ? request.getTimestamp() : System.currentTimeMillis());
        sensorData.put("received_at", LocalDateTime.now().toString());

        Map<String, Object> container = new ConcurrentHashMap<>();
        container.put("status", "success");
        container.put("data", sensorData);
        container.put("last_update", LocalDateTime.now().toString());
        container.put("device_connected", true);
        
        latestSensorDataContainer.put(deviceId, container);

        connectedDevices.put(deviceId, new DeviceStatus(deviceId, beltType, stateName, beltWornVal));

        System.out.printf("📡 [HARDWARE %s] Data from %s | State: %s | Worn: %s | HR: %.1f | SpO2: %.1f | Temp: %.1f%n",
                beltType, deviceId, stateName, beltWornVal ? "YES" : "NO",
                (Double) sensorData.get("heartRate"), (Double) sensorData.get("spo2"), (Double) sensorData.get("temperature"));

        // Encrypt & Save to Firebase storage
        try {
            String rawJson = objectMapper.writeValueAsString(sensorData);
            firebaseEncryptionService.saveToFirebaseEncrypted("telemetry", deviceId, rawJson);
        } catch (Exception e) {
            System.err.println("Failed to serialize telemetry for Firebase: " + e.getMessage());
        }

        // Process State Transitions
        if ("PREFALL".equalsIgnoreCase(stateName)) {
            System.out.println("[PREFALL DETECTED] Triggering prefall notification to ALL linked guardians for " + deviceId);
            fallDetectionService.handlePrefallAlert(deviceId, beltType, lat, lng, request.getMicMessageAudio());
        } else if ("FALL_DETECTED".equalsIgnoreCase(stateName)) {
            System.out.println("[FALL DETECTED] Triggering fall escalation workflow for " + deviceId);
            fallDetectionService.triggerFall(deviceId, 1.0);
            fallDetectionService.handleFallAlert(deviceId, beltType, lat, lng, request.getMicMessageAudio());
        }

        Map<String, Object> response = new LinkedHashMap<>();
        response.put("status", "success");
        response.put("message", "Data received successfully");
        response.put("beltType", beltType);
        response.put("timestamp", LocalDateTime.now().toString());
        return response;
    }

    public Map<String, Object> getLatestSensorData(String deviceId) {
        String targetId = (deviceId != null && !deviceId.trim().isEmpty()) ? deviceId : "vois_belt";
        
        if (!latestSensorDataContainer.containsKey(targetId)) {
            Map<String, Object> disconnectedData = new LinkedHashMap<>();
            disconnectedData.put("deviceId", targetId);
            disconnectedData.put("beltType", "Waist Belt");
            disconnectedData.put("beltWorn", false);
            disconnectedData.put("bandConnected", false);
            disconnectedData.put("wristBandWorn", false);
            disconnectedData.put("heartRate", 0);
            disconnectedData.put("spo2", 0);
            disconnectedData.put("temperature", 0);
            disconnectedData.put("acceleration", 0);
            disconnectedData.put("stateName", "DISCONNECTED");
            disconnectedData.put("latitude", null);
            disconnectedData.put("longitude", null);
            disconnectedData.put("isLastLocationFetched", false);
            disconnectedData.put("locationWarning", "No hardware connected");

            Map<String, Object> container = new LinkedHashMap<>();
            container.put("status", "success");
            container.put("device_connected", false);
            container.put("data", disconnectedData);
            container.put("message", "No hardware connected for device: " + targetId);
            return container;
        }

        Map<String, Object> container = new LinkedHashMap<>(latestSensorDataContainer.get(targetId));
        if (container.containsKey("last_update")) {
            String lastUpdateStr = (String) container.get("last_update");
            LocalDateTime lastUpdate = LocalDateTime.parse(lastUpdateStr);
            // Increased timeout from 5s to 10s for more tolerance to network delays
            if (java.time.Duration.between(lastUpdate, LocalDateTime.now()).getSeconds() > 10) {
                System.out.println("[SENSOR DATA] Device " + targetId + " timeout - last update: " + lastUpdateStr);
                container.put("device_connected", false);
                if (container.get("data") instanceof Map) {
                    @SuppressWarnings("unchecked")
                    Map<String, Object> innerData = new LinkedHashMap<>((Map<String, Object>) container.get("data"));
                    innerData.put("beltWorn", false);
                    innerData.put("bandConnected", false);
                    innerData.put("wristBandWorn", false);
                    innerData.put("heartRate", 0);
                    innerData.put("spo2", 0);
                    innerData.put("temperature", 0);
                    innerData.put("stateName", "DISCONNECTED");
                    container.put("data", innerData);
                }
            }
        }
        return container;
    }

    public Map<String, Object> getDeviceStatus() {
        LocalDateTime now = LocalDateTime.now();
        Map<String, Object> activeDevices = new LinkedHashMap<>();

        connectedDevices.entrySet().removeIf(entry ->
                java.time.Duration.between(entry.getValue().getLastSeen(), now).getSeconds() > 60);

        for (Map.Entry<String, DeviceStatus> entry : connectedDevices.entrySet()) {
            long diffSec = java.time.Duration.between(entry.getValue().getLastSeen(), now).getSeconds();
            Map<String, Object> devInfo = new LinkedHashMap<>();
            devInfo.put("device_id", entry.getKey());
            devInfo.put("belt_type", entry.getValue().getBeltType());
            devInfo.put("last_seen", entry.getValue().getLastSeen().toString());
            devInfo.put("state", entry.getValue().getState());
            devInfo.put("belt_worn", entry.getValue().getBeltWorn());
            devInfo.put("connection_status", diffSec <= 10 ? "online" : "stale");
            devInfo.put("last_seen_seconds", diffSec);
            activeDevices.put(entry.getKey(), devInfo);
        }

        Map<String, Object> response = new LinkedHashMap<>();
        response.put("status", "success");
        response.put("devices", activeDevices);
        response.put("total_devices", activeDevices.size());
        return response;
    }
}
