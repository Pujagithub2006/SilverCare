package com.silvercare.controller;

import com.silvercare.dto.*;
import com.silvercare.entity.Elderly;
import com.silvercare.service.ElderlyManagementService;
import com.silvercare.service.ElderlyNotificationService;
import com.silvercare.service.MedicineService;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.List;
import java.util.Map;

@RestController
public class ElderlyManagementController {

    @Autowired
    private ElderlyManagementService elderlyManagementService;

    @Autowired
    private ElderlyNotificationService elderlyNotificationService;

    @Autowired
    private MedicineService medicineService;

    @PostMapping("/elderly-register")
    public ResponseEntity<ApiResponse<Object>> registerElderly(@RequestBody ElderlyRegisterRequest request) {
        try {
            Elderly elderly = elderlyManagementService.registerElderly(request);
            ApiResponse<Object> response = ApiResponse.builder()
                    .status("success")
                    .message("Elderly registered and linked to guardian successfully")
                    .elderly_id(elderly.getElderlyId())
                    .guardian_name(request.getGuardianUsername())
                    .build();
            return ResponseEntity.status(HttpStatus.CREATED).body(response);
        } catch (SecurityException e) {
            return ResponseEntity.status(HttpStatus.UNAUTHORIZED).body(ApiResponse.error(e.getMessage()));
        } catch (IllegalArgumentException e) {
            int code = e.getMessage().contains("not found") ? 404 : 400;
            return ResponseEntity.status(code).body(ApiResponse.error(e.getMessage()));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping("/elderly-info/{elderlyId}")
    public ResponseEntity<ApiResponse<Object>> getElderlyInfo(@PathVariable String elderlyId) {
        Elderly elderly = elderlyManagementService.getElderly(elderlyId);
        if (elderly == null) {
            return ResponseEntity.status(HttpStatus.NOT_FOUND).body(ApiResponse.error("Elderly profile not found"));
        }
        return ResponseEntity.ok(ApiResponse.builder().status("success").data(elderly).build());
    }

    @PostMapping("/elderly-update")
    public ResponseEntity<ApiResponse<Object>> updateElderly(@RequestBody ElderlyUpdateRequest request) {
        try {
            elderlyManagementService.updateElderly(request);
            return ResponseEntity.ok(ApiResponse.success("Elderly information updated"));
        } catch (IllegalArgumentException e) {
            return ResponseEntity.status(HttpStatus.NOT_FOUND).body(ApiResponse.error(e.getMessage()));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping("/guardian-elderly/{guardianUsername}")
    public ResponseEntity<ApiResponse<Object>> getGuardianElderly(@PathVariable String guardianUsername) {
        List<Elderly> list = elderlyManagementService.getElderlyByGuardian(guardianUsername);
        ApiResponse<Object> response = ApiResponse.builder()
                .status("success")
                .data(list)
                .count(list.size())
                .build();
        return ResponseEntity.ok(response);
    }

    @PostMapping({"/elderly/login", "/elderly-login"})
    public ResponseEntity<ApiResponse<Object>> elderlyLogin(@RequestBody ElderlyLoginRequest request) {
        try {
            Elderly elderly = elderlyManagementService.loginElderly(request);
            ApiResponse<Object> response = ApiResponse.builder()
                    .status("success")
                    .message("Login successful")
                    .elderly_id(elderly.getElderlyId())
                    .name(elderly.getName())
                    .guardian_username(elderly.getGuardianUsername())
                    .build();
            return ResponseEntity.ok(response);
        } catch (Exception e) {
            return ResponseEntity.ok(ApiResponse.builder()
                    .status("error")
                    .message(e.getMessage() != null ? e.getMessage() : "Login failed")
                    .build());
        }
    }

    @PostMapping("/elderly/register-session")
    public ResponseEntity<ApiResponse<Object>> registerSession(@RequestBody ElderlySessionRequest request) {
        try {
            elderlyNotificationService.registerElderlySession(request.getElderlyId(),
                    request.getDeviceInfo() != null ? request.getDeviceInfo() : "unknown_device");
            return ResponseEntity.ok(ApiResponse.success("Session registered successfully"));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @PostMapping("/elderly/unregister-session")
    public ResponseEntity<ApiResponse<Object>> unregisterSession(@RequestBody ElderlySessionRequest request) {
        try {
            elderlyNotificationService.unregisterElderlySession(request.getElderlyId());
            return ResponseEntity.ok(ApiResponse.success("Session unregistered successfully"));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping("/elderly/notifications/{elderlyId}")
    public ResponseEntity<ApiResponse<Object>> getElderlyNotifications(@PathVariable String elderlyId) {
        Map<String, Map<String, Object>> userNotifications = elderlyNotificationService.getElderlyNotifications(elderlyId);
        ApiResponse<Object> response = ApiResponse.builder()
                .status("success")
                .notifications(userNotifications)
                .build();
        return ResponseEntity.ok(response);
    }

    @PostMapping("/elderly/clear-notification")
    public ResponseEntity<ApiResponse<Object>> clearNotification(@RequestBody MedicineResponseRequest request) {
        try {
            medicineService.handleMedicineResponse(request.getElderlyId(), request.getMedicineId(), request.getResponse());
            if (!"snooze".equalsIgnoreCase(request.getResponse())) {
                elderlyNotificationService.clearElderlyNotification(request.getElderlyId(), request.getMedicineId());
            }
            return ResponseEntity.ok(ApiResponse.success("Response '" + request.getResponse() + "' recorded successfully"));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @RequestMapping(value = {"/api/admin/clear-database", "/admin/clear-database"}, method = {RequestMethod.GET, RequestMethod.POST})
    public ResponseEntity<ApiResponse<Object>> clearDatabase() {
        try {
            elderlyManagementService.clearAllData();
            return ResponseEntity.ok(ApiResponse.success("All database entries deleted successfully!"));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }
}
