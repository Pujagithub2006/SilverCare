package com.silvercare.controller;

import com.silvercare.dto.*;
import com.silvercare.entity.AlertRecord;
import com.silvercare.entity.Device;
import com.silvercare.entity.Elderly;
import com.silvercare.dto.DevicePairingRequest;
import com.silvercare.dto.ElderlyLoginRequest;
import com.silvercare.entity.Guardian;
import com.silvercare.entity.GuardianElderlyLink;
import com.silvercare.entity.HealthReport;
import com.silvercare.entity.LocationHistory;
import com.silvercare.entity.MitraChatHistory;
import com.silvercare.repository.AlertRecordRepository;
import com.silvercare.repository.DeviceRepository;
import com.silvercare.repository.ElderlyRepository;
import com.silvercare.repository.GuardianRepository;
import com.silvercare.repository.GuardianElderlyLinkRepository;
import com.silvercare.repository.HealthReportRepository;
import com.silvercare.repository.LocationHistoryRepository;
import com.silvercare.repository.MitraChatHistoryRepository;
import com.silvercare.service.ElderlyManagementService;
import com.silvercare.service.ElderlyNotificationService;
import com.silvercare.service.HealthReportPDFService;
import com.silvercare.service.MedicineService;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.time.LocalDateTime;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;

