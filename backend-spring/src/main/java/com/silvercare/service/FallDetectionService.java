package com.silvercare.service;

import com.silvercare.entity.Elderly;
import com.silvercare.entity.Guardian;
import com.silvercare.repository.ElderlyRepository;
import com.silvercare.repository.GuardianRepository;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;

import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicBoolean;

@Service
public class FallDetectionService {

    private final AtomicBoolean fallDetected = new AtomicBoolean(false);

    // Device current user map (e.g. "vois_belt" -> current_user)
    private final Map<String, String> deviceCurrentUserMap = new ConcurrentHashMap<>();

    @Autowired
    private ElderlyRepository elderlyRepository;

    @Autowired
    private GuardianRepository guardianRepository;

    @Autowired
    private TwilioService twilioService;

    public void triggerFall(String deviceId, Double confidence) {
        System.out.println("[FALL DETECTED] Device: " + deviceId + ", Confidence: " + confidence);
        fallDetected.set(true);
    }

    public boolean isFallDetected() {
        return fallDetected.get();
    }

    public void clearFall() {
        fallDetected.set(false);
        System.out.println("[FALL CLEARED] Fall detection status reset");
    }

    public String getGuardianPhoneForElderly(String deviceId) {
        try {
            if ("vois_belt".equalsIgnoreCase(deviceId)) {
                String currentUser = deviceCurrentUserMap.getOrDefault("vois_belt", null);
                if (currentUser != null) {
                    Elderly elderly = elderlyRepository.findByElderlyId(currentUser).orElse(null);
                    if (elderly != null) {
                        Guardian guardian = guardianRepository.findByUsername(elderly.getGuardianUsername()).orElse(null);
                        if (guardian != null) {
                            return guardian.getPhone();
                        }
                    }
                }
            }

            // Fallback: look up elderly with deviceId matching ID or first elderly
            Elderly elderly = elderlyRepository.findByElderlyId(deviceId).orElse(null);
            if (elderly != null) {
                Guardian g = guardianRepository.findByUsername(elderly.getGuardianUsername()).orElse(null);
                if (g != null) return g.getPhone();
            }

            List<Elderly> all = elderlyRepository.findAll();
            if (!all.isEmpty()) {
                Guardian g = guardianRepository.findByUsername(all.get(0).getGuardianUsername()).orElse(null);
                if (g != null) return g.getPhone();
            }
        } catch (Exception e) {
            System.err.println("Error looking up guardian phone: " + e.getMessage());
        }
        return null;
    }

    public void notifyGuardianFall(String elderlyName, String deviceId, String location) {
        System.out.println("[GUARDIAN ALERT] Fall detected for " + elderlyName + " at " + location);
        String phone = getGuardianPhoneForElderly(deviceId);
        if (phone != null) {
            twilioService.sendFallAlertSms(phone, elderlyName, location, deviceId);
            twilioService.makeEmergencyCall(phone, elderlyName, location);
        } else {
            System.out.println("[SMS] ❌ No guardian found for elderly device: " + deviceId);
        }
    }

    public void notifyGuardianNoResponse(String elderlyName, String deviceId, String location) {
        System.out.println("[GUARDIAN URGENT ALERT] " + elderlyName + " DID NOT RESPOND TO FALL ALERT!");
        String phone = getGuardianPhoneForElderly(deviceId);
        if (phone != null) {
            twilioService.sendUrgentAlertSms(phone, elderlyName, location, deviceId);
            twilioService.makeNoResponseAlertCall(phone, elderlyName, location);
        } else {
            System.out.println("[SMS] ❌ No guardian found for elderly device: " + deviceId);
        }
    }

    public void switchUser(String elderlyId) {
        deviceCurrentUserMap.put("vois_belt", elderlyId);
        System.out.println("Device 'vois_belt' switched to user: " + elderlyId);
    }

    public String getCurrentUser() {
        return deviceCurrentUserMap.getOrDefault("vois_belt", null);
    }

    public List<String> getAvailableUsers() {
        return elderlyRepository.findAll().stream().map(Elderly::getElderlyId).toList();
    }
}
