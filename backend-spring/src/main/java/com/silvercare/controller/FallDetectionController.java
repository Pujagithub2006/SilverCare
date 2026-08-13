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

    @PostMapping("/api/alerts/prefall")
    public ResponseEntity<ApiResponse<Object>> triggerPrefall(@RequestBody Map<String, Object> body) {
        try {
            String deviceId = body.get("deviceId") != null ? body.get("deviceId").toString() : "vois_belt";
            String beltType = body.get("beltType") != null ? body.get("beltType").toString() : "Waist Belt";
            Double lat = body.get("latitude") != null ? Double.parseDouble(body.get("latitude").toString()) : null;
            Double lng = body.get("longitude") != null ? Double.parseDouble(body.get("longitude").toString()) : null;
            String audioMsg = body.get("audioMsg") != null ? body.get("audioMsg").toString() : null;

            var alert = fallDetectionService.handlePrefallAlert(deviceId, beltType, lat, lng, audioMsg);
            ApiResponse<Object> response = ApiResponse.builder()
                    .status("success")
                    .message("Prefall alert logged. SMS sent to Guardian and Neighbor.")
                    .data(alert)
                    .build();
            return ResponseEntity.ok(response);
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(ApiResponse.error(e.getMessage()));
        }
    }

    @PostMapping("/api/alerts/sos")
    public ResponseEntity<ApiResponse<Object>> triggerSos(@RequestBody Map<String, Object> body) {
        try {
            String deviceId = body.get("deviceId") != null ? body.get("deviceId").toString() : "vois_belt";
            String elderlyId = body.get("elderlyId") != null ? body.get("elderlyId").toString() : null;
            Double lat = body.get("latitude") != null ? Double.parseDouble(body.get("latitude").toString()) : null;
            Double lng = body.get("longitude") != null ? Double.parseDouble(body.get("longitude").toString()) : null;

            var alert = fallDetectionService.handleSosButton(deviceId, elderlyId, lat, lng);
            ApiResponse<Object> response = ApiResponse.builder()
                    .status("success")
                    .message("EMERGENCY SOS button triggered! Calls & SMS with live GPS dispatched.")
                    .data(alert)
                    .build();
            return ResponseEntity.ok(response);
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(ApiResponse.error(e.getMessage()));
        }
    }

    @PostMapping("/api/alerts/i-am-fine")
    public ResponseEntity<Map<String, Object>> pressIamFine(@RequestBody Map<String, Object> body) {
        String deviceId = body.get("deviceId") != null ? body.get("deviceId").toString() : "vois_belt";
        String elderlyId = body.get("elderlyId") != null ? body.get("elderlyId").toString() : null;

        Map<String, Object> res = fallDetectionService.handleIamFineButton(deviceId, elderlyId);
        return ResponseEntity.ok(res);
    }

    @PostMapping("/api/alerts/acknowledge")
    public ResponseEntity<ApiResponse<Object>> acknowledgeAlert(@RequestBody Map<String, Object> body) {
        try {
            String alertId = body.get("alertId") != null ? body.get("alertId").toString() : "";
            String username = body.get("guardianUsername") != null ? body.get("guardianUsername").toString() : "Guardian";
            String msg = body.get("message") != null ? body.get("message").toString() : "Acknowledged by guardian";

            var record = fallDetectionService.acknowledgeAlert(alertId, username, msg);
            ApiResponse<Object> response = ApiResponse.builder()
                    .status("success")
                    .message("Alert acknowledged successfully")
                    .data(record)
                    .build();
            return ResponseEntity.ok(response);
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(ApiResponse.error(e.getMessage()));
        }
    }
}
