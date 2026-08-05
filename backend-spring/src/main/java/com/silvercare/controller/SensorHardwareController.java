package com.silvercare.controller;

import com.silvercare.dto.ApiResponse;
import com.silvercare.dto.SensorDataRequest;
import com.silvercare.dto.TwilioSmsRequest;
import com.silvercare.service.FallDetectionService;
import com.silvercare.service.SensorDataService;
import com.silvercare.service.TwilioService;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.time.LocalDateTime;
import java.util.Map;

@RestController
public class SensorHardwareController {

    @Autowired
    private SensorDataService sensorDataService;

    @Autowired
    private FallDetectionService fallDetectionService;

    @Autowired
    private TwilioService twilioService;

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
    public ResponseEntity<Map<String, Object>> getSensorData() {
        return ResponseEntity.ok(sensorDataService.getLatestSensorData());
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

    @GetMapping("/hardware-data/{elderlyId}")
    public ResponseEntity<ApiResponse<Object>> getHardwareData(@PathVariable String elderlyId) {
        Map<String, Object> data = Map.of(
                "heartRate", 0,
                "oxygenLevel", 0,
                "temperature", 0,
                "beltConnected", false,
                "beltLastSeen", "",
                "lastUpdate", LocalDateTime.now().toString(),
                "message", "No hardware connected"
        );
        ApiResponse<Object> response = ApiResponse.builder()
                .status("success")
                .data(data)
                .build();
        return ResponseEntity.ok(response);
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
