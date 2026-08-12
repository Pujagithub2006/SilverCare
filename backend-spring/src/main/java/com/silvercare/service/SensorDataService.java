package com.silvercare.service;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.silvercare.dto.SensorDataRequest;
import com.silvercare.entity.Device;
import com.silvercare.entity.Elderly;
import com.silvercare.repository.DeviceRepository;
import com.silvercare.repository.ElderlyRepository;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;

import java.time.LocalDateTime;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;

@Service
public class SensorDataService {
    // SilverCare Telemetry Engine - Auto-deploy Verification Test

    @Autowired
    private FallDetectionService fallDetectionService;

    @Autowired
    private FirebaseEncryptionService firebaseEncryptionService;

    @Autowired
    private DeviceRepository deviceRepository;

    @Autowired
    private ElderlyRepository elderlyRepository;

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

        public String getDeviceId() {
            return deviceId;
        }

        public String getBeltType() {
            return beltType;
        }

        public LocalDateTime getLastSeen() {
            return lastSeen;
        }

        public String getState() {
            return state;
        }

        public Boolean getBeltWorn() {
            return beltWorn;
        }

        public void setLastSeen(LocalDateTime lastSeen) {
            this.lastSeen = lastSeen;
        }

        public void setBeltType(String beltType) {
            this.beltType = beltType;
        }

        public void setState(String state) {
            this.state = state;
        }

        public void setBeltWorn(Boolean beltWorn) {
            this.beltWorn = beltWorn;
        }
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

    public Set<String> generateKeysForDevice(String deviceId, String macAddress) {
        Set<String> keys = new LinkedHashSet<>();
        if (deviceId != null && !deviceId.trim().isEmpty()) {
            String cleanDev = deviceId.trim();
            keys.add(cleanDev);
            keys.add(cleanDev.toLowerCase());
            keys.add(cleanDev.toUpperCase());
        }

        String macToUse = macAddress;
        if (macToUse == null || macToUse.trim().isEmpty()) {
            macToUse = deviceId;
        }

        if (macToUse != null && !macToUse.trim().isEmpty()) {
            String rawMac = macToUse.trim();
            keys.add(rawMac);
            keys.add(rawMac.toLowerCase());
            keys.add(rawMac.toUpperCase());

            // Extract 12-character hex sequence if present (e.g. 0000B05A0D40 inside
            // SC_WB_0000B05A0D40_1A2B)
            String cleanHex = rawMac.replaceAll("[^a-zA-Z0-9]", "");
            java.util.regex.Matcher matcher = java.util.regex.Pattern.compile("[a-fA-F0-9]{12}").matcher(cleanHex);
            List<String> foundHexes = new ArrayList<>();
            while (matcher.find()) {
                foundHexes.add(matcher.group());
            }
            if (foundHexes.isEmpty() && !cleanHex.isEmpty()) {
                foundHexes.add(cleanHex);
            }

            for (String hex : foundHexes) {
                keys.add(hex);
                keys.add(hex.toLowerCase());
                keys.add(hex.toUpperCase());
                keys.add("DEV_" + hex.toUpperCase());
                keys.add("device_" + hex.toLowerCase());

                if (hex.length() == 12) {
                    StringBuilder formatted = new StringBuilder();
                    for (int i = 0; i < 12; i += 2) {
                        if (i > 0)
                            formatted.append(":");
                        formatted.append(hex.substring(i, i + 2));
                    }
                    keys.add(formatted.toString().toUpperCase());
                    keys.add(formatted.toString().toLowerCase());
                }
            }
        }
        return keys;
    }

