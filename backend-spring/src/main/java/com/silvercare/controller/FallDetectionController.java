package com.silvercare.controller;

import com.silvercare.dto.*;
import com.silvercare.entity.Guardian;
import com.silvercare.service.FallDetectionService;
import com.silvercare.service.GuardianAuthService;
import com.silvercare.service.TwilioService;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.LinkedHashMap;
import java.util.Map;

@RestController
public class FallDetectionController {

    @Autowired
    private FallDetectionService fallDetectionService;

    @Autowired
    private GuardianAuthService guardianAuthService;

    @Autowired
    private TwilioService twilioService;

    @PostMapping("/detect-fall")
    public ResponseEntity<ApiResponse<Object>> detectFall(@RequestBody FallDetectRequest request) {
        try {
            String deviceId = request.getDeviceId() != null ? request.getDeviceId() : "unknown";
            Double confidence = request.getConfidence() != null ? request.getConfidence() : 1.0;
            fallDetectionService.triggerFall(deviceId, confidence);

            ApiResponse<Object> response = ApiResponse.builder()
                    .status("success")
                    .message("Fall detected and alert triggered")
                    .build();
            return ResponseEntity.ok(response);
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping("/fall-status")
    public ResponseEntity<Map<String, Object>> getFallStatus() {
        Map<String, Object> map = new LinkedHashMap<>();
        map.put("fall_detected", fallDetectionService.isFallDetected());
        return ResponseEntity.ok(map);
    }

    @PostMapping("/clear-fall")
    public ResponseEntity<Map<String, String>> clearFall() {
        fallDetectionService.clearFall();
        return ResponseEntity.ok(Map.of("status", "fall_cleared"));
    }

    @PostMapping("/notify-guardian-fall")
    public ResponseEntity<ApiResponse<Object>> notifyGuardianFall(@RequestBody NotifyGuardianFallRequest request) {
        try {
            String elderlyName = request.getElderlyName() != null ? request.getElderlyName() : "User";
            String deviceId = request.getDeviceId() != null ? request.getDeviceId() : "unknown";
            String location = request.getLocation() != null ? request.getLocation() : "Unknown location";

            fallDetectionService.notifyGuardianFall(elderlyName, deviceId, location);

            ApiResponse<Object> response = ApiResponse.builder()
                    .status("success")
                    .message("Guardian notified of potential fall")
                    .name(elderlyName)
                    .build();
            return ResponseEntity.ok(response);
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(ApiResponse.error(e.getMessage()));
        }
    }

    @PostMapping("/notify-guardian-no-response")
    public ResponseEntity<ApiResponse<Object>> notifyGuardianNoResponse(@RequestBody NotifyGuardianFallRequest request) {
        try {
            String elderlyName = request.getElderlyName() != null ? request.getElderlyName() : "User";
            String deviceId = request.getDeviceId() != null ? request.getDeviceId() : "unknown";
            String location = request.getLocation() != null ? request.getLocation() : "Unknown location";

            fallDetectionService.notifyGuardianNoResponse(elderlyName, deviceId, location);

            ApiResponse<Object> response = ApiResponse.builder()
                    .status("success")
                    .message("Guardian notified with urgent sound alert")
                    .name(elderlyName)
                    .build();
            return ResponseEntity.ok(response);
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(ApiResponse.error(e.getMessage()));
        }
    }

    @PostMapping("/notify-guardian-safe")
    public ResponseEntity<ApiResponse<Object>> notifyGuardianSafe(@RequestBody NotifyGuardianFallRequest request) {
        String elderlyName = request.getElderlyName() != null ? request.getElderlyName() : "User";
        System.out.println("[GUARDIAN INFO] " + elderlyName + " confirmed safe (False Alarm)");
        ApiResponse<Object> response = ApiResponse.builder()
                .status("success")
                .message("Guardian notified - false alarm")
                .name(elderlyName)
                .build();
        return ResponseEntity.ok(response);
    }

    @PostMapping("/switch-user")
    public ResponseEntity<ApiResponse<Object>> switchUser(@RequestBody SwitchUserRequest request) {
        if (request.getElderlyId() == null || request.getElderlyId().isEmpty()) {
            return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(ApiResponse.error("elderly_id required"));
        }
        fallDetectionService.switchUser(request.getElderlyId());
        ApiResponse<Object> response = ApiResponse.builder()
                .status("success")
                .message("Device switched to " + request.getElderlyId())
                .current_user(request.getElderlyId())
                .build();
        return ResponseEntity.ok(response);
    }

    @GetMapping("/current-user")
    public ResponseEntity<ApiResponse<Object>> getCurrentUser() {
        String currentUser = fallDetectionService.getCurrentUser();
        ApiResponse<Object> response = ApiResponse.builder()
                .status("success")
                .current_user(currentUser)
                .available_users(fallDetectionService.getAvailableUsers())
                .build();
        return ResponseEntity.ok(response);
    }

    @PostMapping("/emergency-call")
    public ResponseEntity<ApiResponse<Object>> emergencyCall(@RequestBody EmergencyCallRequest request) {
        try {
            String elderlyName = request.getElderlyName() != null ? request.getElderlyName() : "User";
            String guardianUsername = request.getGuardianUsername() != null ? request.getGuardianUsername() : "";
            String location = request.getLocation() != null ? request.getLocation() : "Unknown location";

            Guardian guardian = guardianAuthService.getGuardian(guardianUsername);
            if (guardian == null) {
                return ResponseEntity.status(HttpStatus.NOT_FOUND).body(ApiResponse.error("Guardian not found"));
            }

            if (guardian.getPhone() == null || guardian.getPhone().isEmpty()) {
                return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(ApiResponse.error("Guardian phone number not found"));
            }

            boolean ok = twilioService.makeEmergencyCall(guardian.getPhone(), elderlyName, location);
            if (ok) {
                ApiResponse<Object> response = ApiResponse.builder()
                        .status("success")
                        .message("Emergency call initiated to guardian")
                        .guardian_phone(guardian.getPhone())
                        .guardian_name(guardian.getName())
                        .build();
                return ResponseEntity.ok(response);
            } else {
                return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error("Failed to initiate emergency call"));
            }
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @PostMapping("/urgent-call-no-response")
    public ResponseEntity<ApiResponse<Object>> urgentCallNoResponse(@RequestBody EmergencyCallRequest request) {
        try {
            String elderlyName = request.getElderlyName() != null ? request.getElderlyName() : "User";
            String guardianUsername = request.getGuardianUsername() != null ? request.getGuardianUsername() : "";
            String location = request.getLocation() != null ? request.getLocation() : "Unknown location";

            Guardian guardian = guardianAuthService.getGuardian(guardianUsername);
            if (guardian == null) {
                return ResponseEntity.status(HttpStatus.NOT_FOUND).body(ApiResponse.error("Guardian not found"));
            }

            if (guardian.getPhone() == null || guardian.getPhone().isEmpty()) {
                return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(ApiResponse.error("Guardian phone number not found"));
            }

            boolean ok = twilioService.makeNoResponseAlertCall(guardian.getPhone(), elderlyName, location);
            if (ok) {
                ApiResponse<Object> response = ApiResponse.builder()
                        .status("success")
                        .message("URGENT call with siren initiated")
                        .guardian_phone(guardian.getPhone())
                        .alert_type("NO_RESPONSE")
                        .build();
                return ResponseEntity.ok(response);
            } else {
                return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error("Failed to initiate urgent call"));
            }
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(ApiResponse.error(e.getMessage()));
        }
    }
}
