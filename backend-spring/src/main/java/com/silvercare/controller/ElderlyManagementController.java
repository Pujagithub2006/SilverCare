package com.silvercare.controller;

import com.silvercare.dto.*;
import com.silvercare.entity.Elderly;
import com.silvercare.entity.Guardian;
import com.silvercare.entity.GuardianElderlyLink;
import com.silvercare.repository.GuardianElderlyLinkRepository;
import com.silvercare.repository.GuardianRepository;
import com.silvercare.service.ElderlyManagementService;
import com.silvercare.service.ElderlyNotificationService;
import com.silvercare.service.MedicineService;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;

@RestController
public class ElderlyManagementController {

    @Autowired
    private ElderlyManagementService elderlyManagementService;

    @Autowired
    private GuardianRepository guardianRepository;

    @Autowired
    private GuardianElderlyLinkRepository guardianElderlyLinkRepository;

    @Autowired
    private ElderlyNotificationService elderlyNotificationService;

    @Autowired
    private MedicineService medicineService;

    private GuardianDetails resolveGuardianDetails(Elderly elderly) {
        if (elderly == null) return new GuardianDetails("", "", "");

        String elderlyId = elderly.getElderlyId();
        String gUsername = elderly.getGuardianUsername();
        Guardian guardian = null;

        // Step 1: Check guardian_elderly_links repository by elderlyId
        if (elderlyId != null && !elderlyId.trim().isEmpty()) {
            List<GuardianElderlyLink> links = guardianElderlyLinkRepository.findByElderlyId(elderlyId.trim());
            if (links != null && !links.isEmpty()) {
                for (GuardianElderlyLink link : links) {
                    if (link.getGuardian() != null) {
                        guardian = link.getGuardian();
                        break;
                    }
                }
            }
        }

        // Step 2: Check by guardianUsername on Elderly entity
        if (guardian == null && gUsername != null && !gUsername.trim().isEmpty()) {
            guardian = guardianRepository.findByUsername(gUsername.trim()).orElse(null);
        }

        // Step 3: Check all guardian_elderly_links for case-insensitive elderlyId match
        if (guardian == null && elderlyId != null && !elderlyId.trim().isEmpty()) {
            List<GuardianElderlyLink> allLinks = guardianElderlyLinkRepository.findAll();
            for (GuardianElderlyLink link : allLinks) {
                if (link.getElderlyId() != null && link.getElderlyId().equalsIgnoreCase(elderlyId.trim())) {
                    if (link.getGuardian() != null) {
                        guardian = link.getGuardian();
                        break;
                    }
                }
            }
        }

        // Step 4: Check all Guardians in DB to see if any has elderlyId in linked list
        List<Guardian> allGuardians = guardianRepository.findAll();
        if (guardian == null && elderlyId != null && !elderlyId.trim().isEmpty()) {
            for (Guardian g : allGuardians) {
                List<String> linked = g.getElderlyLinked();
                if (linked != null) {
                    for (String item : linked) {
                        if (item != null && item.equalsIgnoreCase(elderlyId.trim())) {
                            guardian = g;
                            break;
                        }
                    }
                }
                if (guardian != null) break;
            }
        }

        // Step 5: Fallback to first registered guardian in database if guardians exist
        if (guardian == null && !allGuardians.isEmpty()) {
            guardian = allGuardians.get(0);
        }

        if (guardian != null) {
            String name = (guardian.getName() != null && !guardian.getName().trim().isEmpty()) ? guardian.getName() : guardian.getUsername();
            String phone = guardian.getPhone() != null ? guardian.getPhone() : "";
            return new GuardianDetails(guardian.getUsername(), name, phone);
        } else if (gUsername != null && !gUsername.trim().isEmpty()) {
            return new GuardianDetails(gUsername, gUsername, "");
        }

        return new GuardianDetails("", "", "");
    }

    private static class GuardianDetails {
        String username;
        String name;
        String phone;
        GuardianDetails(String username, String name, String phone) {
            this.username = username;
            this.name = name;
            this.phone = phone;
        }
    }

