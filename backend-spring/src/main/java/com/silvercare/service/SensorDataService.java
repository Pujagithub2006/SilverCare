package com.silvercare.service;

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

    public static class DeviceStatus {
        private String deviceId;
        private LocalDateTime lastSeen;
        private String state;
        private Boolean beltWorn;

        public DeviceStatus(String deviceId, String state, Boolean beltWorn) {
            this.deviceId = deviceId;
            this.state = state;
            this.beltWorn = beltWorn;
            this.lastSeen = LocalDateTime.now();
        }

        public String getDeviceId() { return deviceId; }
        public LocalDateTime getLastSeen() { return lastSeen; }
        public String getState() { return state; }
        public Boolean getBeltWorn() { return beltWorn; }
        public void setLastSeen(LocalDateTime lastSeen) { this.lastSeen = lastSeen; }
        public void setState(String state) { this.state = state; }
        public void setBeltWorn(Boolean beltWorn) { this.beltWorn = beltWorn; }
    }

    private final Map<String, Object> latestSensorDataContainer = new ConcurrentHashMap<>();
    private final Map<String, DeviceStatus> connectedDevices = new ConcurrentHashMap<>();

    public Map<String, Object> receiveSensorData(SensorDataRequest request) {
        String deviceId = request.getDeviceId() != null ? request.getDeviceId() : "unknown";
        String stateName = request.getStateName() != null ? request.getStateName() : "UNKNOWN";

        Map<String, Object> sensorData = new LinkedHashMap<>();
        sensorData.put("deviceId", deviceId);
        sensorData.put("state", request.getState() != null ? request.getState() : 0);
        sensorData.put("stateName", stateName);
        sensorData.put("heartRate", request.getHeartRate() != null ? request.getHeartRate() : 0.0);
        sensorData.put("spo2", request.getSpo2() != null ? request.getSpo2() : 0.0);
        sensorData.put("temperature", request.getTemperature() != null ? request.getTemperature() : 0.0);
        sensorData.put("beltWorn", request.getBeltWorn() != null ? request.getBeltWorn() : false);
        sensorData.put("acceleration", request.getAcceleration() != null ? request.getAcceleration() : 0.0);
        sensorData.put("timestamp", request.getTimestamp() != null ? request.getTimestamp() : System.currentTimeMillis());
        sensorData.put("received_at", LocalDateTime.now().toString());

        latestSensorDataContainer.put("status", "success");
        latestSensorDataContainer.put("data", sensorData);
        latestSensorDataContainer.put("last_update", LocalDateTime.now().toString());
        latestSensorDataContainer.put("device_connected", true);

        connectedDevices.put(deviceId, new DeviceStatus(deviceId, stateName, request.getBeltWorn()));

        System.out.printf("📡 [HARDWARE] Data received from %s | State: %s | HR: %.1f | SpO2: %.1f | Temp: %.1f%n",
                deviceId, stateName, request.getHeartRate(), request.getSpo2(), request.getTemperature());

        if ("FALL_DETECTED".equalsIgnoreCase(stateName)) {
            System.out.println("🚨 [FALL DETECTED] Triggering fall workflow for " + deviceId);
            fallDetectionService.triggerFall(deviceId, 1.0);
            fallDetectionService.notifyGuardianFall("User", deviceId, "Home");
        }

        Map<String, Object> response = new LinkedHashMap<>();
        response.put("status", "success");
        response.put("message", "Data received successfully");
        response.put("timestamp", LocalDateTime.now().toString());
        return response;
    }

    public Map<String, Object> getLatestSensorData() {
        if (latestSensorDataContainer.containsKey("last_update")) {
            String lastUpdateStr = (String) latestSensorDataContainer.get("last_update");
            LocalDateTime lastUpdate = LocalDateTime.parse(lastUpdateStr);
            if (java.time.Duration.between(lastUpdate, LocalDateTime.now()).getSeconds() > 10) {
                latestSensorDataContainer.put("device_connected", false);
            }
        }
        return new LinkedHashMap<>(latestSensorDataContainer);
    }

    public Map<String, Object> getDeviceStatus() {
        LocalDateTime now = LocalDateTime.now();
        Map<String, Object> activeDevices = new LinkedHashMap<>();

        connectedDevices.entrySet().removeIf(entry ->
                java.time.Duration.between(entry.getValue().getLastSeen(), now).getSeconds() > 30);

        for (Map.Entry<String, DeviceStatus> entry : connectedDevices.entrySet()) {
            long diffSec = java.time.Duration.between(entry.getValue().getLastSeen(), now).getSeconds();
            Map<String, Object> devInfo = new LinkedHashMap<>();
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