    public Map<String, Object> receiveSensorData(SensorDataRequest request) {
        String deviceId = request.getDeviceId() != null ? request.getDeviceId() : "vois_belt";
        String macAddress = request.getMacAddress();
        if (macAddress == null || macAddress.trim().isEmpty()) {
            macAddress = deviceId;
        }
        // Use MAC address as the single key for the entire system
        String storageKey = (macAddress != null && !macAddress.trim().isEmpty()) ? macAddress : deviceId;
        System.out.println("[SENSOR DATA] Received data from deviceId: " + deviceId + " (mac: " + macAddress
                + ") using storageKey: " + storageKey + " at " + LocalDateTime.now());
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
            lastKnownCoordinatesMap.put(deviceId, new double[] { lat, lng });
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
        boolean bandConnected = request.getBandConnected() != null ? request.getBandConnected()
                : (request.getWristBandWorn() != null ? request.getWristBandWorn() : true);

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
        Double accelVal = request.getAcceleration();
        Double ax = request.getAccelX();
        Double ay = request.getAccelY();
        Double az = request.getAccelZ();

        // Process actual sensitivised acceleration value (bounded strictly between 9.0 and 25.0 m/s^2)
        if (accelVal == null) {
            if (ax != null || ay != null || az != null) {
                double x = ax != null ? ax : 0.0;
                double y = ay != null ? ay : 0.0;
                double z = az != null ? az : 0.0;
                double raw = Math.sqrt(x * x + y * y + z * z);
                double delta = Math.abs(raw - 9.81);
                double filteredDelta = delta > 0.25 ? (delta - 0.25) : 0.0;
                accelVal = 9.81 + (filteredDelta * 6.0);
            } else {
                accelVal = 9.81;
            }
        }

        // Clamp strictly between 9.0 m/s^2 and 25.0 m/s^2 as requested
        accelVal = Math.min(25.0, Math.max(9.0, accelVal));
        sensorData.put("acceleration", accelVal);
        if (ax != null) sensorData.put("accelX", ax);
        if (ay != null) sensorData.put("accelY", ay);
        if (az != null) sensorData.put("accelZ", az);
        if (request.getGyroX() != null) sensorData.put("gyroX", request.getGyroX());
        if (request.getGyroY() != null) sensorData.put("gyroY", request.getGyroY());
        if (request.getGyroZ() != null) sensorData.put("gyroZ", request.getGyroZ());
        sensorData.put("latitude", lat);
        sensorData.put("longitude", lng);
        sensorData.put("isLastLocationFetched", isLastLocationFetched);
        sensorData.put("locationWarning", locationWarning);
        sensorData.put("micMessageAudio", request.getMicMessageAudio());
        sensorData.put("timestamp",
                request.getTimestamp() != null ? request.getTimestamp() : System.currentTimeMillis());
        sensorData.put("received_at", LocalDateTime.now().toString());

        Set<String> targetKeys = generateKeysForDevice(deviceId, macAddress);
        targetKeys.addAll(generateKeysForDevice("device_3076f5f44e24", "30:76:F5:F4:4E:24"));
        targetKeys.add("vois_belt");
        targetKeys.add("default_senior");

        try {
            List<Elderly> allElderly = elderlyRepository.findAll();
            for (Elderly e : allElderly) {
                if (e.getElderlyId() != null) {
                    targetKeys.add(e.getElderlyId());
                    targetKeys.add(e.getElderlyId().toLowerCase());
                }
                if (e.getPrimaryDeviceId() != null) {
                    targetKeys.add(e.getPrimaryDeviceId());
                    targetKeys.add(e.getPrimaryDeviceId().toLowerCase());
                }
                if (e.getPrimaryDeviceMac() != null) {
                    targetKeys.addAll(generateKeysForDevice(null, e.getPrimaryDeviceMac()));
                }

                // Auto-pair active hardware MAC address to Elderly if missing or set to default
                if (macAddress != null && !macAddress.trim().isEmpty()) {
                    String cleanIncomingMac = macAddress.trim();
                    if (e.getPrimaryDeviceMac() == null || e.getPrimaryDeviceMac().trim().isEmpty()
                            || "00:00:B0:5A:0D:40".equals(e.getPrimaryDeviceMac())
                            || "30:76:F5:F4:4E:24".equals(e.getPrimaryDeviceMac())) {
                        e.setPrimaryDeviceMac(cleanIncomingMac);
                        e.setPrimaryDeviceId("device_" + cleanIncomingMac.replace(":", "").toLowerCase());
                        try {
                            elderlyRepository.save(e);
                            System.out.println("🔗 [AUTO-PAIR] Updated Elderly '" + e.getElderlyId()
                                    + "' with primaryDeviceMac: " + cleanIncomingMac);
                        } catch (Exception ignored) {
                        }
                    }
                }
            }
        } catch (Exception ignored) {
        }

        Optional<Device> matchedDevice = Optional.empty();
        for (String k : targetKeys) {
            matchedDevice = deviceRepository.findByMacAddress(k);
            if (matchedDevice.isPresent())
                break;
            matchedDevice = deviceRepository.findByDeviceId(k);
            if (matchedDevice.isPresent())
                break;
        }

        if (matchedDevice.isPresent()) {
            Device dev = matchedDevice.get();
            if (dev.getDeviceId() != null)
                targetKeys.addAll(generateKeysForDevice(dev.getDeviceId(), dev.getMacAddress()));
            if (dev.getMacAddress() != null)
                targetKeys.addAll(generateKeysForDevice(null, dev.getMacAddress()));
            if (dev.getAssignedElderlyId() != null) {
                targetKeys.add(dev.getAssignedElderlyId());
                targetKeys.add(dev.getAssignedElderlyId().toLowerCase());
            }

            dev.setStatus("ACTIVE");
            dev.setLastSeen(LocalDateTime.now());
            try {
                deviceRepository.save(dev);
            } catch (Exception ignored) {
            }
        }

        if ("Waist Belt".equalsIgnoreCase(beltType)) {
            targetKeys.add("vois_belt");
        }

        Map<String, Object> container = new LinkedHashMap<>();
        container.put("status", "success");
        container.put("data", sensorData);
        container.put("last_update", LocalDateTime.now().toString());
        container.put("device_connected", true);

        // Store data under all matched MAC and device keys for maximum compatibility
        for (String key : targetKeys) {
            latestSensorDataContainer.put(key, container);
            connectedDevices.put(key, new DeviceStatus(key, beltType, stateName, beltWornVal));
        }

        System.out.printf(
                "📡 [HARDWARE %s] Data from %s (mac: %s) stored under %d keys | State: %s | Worn: %s | HR: %.1f | SpO2: %.1f | Temp: %.1f%n",
                beltType, deviceId, macAddress, targetKeys.size(), stateName, beltWornVal ? "YES" : "NO",
                (Double) sensorData.get("heartRate"), (Double) sensorData.get("spo2"),
                (Double) sensorData.get("temperature"));

        // Encrypt & Save to Firebase storage
        try {
            String rawJson = objectMapper.writeValueAsString(sensorData);
            firebaseEncryptionService.saveToFirebaseEncrypted("telemetry", deviceId, rawJson);
        } catch (Exception e) {
            System.err.println("Failed to serialize telemetry for Firebase: " + e.getMessage());
        }

        // Process State Transitions
        if ("PREFALL".equalsIgnoreCase(stateName)) {
            System.out.println(
                    "[PREFALL DETECTED] Triggering prefall notification to ALL linked guardians for " + deviceId);
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
        String targetId = (deviceId != null && !deviceId.trim().isEmpty()) ? deviceId.trim() : "vois_belt";

        Map<String, Object> foundContainer = latestSensorDataContainer.get(targetId);

        if (foundContainer == null) {
            Set<String> searchKeys = generateKeysForDevice(targetId, null);
            for (String k : searchKeys) {
                if (latestSensorDataContainer.containsKey(k)) {
                    foundContainer = latestSensorDataContainer.get(k);
                    break;
                }
            }
        }

        if (foundContainer == null) {
            Optional<Device> devOpt = deviceRepository.findByDeviceId(targetId);
            if (devOpt.isEmpty())
                devOpt = deviceRepository.findByMacAddress(targetId);
            if (devOpt.isPresent()) {
                Device dev = devOpt.get();
                Set<String> devKeys = generateKeysForDevice(dev.getDeviceId(), dev.getMacAddress());
                if (dev.getAssignedElderlyId() != null)
                    devKeys.add(dev.getAssignedElderlyId());
                for (String k : devKeys) {
                    if (latestSensorDataContainer.containsKey(k)) {
                        foundContainer = latestSensorDataContainer.get(k);
                        break;
                    }
                }
            }
        }

        if (foundContainer == null) {
            Optional<Elderly> eOpt = elderlyRepository.findByElderlyId(targetId);
            if (eOpt.isPresent()) {
                Elderly e = eOpt.get();
                Set<String> eKeys = new HashSet<>();
                if (e.getPrimaryDeviceId() != null)
                    eKeys.addAll(generateKeysForDevice(e.getPrimaryDeviceId(), null));
                if (e.getPrimaryDeviceMac() != null)
                    eKeys.addAll(generateKeysForDevice(null, e.getPrimaryDeviceMac()));
                for (String k : eKeys) {
                    if (latestSensorDataContainer.containsKey(k)) {
                        foundContainer = latestSensorDataContainer.get(k);
                        break;
                    }
                }
            }
        }

        if (foundContainer == null) {
            Set<String> defaultMacKeys = generateKeysForDevice("device_3076f5f44e24", "30:76:F5:F4:4E:24");
            defaultMacKeys.add("vois_belt");
            for (String k : defaultMacKeys) {
                if (latestSensorDataContainer.containsKey(k)) {
                    foundContainer = latestSensorDataContainer.get(k);
                    break;
                }
            }
        }

        // Active Hardware Fallback: If no explicit match found for targetId, fall back
        // to ANY active connected hardware container in memory
        if (foundContainer == null && !latestSensorDataContainer.isEmpty()) {
            for (Map.Entry<String, Map<String, Object>> entry : latestSensorDataContainer.entrySet()) {
                Map<String, Object> candidate = entry.getValue();
                if (candidate != null && Boolean.TRUE.equals(candidate.get("device_connected"))) {
                    foundContainer = candidate;
                    System.out.println("🔄 [ACTIVE FALLBACK] Mapped query targetId '" + targetId
                            + "' to active hardware container key '" + entry.getKey() + "'");
                    break;
                }
            }
        }

        if (foundContainer == null) {
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

        Map<String, Object> container = new LinkedHashMap<>(foundContainer);
        if (container.containsKey("last_update")) {
            String lastUpdateStr = (String) container.get("last_update");
            LocalDateTime lastUpdate = LocalDateTime.parse(lastUpdateStr);
            if (java.time.Duration.between(lastUpdate, LocalDateTime.now()).getSeconds() > 10) {
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
            } else {
                container.put("device_connected", true);
            }
        }
        return container;
    }

    public Map<String, Object> getDeviceStatus() {
        LocalDateTime now = LocalDateTime.now();
        Map<String, Object> activeDevices = new LinkedHashMap<>();

        connectedDevices.entrySet()
                .removeIf(entry -> java.time.Duration.between(entry.getValue().getLastSeen(), now).getSeconds() > 60);

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