    @PostMapping("/elderly-register")
    public ResponseEntity<ApiResponse<Object>> registerElderly(@RequestBody ElderlyRegisterRequest request) {
        try {
            Elderly elderly = elderlyManagementService.registerElderly(request);
            GuardianDetails gd = resolveGuardianDetails(elderly);

            ApiResponse<Object> response = ApiResponse.builder()
                    .status("success")
                    .message("Elderly registered and linked to guardian successfully")
                    .elderly_id(elderly.getElderlyId())
                    .name(elderly.getName())
                    .phone(elderly.getPhone())
                    .guardian_username(gd.username)
                    .guardian_name(gd.name)
                    .guardian_phone(gd.phone)
                    .preferred_language(elderly.getPreferredLanguage())
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

        GuardianDetails gd = resolveGuardianDetails(elderly);

        Map<String, Object> data = new LinkedHashMap<>();
        data.put("elderly_id", elderly.getElderlyId());
        data.put("name", elderly.getName());
        data.put("age", elderly.getAge());
        data.put("phone", elderly.getPhone());
        data.put("location", elderly.getLocation());
        data.put("medical_history", elderly.getMedicalHistory());
        data.put("guardian_username", gd.username);
        data.put("guardian_name", gd.name);
        data.put("guardian_phone", gd.phone);
        data.put("preferred_language", elderly.getPreferredLanguage());

        return ResponseEntity.ok(ApiResponse.builder()
                .status("success")
                .guardian_username(gd.username)
                .guardian_name(gd.name)
                .guardian_phone(gd.phone)
                .preferred_language(elderly.getPreferredLanguage())
                .data(data)
                .build());
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

    @PostMapping("/api/elderly/{elderlyId}/health-history")
    public ResponseEntity<ApiResponse<Object>> updateHealthHistory(
            @PathVariable String elderlyId,
            @RequestBody Map<String, Object> healthData) {
        try {
            Elderly elderly = elderlyManagementService.getElderly(elderlyId);
            if (elderly == null) {
                return ResponseEntity.status(HttpStatus.NOT_FOUND).body(ApiResponse.error("Elderly not found"));
            }

            // Update health history fields
            if (healthData.containsKey("age")) {
                elderly.setAge(((Number) healthData.get("age")).intValue());
            }
            if (healthData.containsKey("weight")) {
                elderly.setWeight(((Number) healthData.get("weight")).doubleValue());
            }
            if (healthData.containsKey("height")) {
                elderly.setHeight(((Number) healthData.get("height")).doubleValue());
            }
            if (healthData.containsKey("conditions")) {
                elderly.setHealthConditions(healthData.get("conditions").toString());
            }
            if (healthData.containsKey("mobility")) {
                elderly.setMobilityLevel(healthData.get("mobility").toString());
            }
            if (healthData.containsKey("fallCount")) {
                elderly.setFallCount(((Number) healthData.get("fallCount")).intValue());
            }
            if (healthData.containsKey("lastFallDays")) {
                elderly.setLastFallDays(((Number) healthData.get("lastFallDays")).intValue());
            }
            if (healthData.containsKey("medications")) {
                elderly.setMedications(healthData.get("medications").toString());
            }

            elderlyManagementService.updateElderlyEntity(elderly);

            return ResponseEntity.ok(ApiResponse.success("Health history updated successfully"));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping("/api/elderly/{elderlyId}/health-history")
    public ResponseEntity<ApiResponse<Object>> getHealthHistory(@PathVariable String elderlyId) {
        try {
            Elderly elderly = elderlyManagementService.getElderly(elderlyId);
            if (elderly == null) {
                return ResponseEntity.status(HttpStatus.NOT_FOUND).body(ApiResponse.error("Elderly not found"));
            }

            Map<String, Object> healthData = new LinkedHashMap<>();
            healthData.put("age", elderly.getAge());
            healthData.put("weight", elderly.getWeight());
            healthData.put("height", elderly.getHeight());
            healthData.put("conditions", elderly.getHealthConditions());
            healthData.put("mobility", elderly.getMobilityLevel());
            healthData.put("fallCount", elderly.getFallCount());
            healthData.put("lastFallDays", elderly.getLastFallDays());
            healthData.put("medications", elderly.getMedications());
            healthData.put("isCalibrated", elderly.getIsCalibrated());
            healthData.put("calibratedAt", elderly.getCalibratedAt());

            return ResponseEntity.ok(ApiResponse.builder()
                    .status("success")
                    .data(healthData)
                    .build());
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
            GuardianDetails gd = resolveGuardianDetails(elderly);

            ApiResponse<Object> response = ApiResponse.builder()
                    .status("success")
                    .message("Login successful")
                    .elderly_id(elderly.getElderlyId())
                    .name(elderly.getName())
                    .phone(elderly.getPhone())
                    .guardian_username(gd.username)
                    .guardian_name(gd.name)
                    .guardian_phone(gd.phone)
                    .preferred_language(elderly.getPreferredLanguage())
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
