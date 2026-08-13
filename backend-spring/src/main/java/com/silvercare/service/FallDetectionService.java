package com.silvercare.service;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.silvercare.entity.AlertRecord;
import com.silvercare.entity.Elderly;
import com.silvercare.entity.Guardian;
import com.silvercare.entity.GuardianElderlyLink;
import com.silvercare.repository.AlertRecordRepository;
import com.silvercare.repository.ElderlyRepository;
import com.silvercare.repository.GuardianElderlyLinkRepository;
import com.silvercare.repository.GuardianRepository;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.scheduling.annotation.EnableScheduling;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Service;

import java.time.LocalDateTime;
import java.time.Duration;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicBoolean;

@Service
@EnableScheduling
public class FallDetectionService {

    private final AtomicBoolean fallDetected = new AtomicBoolean(false);
    private final Map<String, String> deviceCurrentUserMap = new ConcurrentHashMap<>();
    private final ObjectMapper objectMapper = new ObjectMapper();

    @Autowired
    private ElderlyRepository elderlyRepository;

    @Autowired
    private GuardianRepository guardianRepository;

    @Autowired
    private GuardianElderlyLinkRepository guardianElderlyLinkRepository;

    @Autowired
    private AlertRecordRepository alertRecordRepository;

    @Autowired
    private TwilioService twilioService;

    @Autowired
    private FirebaseEncryptionService firebaseEncryptionService;

    public void triggerFall(String deviceId, Double confidence) {
        System.out.println("[FALL DETECTED] Device: " + deviceId + ", Confidence: " + confidence);
        fallDetected.set(true);
    }

    public boolean isFallDetected() {
        return fallDetected.get();
    }

    public void clearFall() {
        fallDetected.set(false);
        System.out.println("[FALL CLEARED] Fall status reset.");
    }

    /**
     * Resolves elderly ID for a device
     */
    public String resolveElderlyIdForDevice(String deviceId) {
        if (deviceId == null) return "senior_user";
        String mappedUser = deviceCurrentUserMap.get(deviceId);
        if (mappedUser != null) return mappedUser;

        Optional<Elderly> elderlyOpt = elderlyRepository.findByElderlyId(deviceId);
        if (elderlyOpt.isPresent()) return elderlyOpt.get().getElderlyId();

        List<Elderly> all = elderlyRepository.findAll();
        if (!all.isEmpty()) return all.get(0).getElderlyId();

        return deviceId;
    }

    /**
     * Finds ALL linked guardians for an elderly person (Many-to-Many support)
     */
    public List<Guardian> getAllGuardiansForElderly(String elderlyId) {
        List<Guardian> guardians = new ArrayList<>();
        Set<String> guardianUsernames = new HashSet<>();

        // 1. Check explicit links table
        List<GuardianElderlyLink> links = guardianElderlyLinkRepository.findByElderlyId(elderlyId);
        for (GuardianElderlyLink link : links) {
            guardianUsernames.add(link.getGuardianUsername());
        }

        // 2. Check legacy guardianUsername column on Elderly entity
        Optional<Elderly> elderlyOpt = elderlyRepository.findByElderlyId(elderlyId);
        if (elderlyOpt.isPresent() && elderlyOpt.get().getGuardianUsername() != null) {
            guardianUsernames.add(elderlyOpt.get().getGuardianUsername());
        }

        // 3. Retrieve Guardian entities
        for (String gUser : guardianUsernames) {
            guardianRepository.findByUsername(gUser).ifPresent(guardians::add);
        }

        // 4. Fallback if no specific guardian linked: return all registered guardians
        if (guardians.isEmpty()) {
            guardians = guardianRepository.findAll();
        }

        return guardians;
    }

