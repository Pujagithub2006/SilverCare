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
        if (deviceId == null || deviceId.trim().isEmpty()) {
            return getCurrentUser();
        }
        String cleanId = deviceId.trim();

        String mappedUser = deviceCurrentUserMap.get(cleanId);
        if (mappedUser != null)
            return mappedUser;

        String activeUser = deviceCurrentUserMap.get("vois_belt");
        if (activeUser != null && ("vois_belt".equalsIgnoreCase(cleanId) || "unknown".equalsIgnoreCase(cleanId))) {
            return activeUser;
        }

        Optional<Elderly> elderlyOpt = elderlyRepository.findByElderlyId(cleanId);
        if (elderlyOpt.isPresent())
            return elderlyOpt.get().getElderlyId();

        Optional<Elderly> byDevId = elderlyRepository.findByPrimaryDeviceId(cleanId);
        if (byDevId.isPresent())
            return byDevId.get().getElderlyId();

        Optional<Elderly> byMac = elderlyRepository.findByPrimaryDeviceMac(cleanId);
        if (byMac.isPresent())
            return byMac.get().getElderlyId();

        List<Elderly> byName = elderlyRepository.findByNameIgnoreCase(cleanId);
        if (!byName.isEmpty())
            return byName.get(0).getElderlyId();

        List<Elderly> all = elderlyRepository.findAll();
        if (!all.isEmpty()) {
            // Pick most recently created elderly profile instead of legacy first entry
            return all.get(all.size() - 1).getElderlyId();
        }

        return cleanId;
    }

    /**
     * Finds ALL linked guardians for an elderly person (Many-to-Many support)
     */
    public List<Guardian> getAllGuardiansForElderly(String elderlyId) {
        List<Guardian> guardians = new ArrayList<>();
        Set<String> guardianUsernames = new HashSet<>();

        if (elderlyId == null || elderlyId.trim().isEmpty()) {
            return guardians;
        }

        // 1. Check explicit links table
        List<GuardianElderlyLink> links = guardianElderlyLinkRepository.findByElderlyId(elderlyId);
        for (GuardianElderlyLink link : links) {
            if (link.getGuardianUsername() != null && !link.getGuardianUsername().trim().isEmpty()) {
                guardianUsernames.add(link.getGuardianUsername());
            }
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

        return guardians;
    }

    /**
     * Handle PREFALL Alert: Notify Guardian & Neighbor immediately via SMS with
     * vital signs
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
        String neighborPhone = elderly != null
                ? (elderly.getNeighborPhone() != null ? elderly.getNeighborPhone() : elderly.getNeighbourPhone())
                : null;

        String alertId = "PREFALL_" + UUID.randomUUID().toString().substring(0, 8);
        AlertRecord alert = new AlertRecord(alertId, elderlyId, deviceId, beltType != null ? beltType : "Waist Belt",
                "PREFALL", "PENDING_ACK", audioMsg, lat, lng);
        alertRecordRepository.save(alert);

        // Send Prefall SMS with vitals (HR ~118, SpO2 ~94, Temp ~37.1) and GPS to Main
        // Guardian
        System.out.println("[PREFALL ALERT] Dispathing SMS to Guardian: " + mainPhone + " for Elderly: " + elderlyName);
        if (mainPhone != null && !mainPhone.isEmpty()) {
            boolean sent = twilioService.sendPrefallSms(mainPhone, elderlyName, location, 118, 94, 37.1, lat, lng);
            System.out.println("[PREFALL TWILIO SMS RESULT] Sent: " + sent + " to " + mainPhone);
        } else {
            System.err.println("[PREFALL ALERT WARNING] No guardian phone number found to send Twilio SMS!");
        }
        if (neighborPhone != null && !neighborPhone.isEmpty()) {
            twilioService.sendPrefallSms(neighborPhone, elderlyName, location, 118, 94, 37.1, lat, lng);
        }

        // Encrypt & Save to Firebase
        try {
            firebaseEncryptionService.saveToFirebaseEncrypted("alerts", alertId,
                    objectMapper.writeValueAsString(alert));
        } catch (Exception ignored) {
        }

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
        String neighborPhone = elderly != null
                ? (elderly.getNeighborPhone() != null ? elderly.getNeighborPhone() : elderly.getNeighbourPhone())
                : null;

        String alertId = "FALL_" + UUID.randomUUID().toString().substring(0, 8);
        AlertRecord alert = new AlertRecord(alertId, elderlyId, deviceId, beltType != null ? beltType : "Waist Belt",
                "FALL_DETECTED", "PENDING_ACK", audioMsg, lat, lng);
        alertRecordRepository.save(alert);

        fallDetected.set(true);

        // 1. Send SMS & Call Main Guardian immediately
        System.out.println("[FALL ALERT] Dispathing Emergency CALL & SMS to Guardian: " + mainPhone + " for Elderly: "
                + elderlyName);
        if (mainPhone != null && !mainPhone.isEmpty()) {
            boolean smsSent = twilioService.sendFallAlertSms(mainPhone, elderlyName, location, deviceId, lat, lng);
            boolean callMade = twilioService.makeEmergencyCall(mainPhone, elderlyName, location);
            System.out.println("[FALL TWILIO RESULT] Phone: " + mainPhone + ", SMS Sent: " + smsSent
                    + ", Emergency Call Made: " + callMade);
        } else {
            System.err.println(
                    "[FALL ALERT WARNING] No guardian phone number found to initiate Twilio Emergency Call & SMS!");
        }

        // 2. Send SMS & Call Neighbor immediately
        if (neighborPhone != null && !neighborPhone.isEmpty()) {
            twilioService.sendFallAlertSms(neighborPhone, elderlyName, location + " (Neighbor Emergency Alert)",
                    deviceId, lat, lng);
            twilioService.makeNeighborCall(neighborPhone, elderlyName, location);
        }

        // Encrypt & Save to Firebase
        try {
            firebaseEncryptionService.saveToFirebaseEncrypted("alerts", alertId,
                    objectMapper.writeValueAsString(alert));
        } catch (Exception ignored) {
        }

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
        AlertRecord alert = new AlertRecord(alertId, targetElderlyId, deviceId, "SOS Button", "SOS", "PENDING_ACK",
                "elderly pressed SOS.. go quickly", lat, lng);
        alertRecordRepository.save(alert);

        if (mainPhone != null && !mainPhone.isEmpty()) {
            twilioService.sendSosSmsAndCall(mainPhone, elderlyName, lat, lng);
        }

        try {
            firebaseEncryptionService.saveToFirebaseEncrypted("alerts", alertId,
                    objectMapper.writeValueAsString(alert));
        } catch (Exception ignored) {
        }

        return alert;
    }

    /**
     * Handle "I am Fine" Hardware Push Button: Only clears hardware alarm state,
     * does NOT acknowledge alerts
     * Elderly cannot acknowledge fall/prefall alerts - only guardians can
     */
    public Map<String, Object> handleIamFineButton(String deviceId, String elderlyId) {
        clearFall();
        String targetElderlyId = elderlyId != null ? elderlyId : resolveElderlyIdForDevice(deviceId);

        // Elderly button only clears hardware alarm, does NOT resolve alerts
        // Alerts remain pending for guardian acknowledgment

        Map<String, Object> result = new LinkedHashMap<>();
        result.put("status", "success");
        result.put("message", "Speaker alarm shut down. Alert remains pending for guardian acknowledgment.");
        result.put("stateName", "NORMAL (IMPACTED)");
        return result;
    }

    /**
     * Guardian Acknowledgment Logic with Yes/No Response
     * For Pre-fall: "Did you contact the elderly or send someone for help?"
     * For Fall: "Did neighbor contact you or did neighbor reach there?"
     */
    public AlertRecord acknowledgeAlert(String alertId, String guardianUsername, String responseType,
            String responseMessage) {
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

        Elderly elderly = elderlyRepository.findByElderlyId(alert.getElderlyId()).orElse(null);
        String elderlyName = elderly != null ? elderly.getName() : "Senior Citizen";
        String neighborPhone = elderly != null ? elderly.getNeighborPhone() : null;
        String secondaryPhone = elderly != null ? elderly.getSecondaryGuardianPhone() : null;
        String location = elderly != null ? elderly.getLocation() : "Home";

        // Handle Yes response - acknowledge and resolve
        if ("YES".equalsIgnoreCase(responseType)) {
            alert.setStatus("ACKNOWLEDGED");
            alert.setAcknowledgedBy(guardianUsername);
            alert.setAcknowledgedAt(LocalDateTime.now().toString());
            alert.setResponseMessage(responseMessage);
            alertRecordRepository.save(alert);
            clearFall();

            System.out.printf("[ALERT ACKNOWLEDGED] Guardian '%s' responded YES to alert %s: '%s'%n",
                    guardianUsername, alert.getAlertId(), responseMessage);
        }
        // Handle No response - immediate escalation
        else if ("NO".equalsIgnoreCase(responseType)) {
            if ("PREFALL".equalsIgnoreCase(alert.getAlertType())) {
                // Immediate SMS to neighbor
                alert.setStatus("ESCALATED_PREFALL_NEIGHBOR_SMS");
                alert.setAcknowledgedBy(guardianUsername);
                alert.setAcknowledgedAt(LocalDateTime.now().toString());
                alert.setResponseMessage(responseMessage);
                alertRecordRepository.save(alert);

                if (neighborPhone != null && !neighborPhone.isEmpty()) {
                    twilioService.sendPrefallSms(neighborPhone, elderlyName,
                            location + " (Guardian unable to help - immediate neighbor assistance needed)", 118, 94,
                            37.1, alert.getLatitude(), alert.getLongitude());
                }

                System.out.printf(
                        "[ALERT ESCALATED] Guardian '%s' responded NO to pre-fall alert %s - SMSing neighbor immediately%n",
                        guardianUsername, alert.getAlertId());
            } else if ("FALL_DETECTED".equalsIgnoreCase(alert.getAlertType())) {
                // Immediate call to guardian 2
                alert.setStatus("ESCALATED_FALL_GUARDIAN2_CALL");
                alert.setAcknowledgedBy(guardianUsername);
                alert.setAcknowledgedAt(LocalDateTime.now().toString());
                alert.setResponseMessage(responseMessage);
                alertRecordRepository.save(alert);

                if (secondaryPhone != null && !secondaryPhone.isEmpty()) {
                    twilioService.sendUrgentAlertSms(secondaryPhone, elderlyName,
                            location + " (Neighbor unable to help - immediate secondary guardian assistance needed)",
                            alert.getDeviceId());
                    twilioService.makeSecondaryGuardianCall(secondaryPhone, elderlyName, location);
                }

                System.out.printf(
                        "[ALERT ESCALATED] Guardian '%s' responded NO to fall alert %s - Calling guardian 2 immediately%n",
                        guardianUsername, alert.getAlertId());
            }
        }
        // Legacy acknowledgment (backward compatibility)
        else {
            alert.setStatus("ACKNOWLEDGED");
            alert.setAcknowledgedBy(guardianUsername);
            alert.setAcknowledgedAt(LocalDateTime.now().toString());
            alert.setResponseMessage(responseMessage);
            alertRecordRepository.save(alert);
            clearFall();

            System.out.printf("[ALERT ACKNOWLEDGED] Guardian '%s' acknowledged alert %s with message: '%s'%n",
                    guardianUsername, alert.getAlertId(), responseMessage);
        }

        try {
            firebaseEncryptionService.saveToFirebaseEncrypted("alerts", alert.getAlertId(),
                    objectMapper.writeValueAsString(alert));
        } catch (Exception ignored) {
        }

        return alert;
    }

    /**
     * Multi-Tier Scheduled Escalation Engine:
     * 1. Prefall: SMS guardian initially, if unacknowledged in 4 mins (240s) -> SMS
     * neighbor
     * 2. Fall: Call guardian initially, if unacknowledged in 2 mins (120s) -> Call
     * neighbor
     * 3. Fall: If neighbor unacknowledged in additional 2 mins (240s total) -> Call
     * guardian 2
     * 
     * Alert hierarchy completes even if hardware state changes (e.g., pre-fall ->
     * sudden movement)
     */
    @Scheduled(fixedDelay = 10000) // Check every 10 seconds
    public void runMultiTierEscalationEngine() {
        // Check all non-acknowledged alerts, not just PENDING_ACK
        List<AlertRecord> allActiveAlerts = alertRecordRepository.findByStatusIn(
                Arrays.asList("PENDING_ACK", "ESCALATED_PREFALL_NEIGHBOR_SMS", "ESCALATED_FALL_NEIGHBOR_CALL"));
        LocalDateTime now = LocalDateTime.now();

        for (AlertRecord alert : allActiveAlerts) {
            String createdAtStr = alert.getCreatedAt();
            if (createdAtStr == null || createdAtStr.isEmpty())
                continue;

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

            // 1. Prefall Escalation (4 Minutes = 240 Seconds) -> SMS Neighbor
            if ("PREFALL".equalsIgnoreCase(alert.getAlertType()) && "PENDING_ACK".equals(alert.getStatus())
                    && elapsedSeconds >= 240) {
                System.out.printf(
                        "[PREFALL ESCALATION] Alert %s unacknowledged for %d seconds (>4 mins)! SMSing neighbor.%n",
                        alert.getAlertId(), elapsedSeconds);
                alert.setStatus("ESCALATED_PREFALL_NEIGHBOR_SMS");
                alertRecordRepository.save(alert);

                if (neighborPhone != null && !neighborPhone.isEmpty()) {
                    twilioService.sendPrefallSms(neighborPhone, elderlyName,
                            location + " (Prefall unacknowledged after 4 mins)", 118, 94, 37.1, alert.getLatitude(),
                            alert.getLongitude());
                }
            }

            // 2. Fall Escalation Tier 1 (2 Minutes = 120 Seconds) -> Call Neighbor
            if ("FALL_DETECTED".equalsIgnoreCase(alert.getAlertType()) && "PENDING_ACK".equals(alert.getStatus())
                    && elapsedSeconds >= 120) {
                System.out.printf(
                        "[FALL TIER 1 ESCALATION] Fall alert %s unacknowledged after 2 mins! Calling neighbor.%n",
                        alert.getAlertId());
                alert.setStatus("ESCALATED_FALL_NEIGHBOR_CALL");
                alertRecordRepository.save(alert);

                if (neighborPhone != null && !neighborPhone.isEmpty()) {
                    twilioService.makeNeighborCall(neighborPhone, elderlyName,
                            location + " (Fall unacknowledged after 2 mins)");
                }
            }

            // 3. Fall Escalation Tier 2 (4 Minutes = 240 Seconds) -> Call Guardian 2
            if ("FALL_DETECTED".equalsIgnoreCase(alert.getAlertType())
                    && "ESCALATED_FALL_NEIGHBOR_CALL".equals(alert.getStatus()) && elapsedSeconds >= 240) {
                System.out.printf(
                        "[FALL TIER 2 ESCALATION] Fall alert %s unacknowledged after 4 mins! Calling guardian 2.%n",
                        alert.getAlertId());
                alert.setStatus("ESCALATED_FALL_GUARDIAN2_CALL");
                alertRecordRepository.save(alert);

                if (secondaryPhone != null && !secondaryPhone.isEmpty()) {
                    twilioService.sendUrgentAlertSms(secondaryPhone, elderlyName,
                            location + " (No response from main guardian/neighbor in 4 mins)", alert.getDeviceId());
                    twilioService.makeSecondaryGuardianCall(secondaryPhone, elderlyName, location);
                }
            }
        }
    }

    public String getGuardianPhoneForElderly(String deviceId) {
        String elderlyId = resolveElderlyIdForDevice(deviceId);
        List<Guardian> guardians = getAllGuardiansForElderly(elderlyId);
        if (!guardians.isEmpty()) {
            for (Guardian g : guardians) {
                if (g.getPhone() != null && !g.getPhone().trim().isEmpty()) {
                    return g.getPhone().trim();
                }
            }
        }

        Optional<Elderly> elderlyOpt = elderlyRepository.findByElderlyId(elderlyId);
        if (elderlyOpt.isPresent()) {
            Elderly e = elderlyOpt.get();
            if (e.getGuardianUsername() != null && !e.getGuardianUsername().trim().isEmpty()) {
                Optional<Guardian> gOpt = guardianRepository.findByUsername(e.getGuardianUsername().trim());
                if (gOpt.isPresent() && gOpt.get().getPhone() != null && !gOpt.get().getPhone().trim().isEmpty()) {
                    return gOpt.get().getPhone().trim();
                }
            }
            if (e.getSecondaryGuardianPhone() != null && !e.getSecondaryGuardianPhone().trim().isEmpty()) {
                return e.getSecondaryGuardianPhone().trim();
            }
            if (e.getPhone() != null && !e.getPhone().trim().isEmpty()) {
                return e.getPhone().trim();
            }
        }
        // 3. Fallback: Search all registered Guardians in the database (most recent
        // first)
        // Ensures that whatever phone number the user entered during guardian
        // registration/login is found!
        List<Guardian> allGuardians = guardianRepository.findAll();
        if (!allGuardians.isEmpty()) {
            for (int i = allGuardians.size() - 1; i >= 0; i--) {
                Guardian g = allGuardians.get(i);
                if (g.getPhone() != null && !g.getPhone().trim().isEmpty()) {
                    System.out.println("[GUARDIAN PHONE RESOLVED] Found phone " + g.getPhone().trim()
                            + " from registered guardian: " + g.getUsername());
                    return g.getPhone().trim();
                }
            }
        }

        // 4. Fallback: If deviceId itself is formatted as a phone number
        if (deviceId != null) {
            String cleanDev = deviceId.trim();
            if (cleanDev.startsWith("+") || (cleanDev.matches("^\\d{10,13}$"))) {
                return cleanDev;
            }
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
                        deviceId);

                twilioService.makeNoResponseAlertCall(
                        guardian.getPhone(),
                        elderlyName,
                        location);
            }
        }
    }

    public void notifyGuardianFall(String elderlyName, String deviceId, String location) {
        // Use fall detection alert to notify guardians about a fall event
        handleFallAlert(deviceId, "Urgent Belt", null, null,
                "Fall detected for " + elderlyName + " at " + location);
    }

    public void switchUser(String elderlyId) {
        if (elderlyId != null && !elderlyId.trim().isEmpty()) {
            String clean = elderlyId.trim();
            deviceCurrentUserMap.put("vois_belt", clean);
            deviceCurrentUserMap.put("device_3076f5f44e24", clean);
            deviceCurrentUserMap.put(clean, clean);

            elderlyRepository.findByElderlyId(clean).ifPresent(e -> {
                if (e.getPrimaryDeviceId() != null)
                    deviceCurrentUserMap.put(e.getPrimaryDeviceId(), clean);
                if (e.getPrimaryDeviceMac() != null)
                    deviceCurrentUserMap.put(e.getPrimaryDeviceMac(), clean);
            });
            System.out.println("[FALL DETECTION SERVICE] Device active user switched to: " + clean);
        }
    }

    public String getCurrentUser() {
        return deviceCurrentUserMap.getOrDefault("vois_belt", "senior_user");
    }

    public List<String> getAvailableUsers() {
        return elderlyRepository.findAll().stream().map(Elderly::getElderlyId).toList();
    }
}
