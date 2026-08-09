package com.silvercare.controller;

import com.silvercare.dto.*;
import com.silvercare.entity.Device;
import com.silvercare.entity.Elderly;
import com.silvercare.entity.Guardian;
import com.silvercare.entity.GuardianElderlyLink;
import com.silvercare.entity.MitraChatHistory;
import com.silvercare.repository.DeviceRepository;
import com.silvercare.repository.GuardianElderlyLinkRepository;
import com.silvercare.repository.GuardianRepository;
import com.silvercare.repository.MitraChatHistoryRepository;
import com.silvercare.service.ElderlyManagementService;
import com.silvercare.service.ElderlyNotificationService;
import com.silvercare.service.MedicineService;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.time.LocalDateTime;
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

    @Autowired
    private DeviceRepository deviceRepository;

    @Autowired
    private MitraChatHistoryRepository mitraChatHistoryRepository;

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

    @GetMapping("/api/elderly/{elderlyId}/contacts")
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
            contacts.put("speakerMessage", "Fall fall! Emergency detected!");

            return ResponseEntity.ok(ApiResponse.builder()
                    .status("success")
                    .data(contacts)
                    .build());
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @PostMapping("/api/elderly/{elderlyId}/contacts")
    public ResponseEntity<ApiResponse<Object>> updateContacts(
            @PathVariable String elderlyId,
            @RequestBody Map<String, Object> contactData) {
        try {
            Elderly elderly = elderlyManagementService.getElderly(elderlyId);
            if (elderly == null) {
                return ResponseEntity.status(HttpStatus.NOT_FOUND).body(ApiResponse.error("Elderly not found"));
            }

            if (contactData.containsKey("neighborPhone")) {
                elderly.setNeighborPhone(contactData.get("neighborPhone").toString());
            }
            if (contactData.containsKey("neighborName")) {
                elderly.setNeighborName(contactData.get("neighborName").toString());
            }
            if (contactData.containsKey("secondaryGuardianPhone")) {
                elderly.setSecondaryGuardianPhone(contactData.get("secondaryGuardianPhone").toString());
            }
            if (contactData.containsKey("secondaryGuardianName")) {
                elderly.setSecondaryGuardianName(contactData.get("secondaryGuardianName").toString());
            }

            elderlyManagementService.updateElderlyEntity(elderly);

            return ResponseEntity.ok(ApiResponse.success("Contact information updated successfully"));
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

    // ===== DEVICE PAIRING ENDPOINTS =====

    @PostMapping("/api/devices/pair")
    public ResponseEntity<ApiResponse<Object>> pairDeviceToElderly(@RequestBody Map<String, String> request) {
        try {
            String macAddress = request.get("macAddress");
            String elderlyId = request.get("elderlyId");
            String deviceType = request.getOrDefault("deviceType", "Waist Belt");

            if (macAddress == null || elderlyId == null) {
                return ResponseEntity.badRequest().body(ApiResponse.error("macAddress and elderlyId are required"));
            }

            // Find device by MAC address
            Device device = deviceRepository.findByMacAddress(macAddress);
            
            if (device == null) {
                // Create new device
                String deviceId = "SC_" + deviceType.toUpperCase().replace(" ", "_") + "_" + macAddress.replace(":", "") + "_" + System.currentTimeMillis();
                device = Device.builder()
                    .deviceId(deviceId)
                    .deviceType(deviceType)
                    .macAddress(macAddress)
                    .status("ACTIVE")
                    .assignedElderlyId(elderlyId)
                    .pairingDate(LocalDateTime.now())
                    .build();
                deviceRepository.save(device);
            } else {
                // Update existing device
                if (device.getAssignedElderlyId() != null && !device.getAssignedElderlyId().equals(elderlyId)) {
                    // Add previous elderly to history
                    String prevHistory = device.getPreviouslyAssignedTo() != null ? device.getPreviouslyAssignedTo() : "[]";
                    device.setPreviouslyAssignedTo(prevHistory.replace("]", ",\"" + device.getAssignedElderlyId() + "\"]"));
                    device.setUnpairingDate(LocalDateTime.now());
                    device.setUnpairingReason("SWAPPED");
                }
                device.setAssignedElderlyId(elderlyId);
                device.setStatus("ACTIVE");
                device.setPairingDate(LocalDateTime.now());
                device.setUnpairingDate(null);
                device.setUnpairingReason(null);
                deviceRepository.save(device);
            }

            // Update elderly's primary device
            Optional<Elderly> elderlyOpt = elderlyManagementService.getElderlyById(elderlyId);
            if (elderlyOpt.isPresent()) {
                Elderly elderly = elderlyOpt.get();
                elderly.setPrimaryDeviceId(device.getDeviceId());
                elderly.setPrimaryBeltType(deviceType);
                elderlyManagementService.updateElderly(elderlyId, elderly);
            }

            Map<String, Object> data = new LinkedHashMap<>();
            data.put("deviceId", device.getDeviceId());
            data.put("macAddress", device.getMacAddress());
            data.put("deviceType", device.getDeviceType());
            data.put("elderlyId", elderlyId);
            data.put("pairingDate", device.getPairingDate());

            return ResponseEntity.ok(ApiResponse.builder()
                .status("success")
                .message("Device paired successfully")
                .data(data)
                .build());
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @PostMapping("/api/devices/unpair")
    public ResponseEntity<ApiResponse<Object>> unpairDevice(@RequestBody Map<String, String> request) {
        try {
            String deviceId = request.get("deviceId");
            String reason = request.getOrDefault("reason", "OTHER");

            if (deviceId == null) {
                return ResponseEntity.badRequest().body(ApiResponse.error("deviceId is required"));
            }

            Device device = deviceRepository.findById(deviceId).orElse(null);
            if (device == null) {
                return ResponseEntity.notFound().build();
            }

            // Add to history before unpairing
            if (device.getAssignedElderlyId() != null) {
                String prevHistory = device.getPreviouslyAssignedTo() != null ? device.getPreviouslyAssignedTo() : "[]";
                device.setPreviouslyAssignedTo(prevHistory.replace("]", ",\"" + device.getAssignedElderlyId() + "\"]"));
            }

            device.setAssignedElderlyId(null);
            device.setStatus("RETIRED");
            device.setUnpairingDate(LocalDateTime.now());
            device.setUnpairingReason(reason);
            deviceRepository.save(device);

            return ResponseEntity.ok(ApiResponse.success("Device unpaired successfully"));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping("/api/devices/elderly/{elderlyId}")
    public ResponseEntity<ApiResponse<Object>> getElderlyDevices(@PathVariable String elderlyId) {
        try {
            List<Device> currentDevices = deviceRepository.findByAssignedElderlyId(elderlyId);
            List<Device> allDevices = deviceRepository.findAll();
            
            // Find previously assigned devices
            List<Map<String, Object>> previousDevices = new java.util.ArrayList<>();
            for (Device device : allDevices) {
                if (device.getPreviouslyAssignedTo() != null && 
                    device.getPreviouslyAssignedTo().contains(elderlyId) &&
                    !elderlyId.equals(device.getAssignedElderlyId())) {
                    Map<String, Object> prevDevice = new LinkedHashMap<>();
                    prevDevice.put("deviceId", device.getDeviceId());
                    prevDevice.put("macAddress", device.getMacAddress());
                    prevDevice.put("deviceType", device.getDeviceType());
                    prevDevice.put("status", device.getStatus());
                    prevDevice.put("unpairingReason", device.getUnpairingReason());
                    prevDevice.put("unpairingDate", device.getUnpairingDate());
                    previousDevices.add(prevDevice);
                }
            }

            Map<String, Object> data = new LinkedHashMap<>();
            data.put("currentDevices", currentDevices);
            data.put("previousDevices", previousDevices);

            return ResponseEntity.ok(ApiResponse.builder()
                .status("success")
                .data(data)
                .build());
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping("/api/devices/mac/{macAddress}")
    public ResponseEntity<ApiResponse<Object>> getDeviceByMac(@PathVariable String macAddress) {
        try {
            Device device = deviceRepository.findByMacAddress(macAddress);
            if (device == null) {
                return ResponseEntity.notFound().build();
            }

            return ResponseEntity.ok(ApiResponse.builder()
                .status("success")
                .data(device)
                .build());
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
}