    /**
     * Handle PREFALL Alert: Notify Guardian & Neighbor immediately via SMS with vital signs
     */
    public AlertRecord handlePrefallAlert(String deviceId, String beltType, Double lat, Double lng, String audioMsg) {
        String elderlyId = resolveElderlyIdForDevice(deviceId);
        Elderly elderly = elderlyRepository.findByElderlyId(elderlyId).orElse(null);
        String elderlyName = elderly != null ? elderly.getName() : "Senior Citizen";
        String location = elderly != null && elderly.getLocation() != null ? elderly.getLocation() : "Home";

        String mainPhone = getGuardianPhoneForElderly(deviceId);
        if ((mainPhone == null || mainPhone.isEmpty()) && elderly != null && elderly.getPhone() != null) {
            mainPhone = elderly.getPhone();
        }
        String neighborPhone = elderly != null ? (elderly.getNeighborPhone() != null ? elderly.getNeighborPhone() : elderly.getNeighbourPhone()) : null;

        String alertId = "PREFALL_" + UUID.randomUUID().toString().substring(0, 8);
        AlertRecord alert = new AlertRecord(alertId, elderlyId, deviceId, beltType != null ? beltType : "Waist Belt", "PREFALL", "PENDING_ACK", audioMsg, lat, lng);
        alertRecordRepository.save(alert);

        // Send Prefall SMS with vitals (HR ~118, SpO2 ~94, Temp ~37.1) to Main Guardian & Neighbor
        if (mainPhone != null && !mainPhone.isEmpty()) {
            twilioService.sendPrefallSms(mainPhone, elderlyName, location, 118, 94, 37.1);
        }
        if (neighborPhone != null && !neighborPhone.isEmpty()) {
            twilioService.sendPrefallSms(neighborPhone, elderlyName, location, 118, 94, 37.1);
        }

        // Encrypt & Save to Firebase
        try {
            firebaseEncryptionService.saveToFirebaseEncrypted("alerts", alertId, objectMapper.writeValueAsString(alert));
        } catch (Exception ignored) {}

        return alert;
    }

    /**
     * Handle FALL_DETECTED Alert: Call & SMS Main Guardian AND Neighbor immediately
     */
    public AlertRecord handleFallAlert(String deviceId, String beltType, Double lat, Double lng, String audioMsg) {
        String elderlyId = resolveElderlyIdForDevice(deviceId);
        Elderly elderly = elderlyRepository.findByElderlyId(elderlyId).orElse(null);
        String elderlyName = elderly != null ? elderly.getName() : "Senior Citizen";
        String location = elderly != null && elderly.getLocation() != null ? elderly.getLocation() : "Home";

        String mainPhone = getGuardianPhoneForElderly(deviceId);
        if ((mainPhone == null || mainPhone.isEmpty()) && elderly != null && elderly.getPhone() != null) {
            mainPhone = elderly.getPhone();
        }
        String neighborPhone = elderly != null ? (elderly.getNeighborPhone() != null ? elderly.getNeighborPhone() : elderly.getNeighbourPhone()) : null;

        String alertId = "FALL_" + UUID.randomUUID().toString().substring(0, 8);
        AlertRecord alert = new AlertRecord(alertId, elderlyId, deviceId, beltType != null ? beltType : "Waist Belt", "FALL_DETECTED", "PENDING_ACK", audioMsg, lat, lng);
        alertRecordRepository.save(alert);

        fallDetected.set(true);

        // 1. Send SMS & Call Main Guardian
        if (mainPhone != null && !mainPhone.isEmpty()) {
            twilioService.sendFallAlertSms(mainPhone, elderlyName, location, deviceId);
            twilioService.makeEmergencyCall(mainPhone, elderlyName, location);
        }

        // 2. Send SMS & Call Neighbor immediately
        if (neighborPhone != null && !neighborPhone.isEmpty()) {
            twilioService.sendFallAlertSms(neighborPhone, elderlyName, location + " (Neighbor Emergency Alert)", deviceId);
            twilioService.makeNeighborCall(neighborPhone, elderlyName, location);
        }

        // Encrypt & Save to Firebase
        try {
            firebaseEncryptionService.saveToFirebaseEncrypted("alerts", alertId, objectMapper.writeValueAsString(alert));
        } catch (Exception ignored) {}

        return alert;
    }

    /**
     * Handle Hardware SOS Push Button
     */
    public AlertRecord handleSosButton(String deviceId, String elderlyId, Double lat, Double lng) {
        String targetElderlyId = elderlyId != null ? elderlyId : resolveElderlyIdForDevice(deviceId);
        Elderly elderly = elderlyRepository.findByElderlyId(targetElderlyId).orElse(null);
        String elderlyName = elderly != null ? elderly.getName() : "Senior Citizen";
        String mainPhone = getGuardianPhoneForElderly(deviceId);
        if ((mainPhone == null || mainPhone.isEmpty()) && elderly != null && elderly.getPhone() != null) {
            mainPhone = elderly.getPhone();
        }

        String alertId = "SOS_" + UUID.randomUUID().toString().substring(0, 8);
        AlertRecord alert = new AlertRecord(alertId, targetElderlyId, deviceId, "SOS Button", "SOS", "PENDING_ACK", "SOS Button Pressed", lat, lng);
        alertRecordRepository.save(alert);

        if (mainPhone != null && !mainPhone.isEmpty()) {
            twilioService.sendSosSmsAndCall(mainPhone, elderlyName, lat, lng);
        }

        try {
            firebaseEncryptionService.saveToFirebaseEncrypted("alerts", alertId, objectMapper.writeValueAsString(alert));
        } catch (Exception ignored) {}

        return alert;
    }