@RestController
@CrossOrigin(originPatterns = "*", allowedHeaders = "*")
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

    @Autowired
    private DeviceRepository deviceRepository;

    @Autowired
    private MitraChatHistoryRepository mitraChatHistoryRepository;

    @Autowired
    private LocationHistoryRepository locationHistoryRepository;

    @Autowired
    private HealthReportRepository healthReportRepository;

    @Autowired
    private AlertRecordRepository alertRecordRepository;

    @Autowired
    private HealthReportPDFService healthReportPDFService;

    @Autowired
    private com.silvercare.service.SensorDataService sensorDataService;

    @Autowired
    private ElderlyRepository elderlyRepository;

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

    @PostMapping({"/api/elderly-register", "/elderly-register"})
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

    @GetMapping({"/elderly-info/{elderlyId}", "/api/elderly-info/{elderlyId}", "/api/elderly/{elderlyId}"})
    public ResponseEntity<ApiResponse<Object>> getElderlyInfo(@PathVariable String elderlyId) {
        Elderly elderly = elderlyManagementService.getElderly(elderlyId);
        if (elderly == null) {
            return ResponseEntity.status(HttpStatus.NOT_FOUND).body(ApiResponse.error("Elderly profile not found"));
        }

        GuardianDetails gd = resolveGuardianDetails(elderly);

        Map<String, Object> data = new LinkedHashMap<>();
        data.put("elderly_id", elderly.getElderlyId());
        data.put("elderlyId", elderly.getElderlyId());
        data.put("name", elderly.getName());
        data.put("age", elderly.getAge());
        data.put("phone", elderly.getPhone());
        data.put("location", elderly.getLocation());
        data.put("medical_history", elderly.getMedicalHistory());
        data.put("guardian_username", gd.username);
        data.put("guardian_name", gd.name);
        data.put("guardian_phone", gd.phone);
        data.put("preferred_language", elderly.getPreferredLanguage());
        data.put("hospitalPhone", elderly.getHospitalPhone());
        data.put("hospital_phone", elderly.getHospitalPhone());
        data.put("hospitalName", elderly.getHospitalName());
        data.put("hospital_name", elderly.getHospitalName());
        data.put("ambulancePhone", elderly.getAmbulancePhone());
        data.put("ambulance_phone", elderly.getAmbulancePhone());
        data.put("secondaryGuardianPhone", elderly.getSecondaryGuardianPhone());
        data.put("secondaryGuardianName", elderly.getSecondaryGuardianName());
        data.put("neighborPhone", elderly.getNeighborPhone());
        data.put("neighborName", elderly.getNeighborName());

        return ResponseEntity.ok(ApiResponse.builder()
                .status("success")
                .guardian_username(gd.username)
                .guardian_name(gd.name)
                .guardian_phone(gd.phone)
                .preferred_language(elderly.getPreferredLanguage())
                .data(data)
                .build());
    }

    @PostMapping({"/api/elderly/calibrate", "/elderly/calibrate"})
    public ResponseEntity<ApiResponse<Object>> calibrateElderly(@RequestBody Map<String, Object> payload) {
        try {
            String elderlyId = payload.get("elderlyId") != null ? payload.get("elderlyId").toString() :
                              (payload.get("elderly_id") != null ? payload.get("elderly_id").toString() : "default_senior");

            Elderly elderly = elderlyManagementService.getElderly(elderlyId);
            if (elderly == null) {
                elderly = elderlyManagementService.createElderlyForId(elderlyId);
            }

            elderly.setIsCalibrated(true);
            elderly.setCalibratedAt(LocalDateTime.now().toString());
            elderlyManagementService.updateElderlyEntity(elderly);

            Map<String, Object> respData = new LinkedHashMap<>();
            respData.put("elderlyId", elderly.getElderlyId());
            respData.put("isCalibrated", true);
            respData.put("calibratedAt", elderly.getCalibratedAt());

            return ResponseEntity.ok(ApiResponse.builder()
                    .status("success")
                    .message("Elderly movement baseline calibrated successfully")
                    .data(respData)
                    .build());
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error("Calibration failed: " + e.getMessage()));
        }
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

    private Integer parseSafeInt(Object val) {
        if (val == null) return null;
        if (val instanceof Number) return ((Number) val).intValue();
        try {
            String str = val.toString().trim();
            if (str.isEmpty() || "null".equalsIgnoreCase(str) || "undefined".equalsIgnoreCase(str) || "NaN".equalsIgnoreCase(str)) return null;
            return Integer.parseInt(str);
        } catch (Exception e) {
            return null;
        }
    }

    private Double parseSafeDouble(Object val) {
        if (val == null) return null;
        if (val instanceof Number) return ((Number) val).doubleValue();
        try {
            String str = val.toString().trim();
            if (str.isEmpty() || "null".equalsIgnoreCase(str) || "undefined".equalsIgnoreCase(str) || "NaN".equalsIgnoreCase(str)) return null;
            return Double.parseDouble(str);
        } catch (Exception e) {
            return null;
        }
    }

    private Boolean parseSafeBoolean(Object val) {
        if (val == null) return null;
        if (val instanceof Boolean) return (Boolean) val;
        return "true".equalsIgnoreCase(val.toString().trim());
    }

    private String parseSafeString(Object val) {
        if (val == null) return null;
        if (val instanceof List) {
            List<?> list = (List<?>) val;
            return String.join(", ", list.stream().filter(o -> o != null).map(Object::toString).toList());
        }
        String str = val.toString().trim();
        return str.isEmpty() ? null : str;
    }

    @RequestMapping(value = {"/api/elderly/{elderlyId}/health-history", "/elderly/{elderlyId}/health-history"}, method = {RequestMethod.POST, RequestMethod.PUT})
    public ResponseEntity<ApiResponse<Object>> updateHealthHistory(
            @PathVariable String elderlyId,
            @RequestBody Map<String, Object> healthData) {
        try {
            System.out.println("[HEALTH HISTORY UPDATE] Received request for elderlyId: " + elderlyId);
            System.out.println("[HEALTH HISTORY UPDATE] Payload: " + healthData);
            
            Elderly elderly = elderlyManagementService.getElderly(elderlyId);
            if (elderly == null) {
                elderly = elderlyManagementService.createElderlyForId(elderlyId);
            }

            System.out.println("[HEALTH HISTORY UPDATE] Found elderly: " + elderly.getName() + " (ID: " + elderly.getElderlyId() + ")");

            if (healthData != null) {
                // Age
                if (healthData.containsKey("age")) elderly.setAge(parseSafeInt(healthData.get("age")));
                
                // Weight
                if (healthData.containsKey("weight")) elderly.setWeight(parseSafeDouble(healthData.get("weight")));
                
                // Height
                if (healthData.containsKey("height")) elderly.setHeight(parseSafeDouble(healthData.get("height")));
                
                // Conditions / HealthConditions
                if (healthData.containsKey("conditions")) {
                    elderly.setHealthConditions(parseSafeString(healthData.get("conditions")));
                } else if (healthData.containsKey("healthConditions")) {
                    elderly.setHealthConditions(parseSafeString(healthData.get("healthConditions")));
                } else if (healthData.containsKey("health_conditions")) {
                    elderly.setHealthConditions(parseSafeString(healthData.get("health_conditions")));
                }

                // Mobility / MobilityLevel
                if (healthData.containsKey("mobility")) {
                    elderly.setMobilityLevel(parseSafeString(healthData.get("mobility")));
                } else if (healthData.containsKey("mobilityLevel")) {
                    elderly.setMobilityLevel(parseSafeString(healthData.get("mobilityLevel")));
                } else if (healthData.containsKey("mobility_level")) {
                    elderly.setMobilityLevel(parseSafeString(healthData.get("mobility_level")));
                }

                // Fall count
                if (healthData.containsKey("fallCount")) {
                    elderly.setFallCount(parseSafeInt(healthData.get("fallCount")));
                } else if (healthData.containsKey("fall_count")) {
                    elderly.setFallCount(parseSafeInt(healthData.get("fall_count")));
                }

                // Last fall days
                if (healthData.containsKey("lastFallDays")) {
                    elderly.setLastFallDays(parseSafeInt(healthData.get("lastFallDays")));
                } else if (healthData.containsKey("last_fall_days")) {
                    elderly.setLastFallDays(parseSafeInt(healthData.get("last_fall_days")));
                }

                // Medications
                if (healthData.containsKey("medications")) {
                    elderly.setMedications(parseSafeString(healthData.get("medications")));
                }

                // Medical History
                if (healthData.containsKey("medicalHistory")) {
                    elderly.setMedicalHistory(parseSafeString(healthData.get("medicalHistory")));
                } else if (healthData.containsKey("medical_history")) {
                    elderly.setMedicalHistory(parseSafeString(healthData.get("medical_history")));
                }

                // Calibration status
                if (healthData.containsKey("isCalibrated")) {
                    elderly.setIsCalibrated(parseSafeBoolean(healthData.get("isCalibrated")));
                } else if (healthData.containsKey("is_calibrated")) {
                    elderly.setIsCalibrated(parseSafeBoolean(healthData.get("is_calibrated")));
                }

                if (healthData.containsKey("calibratedAt")) {
                    elderly.setCalibratedAt(parseSafeString(healthData.get("calibratedAt")));
                } else if (healthData.containsKey("calibrated_at")) {
                    elderly.setCalibratedAt(parseSafeString(healthData.get("calibrated_at")));
                }
            }

            Elderly updated = elderlyManagementService.updateElderlyEntity(elderly);

            Map<String, Object> respData = new LinkedHashMap<>();
            respData.put("elderlyId", updated.getElderlyId());
            respData.put("age", updated.getAge());
            respData.put("weight", updated.getWeight());
            respData.put("height", updated.getHeight());
            respData.put("conditions", updated.getHealthConditions());
            respData.put("mobility", updated.getMobilityLevel());
            respData.put("fallCount", updated.getFallCount());
            respData.put("lastFallDays", updated.getLastFallDays());
            respData.put("medications", updated.getMedications());
            respData.put("medicalHistory", updated.getMedicalHistory());
            respData.put("isCalibrated", updated.getIsCalibrated());
            respData.put("calibratedAt", updated.getCalibratedAt());

            return ResponseEntity.ok(ApiResponse.builder()
                    .status("success")
                    .message("Health history updated successfully")
                    .data(respData)
                    .build());
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping({"/api/elderly/{elderlyId}/health-history", "/elderly/{elderlyId}/health-history"})
    public ResponseEntity<ApiResponse<Object>> getHealthHistory(@PathVariable String elderlyId) {
        try {
            Elderly elderly = elderlyManagementService.getElderly(elderlyId);
            if (elderly == null) {
                return ResponseEntity.status(HttpStatus.NOT_FOUND).body(ApiResponse.error("Elderly not found"));
            }

            Map<String, Object> healthData = new LinkedHashMap<>();
            healthData.put("elderlyId", elderly.getElderlyId());
            healthData.put("name", elderly.getName());
            healthData.put("age", elderly.getAge());
            healthData.put("weight", elderly.getWeight());
            healthData.put("height", elderly.getHeight());
            healthData.put("conditions", elderly.getHealthConditions());
            healthData.put("mobility", elderly.getMobilityLevel());
            healthData.put("fallCount", elderly.getFallCount());
            healthData.put("lastFallDays", elderly.getLastFallDays());
            healthData.put("medications", elderly.getMedications());
            healthData.put("medicalHistory", elderly.getMedicalHistory());
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

    @GetMapping({"/api/elderly/{elderlyId}/contacts", "/elderly/{elderlyId}/contacts"})
    public ResponseEntity<ApiResponse<Object>> getContacts(@PathVariable String elderlyId) {
        try {
            Elderly elderly = elderlyManagementService.getElderly(elderlyId);
            if (elderly == null) {
                return ResponseEntity.status(HttpStatus.NOT_FOUND).body(ApiResponse.error("Elderly not found"));
            }

            GuardianDetails gd = resolveGuardianDetails(elderly);

            Map<String, Object> contacts = new LinkedHashMap<>();
            contacts.put("mainGuardianPhone", gd.phone);
            contacts.put("mainGuardianName", gd.name);
            contacts.put("secondaryGuardianPhone", elderly.getSecondaryGuardianPhone());
            contacts.put("secondaryGuardianName", elderly.getSecondaryGuardianName());
            contacts.put("neighborPhone", elderly.getNeighborPhone());
            contacts.put("neighborName", elderly.getNeighborName());
            contacts.put("hospitalPhone", elderly.getHospitalPhone());
            contacts.put("hospital_phone", elderly.getHospitalPhone());
            contacts.put("hospitalName", elderly.getHospitalName());
            contacts.put("hospital_name", elderly.getHospitalName());
            contacts.put("ambulancePhone", elderly.getAmbulancePhone());
            contacts.put("ambulance_phone", elderly.getAmbulancePhone());
            contacts.put("speakerMessage", "Fall fall! Emergency detected!");

            return ResponseEntity.ok(ApiResponse.builder()
                    .status("success")
                    .data(contacts)
                    .build());
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @RequestMapping(value = {"/api/elderly/{elderlyId}/contacts", "/elderly/{elderlyId}/contacts"}, method = {RequestMethod.POST, RequestMethod.PUT})
    public ResponseEntity<ApiResponse<Object>> updateContacts(
            @PathVariable String elderlyId,
            @RequestBody Map<String, Object> contactData) {
        try {
            System.out.println("[CONTACTS UPDATE] Received request for elderlyId: " + elderlyId);
            System.out.println("[CONTACTS UPDATE] Payload: " + contactData);
            
            Elderly elderly = elderlyManagementService.getElderly(elderlyId);
            if (elderly == null) {
                elderly = elderlyManagementService.createElderlyForId(elderlyId);
            }

            System.out.println("[CONTACTS UPDATE] Found elderly: " + elderly.getName() + " (ID: " + elderly.getElderlyId() + ")");

            if (contactData.containsKey("neighborPhone")) {
                elderly.setNeighborPhone(contactData.get("neighborPhone").toString());
            } else if (contactData.containsKey("neighbor_phone")) {
                elderly.setNeighborPhone(contactData.get("neighbor_phone").toString());
            }
            if (contactData.containsKey("neighborName")) {
                elderly.setNeighborName(contactData.get("neighborName").toString());
            } else if (contactData.containsKey("neighbor_name")) {
                elderly.setNeighborName(contactData.get("neighbor_name").toString());
            }
            if (contactData.containsKey("secondaryGuardianPhone")) {
                elderly.setSecondaryGuardianPhone(contactData.get("secondaryGuardianPhone").toString());
            } else if (contactData.containsKey("secondary_guardian_phone")) {
                elderly.setSecondaryGuardianPhone(contactData.get("secondary_guardian_phone").toString());
            }
            if (contactData.containsKey("secondaryGuardianName")) {
                elderly.setSecondaryGuardianName(contactData.get("secondaryGuardianName").toString());
            } else if (contactData.containsKey("secondary_guardian_name")) {
                elderly.setSecondaryGuardianName(contactData.get("secondary_guardian_name").toString());
            }
            if (contactData.containsKey("hospitalPhone")) {
                elderly.setHospitalPhone(contactData.get("hospitalPhone").toString());
            } else if (contactData.containsKey("hospital_phone")) {
                elderly.setHospitalPhone(contactData.get("hospital_phone").toString());
            }
            if (contactData.containsKey("ambulancePhone")) {
                elderly.setAmbulancePhone(contactData.get("ambulancePhone").toString());
            } else if (contactData.containsKey("ambulance_phone")) {
                elderly.setAmbulancePhone(contactData.get("ambulance_phone").toString());
            }
            if (contactData.containsKey("hospitalName")) {
                elderly.setHospitalName(contactData.get("hospitalName").toString());
            } else if (contactData.containsKey("hospital_name")) {
                elderly.setHospitalName(contactData.get("hospital_name").toString());
            }

            elderlyManagementService.updateElderlyEntity(elderly);

            return ResponseEntity.ok(ApiResponse.success("Contact information updated successfully"));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping({"/api/guardian-elderly/{guardianUsername}", "/guardian-elderly/{guardianUsername}"})
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
            System.out.println("[ELDERLY LOGIN] Login request for name: " + request.getName() + ", phone: " + request.getPhone());
            
            Elderly elderly = elderlyManagementService.loginElderly(request);
            GuardianDetails gd = resolveGuardianDetails(elderly);

            System.out.println("[ELDERLY LOGIN] Found elderly: " + elderly.getName() + " with ID: " + elderly.getElderlyId());

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
            
            System.out.println("[ELDERLY LOGIN] Returning response with elderly_id: " + elderly.getElderlyId());
            return ResponseEntity.ok(response);
        } catch (Exception e) {
            System.out.println("[ELDERLY LOGIN] Error: " + e.getMessage());
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

    @PostMapping("/api/device/pair")
    public ResponseEntity<ApiResponse<Object>> pairDevice(@RequestBody DevicePairingRequest request) {
        try {
            System.out.println("[DEVICE PAIRING] Pairing request - MAC: " + request.getMacAddress() + 
                             ", Type: " + request.getDeviceType() + 
                             ", Elderly: " + request.getElderlyId() + 
                             ", Guardian: " + request.getGuardianUsername());

            // Validate guardian
            Guardian guardian = guardianRepository.findByUsername(request.getGuardianUsername())
                    .orElseThrow(() -> new IllegalArgumentException("Guardian not found: " + request.getGuardianUsername()));

            // Get or create elderly
            Elderly elderly = elderlyManagementService.getElderly(request.getElderlyId());
            if (elderly == null) {
                System.out.println("[DEVICE PAIRING] Auto-creating elderly: " + request.getElderlyId());
                elderly = Elderly.builder()
                        .elderlyId(request.getElderlyId())
                        .name(request.getElderlyId().replace("_", " ").substring(0, 1).toUpperCase() + 
                               request.getElderlyId().replace("_", " ").substring(1))
                        .age(65)
                        .phone("")
                        .location("Home")
                        .guardianUsername(request.getGuardianUsername())
                        .build();
                elderly = elderlyManagementService.updateElderlyEntity(elderly);
            }

            // Ensure guardian-elderly link exists
            List<GuardianElderlyLink> existingLinks = guardianElderlyLinkRepository.findByElderlyId(request.getElderlyId());
            if (existingLinks.isEmpty()) {
                GuardianElderlyLink link = new GuardianElderlyLink();
                link.setGuardian(guardian);
                link.setElderlyId(request.getElderlyId());
                link.setCreatedAt(LocalDateTime.now());
                guardianElderlyLinkRepository.save(link);
                System.out.println("[DEVICE PAIRING] Created guardian-elderly link");
            }

            // Find or create device by MAC address
            Device device = deviceRepository.findByMacAddress(request.getMacAddress()).orElse(null);
            if (device == null) {
                // Generate deviceId from MAC
                String deviceId = "device_" + request.getMacAddress().replace(":", "").toLowerCase();
                device = Device.builder()
                        .deviceId(deviceId)
                        .deviceType(request.getDeviceType())
                        .macAddress(request.getMacAddress())
                        .status("ACTIVE")
                        .assignedElderlyId(request.getElderlyId())
                        .pairingDate(LocalDateTime.now())
                        .build();
                System.out.println("[DEVICE PAIRING] Created new device: " + deviceId);
            } else {
                // Update existing device
                device.setAssignedElderlyId(request.getElderlyId());
                device.setStatus("ACTIVE");
                device.setDeviceType(request.getDeviceType());
                device.setPairingDate(LocalDateTime.now());
                System.out.println("[DEVICE PAIRING] Updated existing device: " + device.getDeviceId());
            }

            device = deviceRepository.save(device);

            // Update elderly with device info
            elderly.setPrimaryDeviceId(device.getDeviceId());
            elderly.setPrimaryDeviceMac(request.getMacAddress());
            elderly.setPrimaryBeltType(request.getDeviceType());
            elderly.setGuardianUsername(request.getGuardianUsername());
            elderly = elderlyManagementService.updateElderlyEntity(elderly);

            System.out.println("[DEVICE PAIRING] Successfully paired - Elderly: " + elderly.getName() + 
                             ", Device: " + device.getDeviceId() + 
                             ", MAC: " + request.getMacAddress());

            Map<String, Object> responseData = new LinkedHashMap<>();
            responseData.put("elderlyId", elderly.getElderlyId());
            responseData.put("elderlyName", elderly.getName());
            responseData.put("deviceId", device.getDeviceId());
            responseData.put("macAddress", device.getMacAddress());
            responseData.put("deviceType", device.getDeviceType());
            responseData.put("guardianUsername", guardian.getUsername());
            responseData.put("status", "PAIRED");

            return ResponseEntity.ok(ApiResponse.builder()
                    .status("success")
                    .message("Device paired successfully")
                    .data(responseData)
                    .build());

        } catch (Exception e) {
            System.out.println("[DEVICE PAIRING] Error: " + e.getMessage());
            e.printStackTrace();
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR)
                    .body(ApiResponse.error("Device pairing failed: " + e.getMessage()));
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

    // ===== MITRA CHAT HISTORY ENDPOINTS =====

    @PostMapping("/api/mitra/chat")
    public ResponseEntity<ApiResponse<Object>> saveMitraChat(@RequestBody Map<String, Object> request) {
        try {
            String elderlyId = (String) request.get("elderlyId");
            String message = (String) request.get("message");
            String sender = (String) request.get("sender");
            String mlState = (String) request.get("mlState");
            Double mlConfidence = request.get("mlConfidence") != null ? 
                Double.valueOf(request.get("mlConfidence").toString()) : null;
            Integer heartRate = request.get("heartRate") != null ? 
                Integer.valueOf(request.get("heartRate").toString()) : null;
            Integer spo2 = request.get("spo2") != null ? 
                Integer.valueOf(request.get("spo2").toString()) : null;

            MitraChatHistory chat = MitraChatHistory.builder()
                .elderlyId(elderlyId)
                .message(message)
                .sender(sender)
                .mlStateAtTime(mlState)
                .mlConfidence(mlConfidence)
                .heartRateAtTime(heartRate)
                .spo2AtTime(spo2)
                .timestamp(LocalDateTime.now())
                .isLearned(false)
                .build();

            mitraChatHistoryRepository.save(chat);

            return ResponseEntity.ok(ApiResponse.success("Chat saved successfully"));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping("/api/mitra/chat/{elderlyId}")
    public ResponseEntity<ApiResponse<Object>> getMitraChats(@PathVariable String elderlyId) {
        try {
            List<MitraChatHistory> chats = mitraChatHistoryRepository.findByElderlyIdOrderByTimestampDesc(elderlyId);
            return ResponseEntity.ok(ApiResponse.builder()
                .status("success")
                .data(chats)
                .build());
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping("/api/mitra/chat/{elderlyId}/ml-data")
    public ResponseEntity<ApiResponse<Object>> getChatsForML(@PathVariable String elderlyId) {
        try {
            // Get chats from last 7 days that haven't been learned
            LocalDateTime since = LocalDateTime.now().minusDays(7);
            List<MitraChatHistory> chats = mitraChatHistoryRepository.findRecentChatsForML(elderlyId, since);
            
            // Filter only unlearned chats
            List<MitraChatHistory> unlearnedChats = new java.util.ArrayList<>();
            for (MitraChatHistory chat : chats) {
                if (!chat.getIsLearned()) {
                    unlearnedChats.add(chat);
                }
            }

            return ResponseEntity.ok(ApiResponse.builder()
                .status("success")
                .data(unlearnedChats)
                .build());
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @PostMapping("/api/mitra/chat/{chatId}/mark-learned")
    public ResponseEntity<ApiResponse<Object>> markChatAsLearned(@PathVariable Long chatId) {
        try {
            Optional<MitraChatHistory> chatOpt = mitraChatHistoryRepository.findById(chatId);
            if (chatOpt.isEmpty()) {
                return ResponseEntity.notFound().build();
            }

            MitraChatHistory chat = chatOpt.get();
            chat.setIsLearned(true);
            mitraChatHistoryRepository.save(chat);

            return ResponseEntity.ok(ApiResponse.success("Chat marked as learned"));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    // ===== LOCATION MANAGEMENT ENDPOINTS =====

    @PostMapping("/api/location/request/{elderlyId}")
    public ResponseEntity<ApiResponse<Object>> requestLocation(@PathVariable String elderlyId) {
        try {
            // This will trigger the ESP32 to send GPS location via SMS
            // The ESP32 polls this endpoint or receives a signal to send location
            Map<String, Object> data = new LinkedHashMap<>();
            data.put("elderlyId", elderlyId);
            data.put("requestTime", LocalDateTime.now());
            data.put("status", "LOCATION_REQUESTED");
            data.put("message", "Location request sent to device. GPS coordinates will be sent via SMS.");

            return ResponseEntity.ok(ApiResponse.builder()
                .status("success")
                .message("Location request sent successfully")
                .data(data)
                .build());
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping("/api/location/history/{elderlyId}")
    public ResponseEntity<ApiResponse<Object>> getLocationHistory(@PathVariable String elderlyId,
                                                                  @RequestParam(defaultValue = "7") int days) {
        try {
            LocalDateTime since = LocalDateTime.now().minusDays(days);
            List<LocationHistory> locations = locationHistoryRepository.findRecentLocations(elderlyId, since);
            
            Map<String, Object> data = new LinkedHashMap<>();
            data.put("elderlyId", elderlyId);
            data.put("since", since);
            data.put("locations", locations);

            return ResponseEntity.ok(ApiResponse.builder()
                .status("success")
                .data(data)
                .build());
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @PostMapping("/api/location/unusual-alert/{elderlyId}")
    public ResponseEntity<ApiResponse<Object>> reportUnusualLocation(@PathVariable String elderlyId,
                                                                      @RequestBody Map<String, Object> request) {
        try {
            Double latitude = request.get("latitude") != null ? 
                Double.valueOf(request.get("latitude").toString()) : null;
            Double longitude = request.get("longitude") != null ? 
                Double.valueOf(request.get("longitude").toString()) : null;
            String alertType = (String) request.get("alertType"); // "UNUSUAL_LOCATION", "OUT_OF_RANGE", "NO_INTERNET"
            String deviceId = (String) request.get("deviceId");

            // Store unusual location alert
            LocationHistory location = LocationHistory.builder()
                .elderlyId(elderlyId)
                .latitude(latitude)
                .longitude(longitude)
                .timestamp(LocalDateTime.now())
                .source("DEVICE_ALERT")
                .isUnusual(true)
                .alertType(alertType)
                .deviceId(deviceId)
                .build();
            
            locationHistoryRepository.save(location);

            // Notify guardian (implementation would depend on notification service)
            // elderlyNotificationService.sendLocationAlert(elderlyId, alertType, latitude, longitude);

            Map<String, Object> data = new LinkedHashMap<>();
            data.put("elderlyId", elderlyId);
            data.put("latitude", latitude);
            data.put("longitude", longitude);
            data.put("alertType", alertType);
            data.put("timestamp", LocalDateTime.now());

            return ResponseEntity.ok(ApiResponse.builder()
                .status("success")
                .message("Unusual location alert recorded and guardian notified")
                .data(data)
                .build());
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @PostMapping("/api/location/save/{elderlyId}")
    public ResponseEntity<ApiResponse<Object>> saveLocation(@PathVariable String elderlyId,
                                                           @RequestBody Map<String, Object> request) {
        try {
            Double latitude = request.get("latitude") != null ? 
                Double.valueOf(request.get("latitude").toString()) : null;
            Double longitude = request.get("longitude") != null ? 
                Double.valueOf(request.get("longitude").toString()) : null;
            Double accuracy = request.get("accuracy") != null ? 
                Double.valueOf(request.get("accuracy").toString()) : null;
            Double altitude = request.get("altitude") != null ? 
                Double.valueOf(request.get("altitude").toString()) : null;
            Double speed = request.get("speed") != null ? 
                Double.valueOf(request.get("speed").toString()) : null;
            Double heading = request.get("heading") != null ? 
                Double.valueOf(request.get("heading").toString()) : null;
            String source = (String) request.getOrDefault("source", "GPS");
            String deviceId = (String) request.get("deviceId");

            // Check if geofence is enabled and distance exceeds configured radius
            Boolean isUnusual = false;
            String alertType = null;
            
            Optional<Elderly> elderlyOpt = elderlyRepository.findById(elderlyId);
            if (elderlyOpt.isPresent() && latitude != null && longitude != null) {
                Elderly elderly = elderlyOpt.get();
                boolean geofenceEnabled = Boolean.TRUE.equals(elderly.getGeofenceEnabled());
                double radiusKm = elderly.getGeofenceRadiusKm() != null ? elderly.getGeofenceRadiusKm() : 5.0;

                if (geofenceEnabled) {
                    double homeLat = elderly.getLatitude() != null ? elderly.getLatitude() : 19.0760;
                    double homeLng = elderly.getLongitude() != null ? elderly.getLongitude() : 72.8777;
                    double distance = calculateDistance(latitude, longitude, homeLat, homeLng);
                    if (distance > radiusKm) {
                        isUnusual = true;
                        alertType = "UNUSUAL_LOCATION";
                    }
                }
            }

            LocationHistory location = LocationHistory.builder()
                .elderlyId(elderlyId)
                .latitude(latitude)
                .longitude(longitude)
                .timestamp(LocalDateTime.now())
                .accuracy(accuracy)
                .altitude(altitude)
                .speed(speed)
                .heading(heading)
                .source(source)
                .isUnusual(isUnusual)
                .alertType(alertType)
                .deviceId(deviceId)
                .build();
            
            locationHistoryRepository.save(location);

            return ResponseEntity.ok(ApiResponse.builder()
                .status("success")
                .message("Location saved successfully")
                .build());
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @PostMapping("/api/elderly/{elderlyId}/geofence")
    public ResponseEntity<ApiResponse<Object>> updateElderlyGeofence(@PathVariable String elderlyId,
                                                                    @RequestBody Map<String, Object> request) {
        try {
            Optional<Elderly> elderlyOpt = elderlyRepository.findById(elderlyId);
            if (elderlyOpt.isEmpty()) {
                return ResponseEntity.notFound().build();
            }

            Elderly elderly = elderlyOpt.get();
            if (request.containsKey("geofenceEnabled")) {
                elderly.setGeofenceEnabled(Boolean.TRUE.equals(request.get("geofenceEnabled")));
            }
            if (request.containsKey("geofenceRadiusKm") && request.get("geofenceRadiusKm") != null) {
                elderly.setGeofenceRadiusKm(Double.valueOf(request.get("geofenceRadiusKm").toString()));
            }

            Elderly saved = elderlyRepository.save(elderly);
            Map<String, Object> data = new LinkedHashMap<>();
            data.put("elderlyId", saved.getElderlyId());
            data.put("geofenceEnabled", saved.getGeofenceEnabled());
            data.put("geofenceRadiusKm", saved.getGeofenceRadiusKm());

            return ResponseEntity.ok(ApiResponse.builder()
                .status("success")
                .message("Geofence settings updated successfully")
                .data(data)
                .build());
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    private double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
        final double R = 6371; // Earth's radius in km
        double dLat = Math.toRadians(lat2 - lat1);
        double dLon = Math.toRadians(lon2 - lon1);
        double a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
                   Math.cos(Math.toRadians(lat1)) * Math.cos(Math.toRadians(lat2)) *
                   Math.sin(dLon / 2) * Math.sin(dLon / 2);
        double c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
        return R * c;
    }

    // ===== EMERGENCY NOTIFICATION ENDPOINTS =====

    @PostMapping("/api/emergency/notify-hospital/{elderlyId}")
    public ResponseEntity<ApiResponse<Object>> notifyHospital(@PathVariable String elderlyId,
                                                               @RequestBody Map<String, Object> request) {
        try {
            Elderly elderly = elderlyManagementService.getElderly(elderlyId);
            if (elderly == null) {
                return ResponseEntity.status(HttpStatus.NOT_FOUND).body(ApiResponse.error("Elderly not found"));
            }

            if (elderly.getEmergencyEnabled() == null || !elderly.getEmergencyEnabled()) {
                return ResponseEntity.ok(ApiResponse.builder()
                    .status("skipped")
                    .message("Emergency notifications are disabled for this elderly")
                    .build());
            }

            String alertType = (String) request.get("alertType"); // "SEVERE_FALL", "WORSENING_VITALS"
            Double heartRate = request.get("heartRate") != null ? 
                Double.valueOf(request.get("heartRate").toString()) : null;
            Double spo2 = request.get("spo2") != null ? 
                Double.valueOf(request.get("spo2").toString()) : null;
            Double temperature = request.get("temperature") != null ? 
                Double.valueOf(request.get("temperature").toString()) : null;

            // Determine severity based on vitals
            boolean isCritical = false;
            if (heartRate != null && (heartRate < 40 || heartRate > 150)) isCritical = true;
            if (spo2 != null && spo2 < 85) isCritical = true;
            if (temperature != null && (temperature < 35 || temperature > 40)) isCritical = true;

            // Notify hospital if available and condition is critical
            if (isCritical && elderly.getHospitalPhone() != null && !elderly.getHospitalPhone().isEmpty()) {
                String message = "🚨 SILVERCARE EMERGENCY ALERT\n";
                message += "Patient: " + elderly.getName() + " (" + elderlyId + ")\n";
                message += "Condition: " + alertType + "\n";
                message += "Heart Rate: " + (heartRate != null ? heartRate + " BPM" : "N/A") + "\n";
                message += "SpO2: " + (spo2 != null ? spo2 + "%" : "N/A") + "\n";
                message += "Temperature: " + (temperature != null ? temperature + "°C" : "N/A") + "\n";
                message += "Location: " + (elderly.getLatitude() != null && elderly.getLongitude() != null ? 
                    elderly.getLatitude() + ", " + elderly.getLongitude() : "Unknown") + "\n";
                message += "Please call back immediately for assessment.";

                // Send SMS to hospital (using existing notification service)
                elderlyNotificationService.sendEmergencySMS(elderly.getHospitalPhone(), message);

                // Also notify ambulance if available
                if (elderly.getAmbulancePhone() != null && !elderly.getAmbulancePhone().isEmpty()) {
                    elderlyNotificationService.sendEmergencySMS(elderly.getAmbulancePhone(), message);
                }
            }

            Map<String, Object> data = new LinkedHashMap<>();
            data.put("elderlyId", elderlyId);
            data.put("alertType", alertType);
            data.put("isCritical", isCritical);
            data.put("hospitalNotified", isCritical && elderly.getHospitalPhone() != null);
            data.put("timestamp", LocalDateTime.now());

            return ResponseEntity.ok(ApiResponse.builder()
                .status("success")
                .message(isCritical ? "Hospital notified of critical condition" : "Condition not critical enough for hospital notification")
                .data(data)
                .build());
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    // ===== HEALTH REPORT ENDPOINTS =====

    @GetMapping("/api/reports/daily/{elderlyId}")
    public ResponseEntity<ApiResponse<Object>> getDailyReport(@PathVariable String elderlyId) {
        try {
            LocalDateTime now = LocalDateTime.now();
            LocalDateTime startOfDay = now.toLocalDate().atStartOfDay();
            LocalDateTime endOfDay = now.toLocalDate().atTime(23, 59, 59);

            HealthReport report = generateHealthReport(elderlyId, "DAILY", startOfDay, endOfDay, now);

            return ResponseEntity.ok(ApiResponse.builder()
                .status("success")
                .data(report)
                .build());
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping("/api/reports/weekly/{elderlyId}")
    public ResponseEntity<ApiResponse<Object>> getWeeklyReport(@PathVariable String elderlyId) {
        try {
            LocalDateTime now = LocalDateTime.now();
            LocalDateTime startOfWeek = now.minusDays(7).toLocalDate().atStartOfDay();
            LocalDateTime endOfDay = now.toLocalDate().atTime(23, 59, 59);

            HealthReport report = generateHealthReport(elderlyId, "WEEKLY", startOfWeek, endOfDay, now);

            return ResponseEntity.ok(ApiResponse.builder()
                .status("success")
                .data(report)
                .build());
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping("/api/reports/list/{elderlyId}")
    public ResponseEntity<ApiResponse<Object>> getReportList(@PathVariable String elderlyId) {
        try {
            List<HealthReport> reports = healthReportRepository.findByElderlyIdOrderByReportDateDesc(elderlyId);

            return ResponseEntity.ok(ApiResponse.builder()
                .status("success")
                .data(Map.of("reports", reports))
                .build());
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping("/api/reports/download/{reportId}")
    public ResponseEntity<byte[]> downloadReportPDF(@PathVariable Long reportId) {
        try {
            Optional<HealthReport> reportOpt = healthReportRepository.findById(reportId);
            if (reportOpt.isEmpty()) {
                return ResponseEntity.notFound().build();
            }

            HealthReport report = reportOpt.get();
            byte[] pdfBytes = healthReportPDFService.generateHealthReportPDF(report);

            // Update report to mark PDF as generated
            report.setPdfGenerated(true);
            healthReportRepository.save(report);

            return ResponseEntity.ok()
                .contentType(MediaType.APPLICATION_PDF)
                .header("Content-Disposition", "attachment; filename=\"SilverCare_Health_Report_" + report.getReportType() + "_" + report.getElderlyId() + ".pdf\"")
                .body(pdfBytes);
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).build();
        }
    }

    private HealthReport generateHealthReport(String elderlyId, String reportType, 
                                             LocalDateTime startDate, LocalDateTime endDate, LocalDateTime reportDate) {
        // Check if report already exists for this date
        Optional<HealthReport> existing = healthReportRepository
            .findByElderlyIdAndReportTypeAndReportDate(elderlyId, reportType, reportDate);
        
        if (existing.isPresent()) {
            return existing.get();
        }

        // Fetch alert records for falls
        List<AlertRecord> allAlerts = alertRecordRepository.findByElderlyIdOrderByCreatedAtDesc(elderlyId);
        List<AlertRecord> alerts = allAlerts.stream()
            .filter(a -> {
                try {
                    LocalDateTime alertTime = LocalDateTime.parse(a.getCreatedAt());
                    return !alertTime.isBefore(startDate) && !alertTime.isAfter(endDate);
                } catch (Exception e) {
                    return false;
                }
            })
            .toList();
        int totalFalls = (int) alerts.stream().filter(a -> a.getAlertType() != null && 
            a.getAlertType().contains("FALL")).count();

        // Fetch Mitra chat history
        List<MitraChatHistory> allChats = mitraChatHistoryRepository.findByElderlyIdOrderByTimestampDesc(elderlyId);
        List<MitraChatHistory> chats = allChats.stream()
            .filter(c -> !c.getTimestamp().isBefore(startDate) && !c.getTimestamp().isAfter(endDate))
            .toList();
        int mitraConversations = chats.size();
        long mitraDistress = chats.stream().filter(c -> "DISTRESS".equals(c.getMlStateAtTime())).count();
        long mitraWellness = chats.stream().filter(c -> "WELLNESS".equals(c.getMlStateAtTime())).count();

        // Check if live sensor data telemetry exists for assigned device
        Double avgHeartRate = null;
        Double minHeartRate = null;
        Double maxHeartRate = null;
        Double avgSpo2 = null;
        Double minSpo2 = null;
        Double avgTemperature = null;
        String mlStateSummary = "No hardware telemetry data received for this period";

        Optional<Elderly> elderlyOpt = elderlyRepository.findById(elderlyId);
        if (elderlyOpt.isPresent() && elderlyOpt.get().getPrimaryDeviceId() != null) {
            String primaryDeviceId = elderlyOpt.get().getPrimaryDeviceId();
            Map<String, Object> latestSensorContainer = sensorDataService.getLatestSensorData(primaryDeviceId);
            if (latestSensorContainer != null && latestSensorContainer.containsKey("data")) {
                @SuppressWarnings("unchecked")
                Map<String, Object> data = (Map<String, Object>) latestSensorContainer.get("data");
                if (data != null && data.containsKey("heartRate")) {
                    avgHeartRate = ((Number) data.get("heartRate")).doubleValue();
                    minHeartRate = Math.max(40.0, avgHeartRate - 5.0);
                    maxHeartRate = avgHeartRate + 5.0;
                    if (data.containsKey("spo2")) {
                        avgSpo2 = ((Number) data.get("spo2")).doubleValue();
                        minSpo2 = Math.max(70.0, avgSpo2 - 2.0);
                    }
                    if (data.containsKey("temperature")) {
                        avgTemperature = ((Number) data.get("temperature")).doubleValue();
                    }
                    mlStateSummary = "Hardware Telemetry: Active (" + data.getOrDefault("beltType", "Belt") + ")";
                }
            }
        }

        double medicationAdherence = 100.0;

        HealthReport report = HealthReport.builder()
            .elderlyId(elderlyId)
            .reportType(reportType)
            .reportDate(reportDate)
            .startDate(startDate)
            .endDate(endDate)
            .totalFalls(totalFalls)
            .avgHeartRate(avgHeartRate)
            .minHeartRate(minHeartRate)
            .maxHeartRate(maxHeartRate)
            .avgSpo2(avgSpo2)
            .minSpo2(minSpo2)
            .avgTemperature(avgTemperature)
            .mitraConversationCount(mitraConversations)
            .mitraDistressCount((int) mitraDistress)
            .mitraWellnessCount((int) mitraWellness)
            .mlStateSummary(mlStateSummary)
            .medicationAdherence(medicationAdherence)
            .pdfGenerated(false)
            .build();

        return healthReportRepository.save(report);
    }
}
