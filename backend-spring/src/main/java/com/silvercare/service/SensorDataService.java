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

    public Map<String, Object> receiveSensorData(SensorDataRequest request) {
        String deviceId = request.getDeviceId() != null ? request.getDeviceId() : "vois_belt";
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

        Double lat = request.getLatitude() != null ? request.getLatitude() : 18.5204;
        Double lng = request.getLongitude() != null ? request.getLongitude() : 73.8567;

        Map<String, Object> sensorData = new LinkedHashMap<>();
        sensorData.put("deviceId", deviceId);
        sensorData.put("beltType", beltType);
        sensorData.put("state", request.getState() != null ? request.getState() : 0);
        sensorData.put("stateName", stateName);
        sensorData.put("heartRate", request.getHeartRate() != null ? request.getHeartRate() : 72.0);
        sensorData.put("spo2", request.getSpo2() != null ? request.getSpo2() : 98.0);
        sensorData.put("temperature", request.getTemperature() != null ? request.getTemperature() : 36.6);
        sensorData.put("beltWorn", request.getBeltWorn() != null ? request.getBeltWorn() : true);
        sensorData.put("acceleration", request.getAcceleration() != null ? request.getAcceleration() : 1.0);
        sensorData.put("latitude", lat);
        sensorData.put("longitude", lng);
        sensorData.put("micMessageAudio", request.getMicMessageAudio());
        sensorData.put("timestamp", request.getTimestamp() != null ? request.getTimestamp() : System.currentTimeMillis());
        sensorData.put("received_at", LocalDateTime.now().toString());

        Map<String, Object> container = new ConcurrentHashMap<>();
        container.put("status", "success");
        container.put("data", sensorData);
        container.put("last_update", LocalDateTime.now().toString());
        container.put("device_connected", true);
        
        latestSensorDataContainer.put(deviceId, container);

        connectedDevices.put(deviceId, new DeviceStatus(deviceId, beltType, stateName, request.getBeltWorn()));

        System.out.printf("📡 [HARDWARE %s] Data from %s | State: %s | Worn: %s | HR: %.1f | SpO2: %.1f | Temp: %.1f%n",
                beltType, deviceId, stateName, request.getBeltWorn() ? "YES" : "NO",
                request.getHeartRate(), request.getSpo2(), request.getTemperature());

        // Encrypt & Save to Firebase storage
        try {
            String rawJson = objectMapper.writeValueAsString(sensorData);
            firebaseEncryptionService.saveToFirebaseEncrypted("telemetry", deviceId, rawJson);
        } catch (Exception e) {
            System.err.println("Failed to serialize telemetry for Firebase: " + e.getMessage());
        }

        // Process State Transitions
        if ("PREFALL".equalsIgnoreCase(stateName)) {
            System.out.println("⚠️ [PREFALL DETECTED] Triggering prefall notification to ALL linked guardians for " + deviceId);
            fallDetectionService.handlePrefallAlert(deviceId, beltType, lat, lng, request.getMicMessageAudio());
        } else if ("FALL_DETECTED".equalsIgnoreCase(stateName)) {
            System.out.println("🚨 [FALL DETECTED] Triggering fall escalation workflow for " + deviceId);
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
        if (deviceId == null || !latestSensorDataContainer.containsKey(deviceId)) {
             return Map.of(
                "status", "success",
                "device_connected", false,
                "message", "No hardware connected for device: " + (deviceId != null ? deviceId : "unknown")
            );
        }
        Map<String, Object> container = latestSensorDataContainer.get(deviceId);
        if (container.containsKey("last_update")) {
            String lastUpdateStr = (String) container.get("last_update");
            LocalDateTime lastUpdate = LocalDateTime.parse(lastUpdateStr);
            if (java.time.Duration.between(lastUpdate, LocalDateTime.now()).getSeconds() > 15) {
                container.put("device_connected", false);
            }
        }
        return new LinkedHashMap<>(container);
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
