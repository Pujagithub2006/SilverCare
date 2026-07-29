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
        System.out.println("🚨 [FALL DETECTED] Device: " + deviceId + ", Confidence: " + confidence);
        fallDetected.set(true);
    }

    public boolean isFallDetected() {
        return fallDetected.get();
    }

    public void clearFall() {
        fallDetected.set(false);
        System.out.println("✅ [FALL CLEARED] Fall status reset.");
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
     * Handle PREFALL Alert: Notify ALL guardians immediately
     */
    public void handlePrefallAlert(String deviceId, String beltType, Double lat, Double lng, String audioMsg) {
        String elderlyId = resolveElderlyIdForDevice(deviceId);
        Elderly elderly = elderlyRepository.findByElderlyId(elderlyId).orElse(null);
        String elderlyName = elderly != null ? elderly.getName() : "Senior Citizen";

        List<Guardian> guardians = getAllGuardiansForElderly(elderlyId);
        System.out.printf("⚠️ [PREFALL HIERARCHY] Alerting %d linked guardians for %s (%s)%n", guardians.size(), elderlyName, beltType);

        String alertId = "PREFALL_" + UUID.randomUUID().toString().substring(0, 8);
        AlertRecord alert = new AlertRecord(alertId, elderlyId, deviceId, beltType, "PREFALL", "PENDING_ACK", audioMsg, lat, lng);
        alertRecordRepository.save(alert);

        // Encrypt & Save to Firebase
        try {
            firebaseEncryptionService.saveToFirebaseEncrypted("alerts", alertId, objectMapper.writeValueAsString(alert));
        } catch (Exception ignored) {}

        for (Guardian g : guardians) {
            if (g.getPhone() != null && !g.getPhone().isEmpty()) {
                twilioService.sendFallAlertSms(g.getPhone(), elderlyName, "Pre-fall instability detected on " + beltType, deviceId);
            }
        }
    }

    /**
     * Handle FALL_DETECTED Alert: Call & SMS ALL guardians, start 4-minute escalation window
     */
    public void handleFallAlert(String deviceId, String beltType, Double lat, Double lng, String audioMsg) {
        String elderlyId = resolveElderlyIdForDevice(deviceId);
        Elderly elderly = elderlyRepository.findByElderlyId(elderlyId).orElse(null);
        String elderlyName = elderly != null ? elderly.getName() : "Senior Citizen";
        String location = elderly != null && elderly.getLocation() != null ? elderly.getLocation() : "Home";

        List<Guardian> guardians = getAllGuardiansForElderly(elderlyId);
        System.out.printf("🚨 [FALL ALERT HIERARCHY] Calling & SMSing %d linked guardians for %s%n", guardians.size(), elderlyName);

        String alertId = "FALL_" + UUID.randomUUID().toString().substring(0, 8);
        AlertRecord alert = new AlertRecord(alertId, elderlyId, deviceId, beltType, "FALL_DETECTED", "PENDING_ACK", audioMsg, lat, lng);
        alertRecordRepository.save(alert);

        // Encrypt & Save to Firebase
        try {
            firebaseEncryptionService.saveToFirebaseEncrypted("alerts", alertId, objectMapper.writeValueAsString(alert));
        } catch (Exception ignored) {}

        for (Guardian g : guardians) {
            if (g.getPhone() != null && !g.getPhone().isEmpty()) {
                twilioService.sendFallAlertSms(g.getPhone(), elderlyName, location + " (" + beltType + ")", deviceId);
                twilioService.makeEmergencyCall(g.getPhone(), elderlyName, location);
            }
        }
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
            // Find latest pending alert for elderly
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

        System.out.printf("✅ [ALERT ACKNOWLEDGED] Guardian '%s' acknowledged alert %s with message: '%s'%n",
                guardianUsername, alert.getAlertId(), responseMessage);

        // Sync acknowledged state to Firebase
        try {
            firebaseEncryptionService.saveToFirebaseEncrypted("alerts", alert.getAlertId(), objectMapper.writeValueAsString(alert));
        } catch (Exception ignored) {}

        return alert;
    }

    /**
     * Scheduled escalation task: If no guardian acknowledges fall within 4 minutes (240 seconds), notify Neighbours
     */
    @Scheduled(fixedDelay = 20000) // Check every 20 seconds
    public void checkUnacknowledgedFallsAndEscalateToNeighbours() {
        List<AlertRecord> pendingAlerts = alertRecordRepository.findByStatus("PENDING_ACK");
        LocalDateTime now = LocalDateTime.now();

        for (AlertRecord alert : pendingAlerts) {
            if (!"FALL_DETECTED".equalsIgnoreCase(alert.getAlertType())) continue;

            LocalDateTime createdTime = LocalDateTime.parse(alert.getCreatedAt());
            long elapsedSeconds = Duration.between(createdTime, now).getSeconds();

            // 4 minutes escalation threshold (240 seconds)
            if (elapsedSeconds >= 240) {
                System.out.printf("🚨 [ESCALATION TO NEIGHBOUR] Alert %s unacknowledged for %d seconds (>4 mins)! Calling neighbours.%n",
                        alert.getAlertId(), elapsedSeconds);

                alert.setStatus("ESCALATED_TO_NEIGHBOUR");
                alertRecordRepository.save(alert);

                Elderly elderly = elderlyRepository.findByElderlyId(alert.getElderlyId()).orElse(null);
                String elderlyName = elderly != null ? elderly.getName() : "Senior Citizen";
                String neighbourPhone = elderly != null ? elderly.getNeighbourPhone() : null;
                String location = elderly != null ? elderly.getLocation() : "Home";

                if (neighbourPhone != null && !neighbourPhone.isEmpty()) {
                    System.out.println("📞 [NEIGHBOUR ESCALATION] Contacting neighbour at " + neighbourPhone);
                    twilioService.sendUrgentAlertSms(neighbourPhone, elderlyName, location + " (No response from guardians in 4 mins)", alert.getDeviceId());
                    twilioService.makeNoResponseAlertCall(neighbourPhone, elderlyName, location);
                } else {
                    // Fallback to calling primary guardian again or broadcast urgent SMS
                    List<Guardian> guardians = getAllGuardiansForElderly(alert.getElderlyId());
                    for (Guardian g : guardians) {
                        if (g.getPhone() != null) {
                            twilioService.sendUrgentAlertSms(g.getPhone(), elderlyName, "CRITICAL: Fall unacknowledged after 4 mins!", alert.getDeviceId());
                            twilioService.makeNoResponseAlertCall(g.getPhone(), elderlyName, location);
                        }
                    }
                }

                // Sync escalated state to Firebase
                try {
                    firebaseEncryptionService.saveToFirebaseEncrypted("alerts", alert.getAlertId(), objectMapper.writeValueAsString(alert));
                } catch (Exception ignored) {}
            }
        }
    }

    public String getGuardianPhoneForElderly(String deviceId) {
        String elderlyId = resolveElderlyIdForDevice(deviceId);
        List<Guardian> guardians = getAllGuardiansForElderly(elderlyId);
        if (!guardians.isEmpty() && guardians.get(0).getPhone() != null) {
            return guardians.get(0).getPhone();
        }
        return "+919322757538";
    }

    public void notifyGuardianFall(String elderlyName, String deviceId, String location) {
        String elderlyId = resolveElderlyIdForDevice(deviceId);
        handleFallAlert(deviceId, "Waist Belt", 18.5204, 73.8567, null);
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
