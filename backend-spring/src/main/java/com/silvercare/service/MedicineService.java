package com.silvercare.service;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.silvercare.dto.MedicineAddRequest;
import com.silvercare.dto.MedicineConfirmRequest;
import com.silvercare.dto.SuggestionRequest;
import com.silvercare.entity.Elderly;
import com.silvercare.entity.GuardianSuggestion;
import com.silvercare.entity.Medicine;
import com.silvercare.entity.MedicineConfirmationHistory;
import com.silvercare.repository.ElderlyRepository;
import com.silvercare.repository.GuardianSuggestionRepository;
import com.silvercare.repository.MedicineRepository;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;

import java.time.LocalDateTime;
import java.time.LocalTime;
import java.time.format.DateTimeFormatter;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;

@Service
public class MedicineService {

    @Autowired
    private MedicineRepository medicineRepository;

    @Autowired
    private ElderlyRepository elderlyRepository;

    @Autowired
    private GuardianSuggestionRepository suggestionRepository;

    @Autowired
    private ElderlyNotificationService elderlyNotificationService;

    @Autowired
    private FallDetectionService fallDetectionService;

    @Autowired
    private FirebaseEncryptionService firebaseEncryptionService;

    private final ObjectMapper objectMapper = new ObjectMapper();

    // Track snoozed reminders (key: elderlyId_medicineId -> snoozeUntil)
    private final Map<String, LocalDateTime> snoozedReminders = new ConcurrentHashMap<>();

    public Medicine addMedicine(MedicineAddRequest request) {
        String guardianUsername = request.getGuardianUsername().trim();
        String elderlyId = request.getElderlyId().trim();

        Elderly elderly = elderlyRepository.findByElderlyId(elderlyId)
                .orElseThrow(() -> new IllegalArgumentException("Elderly person not found"));

        Medicine medicine = Medicine.builder()
                .elderlyId(elderlyId)
                .medicineName(request.getMedicineName())
                .dosage(request.getDosage())
                .times(request.getTimes() != null ? request.getTimes() : new ArrayList<>())
                .instructions(request.getInstructions() != null ? request.getInstructions() : "")
                .startDate(request.getStartDate())
                .endDate(request.getEndDate())
                .active(true)
                .createdAt(LocalDateTime.now().toString())
                .confirmationHistory(new ArrayList<>())
                .build();

        Medicine saved = medicineRepository.save(medicine);

        // Encrypt & Save to Firebase
        try {
            firebaseEncryptionService.saveToFirebaseEncrypted("medicines", saved.getId().toString(), objectMapper.writeValueAsString(saved));
        } catch (Exception ignored) {}

        return saved;
    }

    public void deleteMedicine(Long medicineId, String guardianUsername, String elderlyId) {
        Medicine medicine = medicineRepository.findById(medicineId)
                .orElseThrow(() -> new IllegalArgumentException("Medicine not found"));

        medicineRepository.delete(medicine);
    }

    public List<Medicine> getActiveMedicines(String elderlyId) {
        return medicineRepository.findByElderlyIdAndActiveTrue(elderlyId);
    }

    public List<Medicine> getAllMedicines(String elderlyId) {
        return medicineRepository.findByElderlyId(elderlyId);
    }

    public String confirmMedicineTaken(MedicineConfirmRequest request) {
        Medicine medicine = medicineRepository.findById(request.getMedicineId())
                .orElseThrow(() -> new IllegalArgumentException("Medicine not found"));

        MedicineConfirmationHistory history = MedicineConfirmationHistory.builder()
                .timeTaken(request.getTimeTaken() != null ? request.getTimeTaken() : LocalTime.now().format(DateTimeFormatter.ofPattern("HH:mm")))
                .taken(request.getTaken())
                .timestamp(LocalDateTime.now().toString())
                .type("manual_confirm")
                .build();

        medicine.getConfirmationHistory().add(history);
        Medicine updated = medicineRepository.save(medicine);

        // Sync to Firebase
        try {
            firebaseEncryptionService.saveToFirebaseEncrypted("medicines", updated.getId().toString(), objectMapper.writeValueAsString(updated));
        } catch (Exception ignored) {}

        return request.getTaken() != null && request.getTaken() ? "taken" : "not taken";
    }

    public void handleMedicineResponse(String elderlyId, Long medicineId, String response) {
        String key = elderlyId + "_" + medicineId;
        Medicine medicine = medicineRepository.findById(medicineId).orElse(null);

        if (medicine == null) {
            return;
        }

        if ("taken".equalsIgnoreCase(response)) {
            MedicineConfirmationHistory confirmation = MedicineConfirmationHistory.builder()
                    .timeTaken(LocalTime.now().format(DateTimeFormatter.ofPattern("HH:mm")))
                    .taken(true)
                    .timestamp(LocalDateTime.now().toString())
                    .type("automatic_reminder")
                    .build();
            medicine.getConfirmationHistory().add(confirmation);
            medicineRepository.save(medicine);
            snoozedReminders.remove(key);
            elderlyNotificationService.clearElderlyNotification(elderlyId, medicineId);
            System.out.println("✅ Medicine marked as taken for " + elderlyId);

        } else if ("snooze".equalsIgnoreCase(response)) {
            snoozedReminders.put(key, LocalDateTime.now().plusMinutes(2));
            System.out.println("⏰ Medicine reminder snoozed for 2 minutes for " + key);

        } else if ("not_taken".equalsIgnoreCase(response)) {
            MedicineConfirmationHistory confirmation = MedicineConfirmationHistory.builder()
                    .timeTaken(LocalTime.now().format(DateTimeFormatter.ofPattern("HH:mm")))
                    .taken(false)
                    .timestamp(LocalDateTime.now().toString())
                    .type("missed_dose")
                    .build();
            medicine.getConfirmationHistory().add(confirmation);
            medicineRepository.save(medicine);
            snoozedReminders.remove(key);
            elderlyNotificationService.clearElderlyNotification(elderlyId, medicineId);
            System.out.println("❌ Medicine marked as missed for " + elderlyId);
        }
    }

    public Map<String, LocalDateTime> getSnoozedReminders() {
        return snoozedReminders;
    }

    public List<GuardianSuggestion> getSuggestions(String elderlyId) {
        return suggestionRepository.findByElderlyId(elderlyId);
    }

    public GuardianSuggestion addSuggestion(String elderlyId, SuggestionRequest request) {
        GuardianSuggestion suggestion = GuardianSuggestion.builder()
                .elderlyId(elderlyId)
                .guardianUsername(request.getGuardianUsername())
                .suggestion(request.getSuggestion())
                .createdAt(LocalDateTime.now().toString())
                .active(true)
                .build();

        return suggestionRepository.save(suggestion);
    }
}
