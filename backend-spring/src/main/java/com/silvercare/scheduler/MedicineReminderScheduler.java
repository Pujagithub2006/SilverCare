package com.silvercare.scheduler;

import com.silvercare.entity.Elderly;
import com.silvercare.entity.Medicine;
import com.silvercare.repository.ElderlyRepository;
import com.silvercare.repository.MedicineRepository;
import com.silvercare.service.ElderlyNotificationService;
import com.silvercare.service.MedicineService;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;

import java.time.LocalDate;
import java.time.LocalDateTime;
import java.time.LocalTime;
import java.time.format.DateTimeFormatter;
import java.util.*;

@Component
public class MedicineReminderScheduler {

    @Autowired
    private MedicineRepository medicineRepository;

    @Autowired
    private ElderlyRepository elderlyRepository;

    @Autowired
    private MedicineService medicineService;

    @Autowired
    private ElderlyNotificationService elderlyNotificationService;

    private final Map<String, LocalDateTime> activeReminders = new HashMap<>();

    @Scheduled(fixedRate = 30000) // Check every 30 seconds
    public void checkMedicineTimes() {
        try {
            String currentTime = LocalTime.now().format(DateTimeFormatter.ofPattern("HH:mm"));
            String currentDate = LocalDate.now().format(DateTimeFormatter.ofPattern("yyyy-MM-dd"));

            // 1. Process snoozed reminders
            checkSnoozedReminders();

            // 2. Process active scheduled medicine times
            List<Elderly> allElderly = elderlyRepository.findAll();
            for (Elderly elderly : allElderly) {
                String elderlyId = elderly.getElderlyId();
                List<Medicine> medicines = medicineRepository.findByElderlyIdAndActiveTrue(elderlyId);

                for (Medicine medicine : medicines) {
                    if (medicine.getStartDate() != null && medicine.getEndDate() != null) {
                        if (currentDate.compareTo(medicine.getStartDate()) < 0 || currentDate.compareTo(medicine.getEndDate()) > 0) {
                            continue;
                        }
                    }

                    List<String> times = medicine.getTimes();
                    if (times != null && times.contains(currentTime)) {
                        String key = elderlyId + "_" + medicine.getId();
                        LocalDateTime lastReminded = activeReminders.get(key);

                        if (lastReminded == null) {
                            triggerMedicineReminder(elderly, medicine);
                            activeReminders.put(key, LocalDateTime.now());
                        } else if (java.time.Duration.between(lastReminded, LocalDateTime.now()).getSeconds() > 120) {
                            triggerMedicineReminder(elderly, medicine);
                            activeReminders.put(key, LocalDateTime.now());
                        }
                    }
                }
            }
        } catch (Exception e) {
            System.err.println("❌ [MEDICINE SCHEDULER ERROR] " + e.getMessage());
        }
    }

    private void checkSnoozedReminders() {
        LocalDateTime now = LocalDateTime.now();
        Map<String, LocalDateTime> snoozed = medicineService.getSnoozedReminders();
        List<String> toRemove = new ArrayList<>();

        for (Map.Entry<String, LocalDateTime> entry : snoozed.entrySet()) {
            if (now.isAfter(entry.getValue()) || now.isEqual(entry.getValue())) {
                String[] parts = entry.getKey().split("_");
                if (parts.length == 2) {
                    String elderlyId = parts[0];
                    Long medicineId = Long.parseLong(parts[1]);
                    Elderly elderly = elderlyRepository.findByElderlyId(elderlyId).orElse(null);
                    Medicine medicine = medicineRepository.findById(medicineId).orElse(null);

                    if (elderly != null && medicine != null) {
                        triggerMedicineReminder(elderly, medicine);
                    }
                }
                toRemove.add(entry.getKey());
            }
        }

        for (String key : toRemove) {
            snoozed.remove(key);
        }
    }

    private void triggerMedicineReminder(Elderly elderly, Medicine medicine) {
        boolean sent = elderlyNotificationService.sendElderlyNotification(elderly.getElderlyId(), medicine, "medicine_reminder");
        System.out.println("🔔 [MEDICINE REMINDER] Triggered for " + elderly.getName());
        System.out.println("   Medicine: " + medicine.getMedicineName() + " | Dosage: " + medicine.getDosage());
        System.out.println("   Elderly Device Notified: " + (sent ? "Yes" : "No - Not logged in"));
    }
}