    /**
     * Handle "I am Fine" Hardware Push Button: Shuts down alarm, clears fall alert, resets state to NORMAL (IMPACTED)
     */
    public Map<String, Object> handleIamFineButton(String deviceId, String elderlyId) {
        clearFall();
        String targetElderlyId = elderlyId != null ? elderlyId : resolveElderlyIdForDevice(deviceId);

        // Mark pending alerts as resolved
        List<AlertRecord> pending = alertRecordRepository.findByStatus("PENDING_ACK");
        for (AlertRecord alert : pending) {
            if (targetElderlyId.equals(alert.getElderlyId())) {
                alert.setStatus("RESOLVED_I_AM_FINE");
                alert.setAcknowledgedBy("ELDERLY_PUSH_BUTTON");
                alert.setAcknowledgedAt(LocalDateTime.now().toString());
                alertRecordRepository.save(alert);
            }
        }

        Map<String, Object> result = new LinkedHashMap<>();
        result.put("status", "success");
        result.put("message", "Speaker alarm shut down. Alert cleared and reset to NORMAL (IMPACTED).");
        result.put("stateName", "NORMAL (IMPACTED)");
        return result;
    }

    /**
     * Guardian "I am Fine" / Acknowledgment Logic
     */
    public AlertRecord acknowledgeAlert(String alertId, String guardianUsername, String responseMessage) {
        Optional<AlertRecord> optAlert = alertRecordRepository.findByAlertId(alertId);
        AlertRecord alert;
        if (optAlert.isPresent()) {
            alert = optAlert.get();
        } else {
            List<AlertRecord> pending = alertRecordRepository.findByStatus("PENDING_ACK");
            if (!pending.isEmpty()) {
                alert = pending.get(pending.size() - 1);
            } else {
                throw new IllegalArgumentException("No pending alert found to acknowledge");
            }
        }

        alert.setStatus("ACKNOWLEDGED");
        alert.setAcknowledgedBy(guardianUsername);
        alert.setAcknowledgedAt(LocalDateTime.now().toString());
        alertRecordRepository.save(alert);

        clearFall();

        System.out.printf("[ALERT ACKNOWLEDGED] Guardian '%s' acknowledged alert %s with message: '%s'%n",
                guardianUsername, alert.getAlertId(), responseMessage);

        try {
            firebaseEncryptionService.saveToFirebaseEncrypted("alerts", alert.getAlertId(), objectMapper.writeValueAsString(alert));
        } catch (Exception ignored) {}

        return alert;
    }

