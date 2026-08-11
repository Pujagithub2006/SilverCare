package com.silvercare.controller;

import com.silvercare.dto.ApiResponse;
import com.silvercare.dto.SensorDataRequest;
import com.silvercare.dto.TwilioSmsRequest;
import com.silvercare.service.FallDetectionService;
import com.silvercare.service.SensorDataService;
import com.silvercare.service.TwilioService;
import com.silvercare.service.ElderlyManagementService;
import com.silvercare.repository.ElderlyRepository;
import com.silvercare.entity.Elderly;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.time.LocalDateTime;
import java.util.LinkedHashMap;
import java.util.Map;

@RestController
@CrossOrigin(originPatterns = "*", allowedHeaders = "*")
public class SensorHardwareController {

    @Autowired
    private SensorDataService sensorDataService;

    @Autowired
    private FallDetectionService fallDetectionService;

    @Autowired
    private TwilioService twilioService;

    @Autowired
    private ElderlyRepository elderlyRepository;

    @Autowired
    private ElderlyManagementService elderlyManagementService;

    @PostMapping("/api/sensor-data")
    public ResponseEntity<Map<String, Object>> receiveSensorData(@RequestBody SensorDataRequest request) {
        if (request == null) {
            return ResponseEntity.status(HttpStatus.BAD_REQUEST)
                    .body(Map.of("status", "error", "message", "No data received"));
        }
        Map<String, Object> result = sensorDataService.receiveSensorData(request);
        return ResponseEntity.ok(result);
    }

    @GetMapping("/api/sensor-data")
    public ResponseEntity<Map<String, Object>> getSensorData(@RequestParam(required = false) String deviceId) {
        if (deviceId == null) {
            return ResponseEntity.ok(Map.of("status", "error", "message", "deviceId is required"));
        }
        return ResponseEntity.ok(sensorDataService.getLatestSensorData(deviceId));
    }

    @GetMapping("/sensor-data")
    public ResponseEntity<ApiResponse<Object>> getSensorDataCompatibility() {
        Map<String, Object> data = Map.of(
                "deviceId", "no_device",
                "heartRate", 0,
                "spo2", 0,
                "temperature", 0,
                "beltWorn", false,
                "stateName", "normal",
                "message", "No hardware connected"
        );
        ApiResponse<Object> response = ApiResponse.builder()
                .status("success")
                .data(data)
                .build();
        return ResponseEntity.ok(response);
    }

