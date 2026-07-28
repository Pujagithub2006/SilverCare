package com.silvercare.service;

import com.silvercare.entity.Medicine;
import org.springframework.stereotype.Service;

import java.time.LocalDateTime;
import java.util.*;

@Service
public class ElderlyNotificationService {

    public static class ElderlySession {
        private String elderlyId;
        private String deviceInfo;
        private LocalDateTime lastActive;

        public ElderlySession(String elderlyId, String deviceInfo) {
            this.elderlyId = elderlyId;
            this.deviceInfo = deviceInfo;
            this.lastActive = LocalDateTime.now();
        }

        public String getElderlyId() { return elderlyId; }
        public String getDeviceInfo() { return deviceInfo; }
        public LocalDateTime getLastActive() { return lastActive; }
    }

    private final Map<String, ElderlySession> elderlySessions = new HashMap<>();
    private final Map<String, Map<String, Object>> activeNotifications = new HashMap<>();

    public void registerElderlySession(String elderlyId, String deviceInfo) {
        elderlySessions.put(elderlyId, new ElderlySession(elderlyId, deviceInfo));
        System.out.println("👤 [ELDERLY] " + elderlyId + " logged in on " + deviceInfo);
    }

    public void unregisterElderlySession(String elderlyId) {
        elderlySessions.remove(elderlyId);
        System.out.println("👋 [ELDERLY] " + elderlyId + " logged out");
    }

    public boolean isElderlyLoggedIn(String elderlyId) {
        return elderlySessions.containsKey(elderlyId);
    }

    public boolean sendElderlyNotification(String elderlyId, Medicine medicine, String notificationType) {
        if (!elderlySessions.containsKey(elderlyId)) {
            System.out.println("⚠️ [ELDERLY] " + elderlyId + " not logged in - skipping notification");
            return false;
        }

        ElderlySession session = elderlySessions.get(elderlyId);

        Map<String, Object> notificationData = new LinkedHashMap<>();
        notificationData.put("elderly_id", elderlyId);
        notificationData.put("medicine", medicine);
        notificationData.put("message", "Time to take your medicine: " + medicine.getMedicineName() + " - " + medicine.getDosage());
        notificationData.put("type", notificationType != null ? notificationType : "medicine_reminder");
        notificationData.put("timestamp", LocalDateTime.now().toString());
        notificationData.put("voice_message", "Time to take your medicine. Please take " + medicine.getDosage() + " of " + medicine.getMedicineName());
        notificationData.put("options", List.of("taken", "snooze"));
        notificationData.put("device_info", session.getDeviceInfo());

        String key = elderlyId + "_" + medicine.getId();
        activeNotifications.put(key, notificationData);

        System.out.println("📱 [ELDERLY NOTIFICATION] Sent to " + elderlyId + " for medicine: " + medicine.getMedicineName());
        return true;
    }

    public Map<String, Map<String, Object>> getElderlyNotifications(String elderlyId) {
        Map<String, Map<String, Object>> userNotifications = new LinkedHashMap<>();
        for (Map.Entry<String, Map<String, Object>> entry : activeNotifications.entrySet()) {
            if (elderlyId.equals(entry.getValue().get("elderly_id"))) {
                userNotifications.put(entry.getKey(), entry.getValue());
            }
        }
        return userNotifications;
    }

    public Map<String, Map<String, Object>> getAllActiveNotifications() {
        return new LinkedHashMap<>(activeNotifications);
    }

    public void clearElderlyNotification(String elderlyId, Long medicineId) {
        String key = elderlyId + "_" + medicineId;
        activeNotifications.remove(key);
        System.out.println("✅ [ELDERLY] Notification cleared for " + key);
    }
}