    /**
     * Multi-Tier Scheduled Escalation Engine:
     * 1. Prefall: If unacknowledged in 5 mins (300s) -> Call Neighbor
     * 2. Fall Tier 1: If unacknowledged in 2 mins (120s) -> Ensure Neighbor receives phone call
     * 3. Fall Tier 2: If unacknowledged in 4 mins (240s) -> Call Secondary Guardian
     */
    @Scheduled(fixedDelay = 10000) // Check every 10 seconds
    public void runMultiTierEscalationEngine() {
        List<AlertRecord> pendingAlerts = alertRecordRepository.findByStatus("PENDING_ACK");
        LocalDateTime now = LocalDateTime.now();

        for (AlertRecord alert : pendingAlerts) {
            String createdAtStr = alert.getCreatedAt();
            if (createdAtStr == null || createdAtStr.isEmpty()) continue;

            LocalDateTime createdTime;
            try {
                createdTime = LocalDateTime.parse(createdAtStr);
            } catch (Exception e) {
                continue;
            }

            long elapsedSeconds = Duration.between(createdTime, now).getSeconds();
            Elderly elderly = elderlyRepository.findByElderlyId(alert.getElderlyId()).orElse(null);
            String elderlyName = elderly != null ? elderly.getName() : "Senior Citizen";
            String neighborPhone = elderly != null ? elderly.getNeighborPhone() : null;
            String secondaryPhone = elderly != null ? elderly.getSecondaryGuardianPhone() : null;
            String location = elderly != null ? elderly.getLocation() : "Home";

            // 1. Prefall Escalation (5 Minutes = 300 Seconds)
            if ("PREFALL".equalsIgnoreCase(alert.getAlertType()) && elapsedSeconds >= 300) {
                System.out.printf("[PREFALL ESCALATION] Alert %s unacknowledged for %d seconds (>5 mins)! Calling neighbor.%n", alert.getAlertId(), elapsedSeconds);
                alert.setStatus("ESCALATED_PREFALL_NEIGHBOR_CALL");
                alertRecordRepository.save(alert);

                if (neighborPhone != null && !neighborPhone.isEmpty()) {
                    twilioService.makeNeighborCall(neighborPhone, elderlyName, location + " (Prefall unacknowledged after 5 mins)");
                }
            }

            // 2. Fall Escalation Tier 1 (2 Minutes = 120 Seconds) -> Ensure Neighbor Call
            if ("FALL_DETECTED".equalsIgnoreCase(alert.getAlertType()) && elapsedSeconds >= 120 && elapsedSeconds < 240) {
                if (!"ESCALATED_FALL_NEIGHBOR_TIER1".equals(alert.getStatus())) {
                    System.out.printf("[FALL TIER 1 ESCALATION] Fall alert %s unacknowledged after 2 mins! Re-calling neighbor.%n", alert.getAlertId());
                    alert.setStatus("ESCALATED_FALL_NEIGHBOR_TIER1");
                    alertRecordRepository.save(alert);

                    if (neighborPhone != null && !neighborPhone.isEmpty()) {
                        twilioService.makeNeighborCall(neighborPhone, elderlyName, location);
                    }
                }
            }

            // 3. Fall Escalation Tier 2 (4 Minutes = 240 Seconds) -> Call Secondary Guardian
            if ("FALL_DETECTED".equalsIgnoreCase(alert.getAlertType()) && elapsedSeconds >= 240) {
                System.out.printf("[FALL TIER 2 ESCALATION] Fall alert %s unacknowledged after 4 mins! Calling secondary guardian.%n", alert.getAlertId());
                alert.setStatus("ESCALATED_FALL_SECONDARY_GUARDIAN");
                alertRecordRepository.save(alert);

                if (secondaryPhone != null && !secondaryPhone.isEmpty()) {
                    twilioService.sendUrgentAlertSms(secondaryPhone, elderlyName, location + " (No response from main guardian/neighbor in 4 mins)", alert.getDeviceId());
                    twilioService.makeSecondaryGuardianCall(secondaryPhone, elderlyName, location);
                }
            }
        }
    }

    public String getGuardianPhoneForElderly(String deviceId) {
        String elderlyId = resolveElderlyIdForDevice(deviceId);
        List<Guardian> guardians = getAllGuardiansForElderly(elderlyId);
        if (!guardians.isEmpty() && guardians.get(0).getPhone() != null && !guardians.get(0).getPhone().isEmpty()) {
            return guardians.get(0).getPhone();
        }
        return "";
    }

    public void notifyGuardianNoResponse(String elderlyName,
                                        String deviceId,
                                        String location) {

        String elderlyId = resolveElderlyIdForDevice(deviceId);
        List<Guardian> guardians = getAllGuardiansForElderly(elderlyId);

        for (Guardian guardian : guardians) {
            if (guardian.getPhone() != null && !guardian.getPhone().isEmpty()) {
                twilioService.sendUrgentAlertSms(
                        guardian.getPhone(),
                        elderlyName,
                        "No guardian has responded. Immediate assistance required at " + location,
                        deviceId
                );

                twilioService.makeNoResponseAlertCall(
                        guardian.getPhone(),
                        elderlyName,
                        location
                );
            }
        }
    }

    public void notifyGuardianFall(String elderlyName, String deviceId, String location) {
        // Use fall detection alert to notify guardians about a fall event
        handleFallAlert(deviceId, "Urgent Belt", null, null,
                "Fall detected for " + elderlyName + " at " + location);
    }

    public void switchUser(String elderlyId) {
        deviceCurrentUserMap.put("vois_belt", elderlyId);
        System.out.println("Device 'vois_belt' switched to user: " + elderlyId);
    }

    public String getCurrentUser() {
        return deviceCurrentUserMap.getOrDefault("vois_belt", "senior_user");
    }

    public List<String> getAvailableUsers() {
        return elderlyRepository.findAll().stream().map(Elderly::getElderlyId).toList();
    }
}