    @GetMapping({"/hardware-data/{elderlyId}", "/api/hardware-data/{elderlyId}"})
    public ResponseEntity<ApiResponse<Object>> getHardwareData(@PathVariable String elderlyId) {
        try {
            System.out.println("[HARDWARE DATA] Request for elderlyId: " + elderlyId);
            
            Elderly elderly = elderlyManagementService.getElderly(elderlyId);
            if (elderly == null) {
                elderly = elderlyManagementService.createElderlyForId(elderlyId);
            }
            
            String deviceId = (elderly.getPrimaryDeviceId() != null && !elderly.getPrimaryDeviceId().trim().isEmpty())
                    ? elderly.getPrimaryDeviceId() : "vois_belt";
            System.out.println("[HARDWARE DATA] Found elderly: " + elderly.getName() + ", deviceId: " + deviceId);

            Map<String, Object> sensorData = sensorDataService.getLatestSensorData(deviceId);
            if (sensorData == null) sensorData = new LinkedHashMap<>();

            boolean isDeviceConnected = Boolean.TRUE.equals(sensorData.get("device_connected"));
            Object lastSeen = sensorData.get("last_update");
            Object msg = sensorData.get("message");

            @SuppressWarnings("unchecked")
            Map<String, Object> innerData = (sensorData.get("data") instanceof Map) 
                    ? (Map<String, Object>) sensorData.get("data") : null;

            Object hr = innerData != null ? innerData.get("heartRate") : sensorData.get("heartRate");
            Object spo2 = innerData != null ? innerData.get("spo2") : sensorData.get("spo2");
            Object temp = innerData != null ? innerData.get("temperature") : sensorData.get("temperature");

            boolean beltConnected = isDeviceConnected;
            if (innerData != null && innerData.containsKey("beltWorn")) {
                beltConnected = isDeviceConnected && Boolean.TRUE.equals(innerData.get("beltWorn"));
            }

            boolean bandConnected = isDeviceConnected;
            if (innerData != null && innerData.containsKey("bandConnected")) {
                bandConnected = isDeviceConnected && Boolean.TRUE.equals(innerData.get("bandConnected"));
            } else if (innerData != null && innerData.containsKey("wristBandWorn")) {
                bandConnected = isDeviceConnected && Boolean.TRUE.equals(innerData.get("wristBandWorn"));
            }

            Map<String, Object> data = new LinkedHashMap<>();
            data.put("heartRate", hr != null ? hr : 0);
            data.put("oxygenLevel", spo2 != null ? spo2 : 0);
            data.put("temperature", temp != null ? temp : 0);
            data.put("beltConnected", beltConnected);
            data.put("bandConnected", bandConnected);
            data.put("beltWorn", beltConnected);
            data.put("bandWorn", bandConnected);
            data.put("beltLastSeen", lastSeen != null ? lastSeen.toString() : "");
            data.put("lastUpdate", lastSeen != null ? lastSeen.toString() : LocalDateTime.now().toString());
            data.put("message", msg != null ? msg.toString() : (isDeviceConnected ? "Hardware connected" : "Hardware disconnected"));

            return ResponseEntity.ok(ApiResponse.builder()
                    .status("success")
                    .data(data)
                    .build());
        } catch (Exception e) {
            System.out.println("[HARDWARE DATA] Error: " + e.getMessage());
            e.printStackTrace();
            Map<String, Object> data = new LinkedHashMap<>();
            data.put("heartRate", 0);
            data.put("oxygenLevel", 0);
            data.put("temperature", 0);
            data.put("beltConnected", false);
            data.put("bandConnected", false);
            data.put("beltLastSeen", "");
            data.put("lastUpdate", LocalDateTime.now().toString());
            data.put("message", "Error loading hardware data: " + e.getMessage());
            return ResponseEntity.ok(ApiResponse.builder().status("success").data(data).build());
        }
    }

    @GetMapping("/api/device-status")
    public ResponseEntity<Map<String, Object>> getDeviceStatus() {
        return ResponseEntity.ok(sensorDataService.getDeviceStatus());
    }

    @PostMapping("/api/twilio-sms")
    public ResponseEntity<ApiResponse<Object>> handleTwilioSms(@RequestBody TwilioSmsRequest request) {
        try {
            String deviceId = request.getDeviceId() != null ? request.getDeviceId() : "unknown";
            String guardianPhone = fallDetectionService.getGuardianPhoneForElderly(deviceId);

            if (guardianPhone != null && !guardianPhone.isEmpty()) {
                boolean success = twilioService.sendFallAlertSms(guardianPhone, deviceId, "Home", request.getMessage());
                if (success) {
                    ApiResponse<Object> response = ApiResponse.builder()
                            .status("success")
                            .message("Twilio SMS sent successfully")
                            .phone(guardianPhone)
                            .build();
                    return ResponseEntity.ok(response);
                } else {
                    return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR)
                            .body(ApiResponse.error("Twilio SMS failed"));
                }
            } else {
                return ResponseEntity.status(HttpStatus.NOT_FOUND)
                        .body(ApiResponse.error("No guardian phone found"));
            }
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR)
                    .body(ApiResponse.error(e.getMessage()));
        }
    }

    @PostMapping("/api/fall-alert")
    public ResponseEntity<ApiResponse<Object>> triggerFallAlertEndpoint(@RequestBody Map<String, Object> body) {
        try {
            String deviceId = body.getOrDefault("deviceId", "test_device").toString();
            fallDetectionService.triggerFall(deviceId, 2.5);
            fallDetectionService.notifyGuardianFall("Test User", deviceId, "Home");
            ApiResponse<Object> response = ApiResponse.builder()
                    .status("success")
                    .message("Fall alert triggered for " + deviceId)
                    .build();
            return ResponseEntity.ok(response);
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR)
                    .body(ApiResponse.error(e.getMessage()));
        }
    }
}
