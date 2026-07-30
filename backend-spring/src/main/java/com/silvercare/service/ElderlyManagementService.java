package com.silvercare.service;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.silvercare.dto.ElderlyLoginRequest;
import com.silvercare.dto.ElderlyRegisterRequest;
import com.silvercare.dto.ElderlyUpdateRequest;
import com.silvercare.entity.Elderly;
import com.silvercare.entity.Guardian;
import com.silvercare.entity.GuardianElderlyLink;
import com.silvercare.repository.AlertRecordRepository;
import com.silvercare.repository.ElderlyRepository;
import com.silvercare.repository.GuardianElderlyLinkRepository;
import com.silvercare.repository.GuardianRepository;
import com.silvercare.repository.GuardianSuggestionRepository;
import com.silvercare.repository.MedicineRepository;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.LocalDateTime;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.LinkedHashMap;
import java.util.Optional;

@Service
public class ElderlyManagementService {

    @Autowired
    private ElderlyRepository elderlyRepository;

    @Autowired
    private GuardianRepository guardianRepository;

    @Autowired
    private GuardianElderlyLinkRepository guardianElderlyLinkRepository;

    @Autowired
    private MedicineRepository medicineRepository;

    @Autowired
    private AlertRecordRepository alertRecordRepository;

    @Autowired
    private GuardianSuggestionRepository suggestionRepository;

    @Autowired
    private GuardianAuthService guardianAuthService;

    @Autowired
    private FirebaseEncryptionService firebaseEncryptionService;

    @Transactional
    public void clearAllData() {
        try {
            medicineRepository.deleteAll();
            suggestionRepository.deleteAll();
            alertRecordRepository.deleteAll();
            guardianElderlyLinkRepository.deleteAll();
            elderlyRepository.deleteAll();
            guardianRepository.deleteAll();
            System.out.println("🗑️ [DATABASE CLEAN] All database entries deleted successfully!");
        } catch (Exception e) {
            System.err.println("Error clearing database entries: " + e.getMessage());
        }
    }

    private final ObjectMapper objectMapper = new ObjectMapper();

    public Elderly registerElderly(ElderlyRegisterRequest request) {
        String guardianUsername = request.getGuardianUsername();
        String guardianPassword = request.getGuardianPassword();

        if (guardianUsername == null || guardianUsername.trim().isEmpty()) {
            throw new IllegalArgumentException("Guardian username is required");
        }

        Guardian guardian = guardianRepository.findByUsername(guardianUsername)
                .orElseThrow(() -> new IllegalArgumentException("Guardian username '" + guardianUsername + "' not found"));

        if (!guardianAuthService.verifyPassword(guardian.getPasswordHash(), guardianPassword)) {
            throw new SecurityException("Invalid guardian password");
        }

        String rawName = request.getName();
        if (rawName == null || rawName.trim().isEmpty()) {
            throw new IllegalArgumentException("Elderly name is required");
        }

        String elderlyId = rawName.toLowerCase().trim().replaceAll("\\s+", "_");

        Elderly elderly = Elderly.builder()
                .elderlyId(elderlyId)
                .name(rawName.trim())
                .age(request.getAge() != null ? request.getAge() : 65)
                .medicalHistory(request.getMedicalHistory() != null ? request.getMedicalHistory() : "")
                .phone(request.getPhone() != null ? request.getPhone() : "")
                .location(request.getLocation() != null ? request.getLocation() : "Home")
                .guardianUsername(guardianUsername)
                .createdAt(LocalDateTime.now().toString())
                .build();

        Elderly savedElderly = elderlyRepository.save(elderly);

        boolean linkExists = guardianElderlyLinkRepository.findByGuardianUsernameAndElderlyId(guardianUsername, elderlyId).isPresent();
        if (!linkExists) {
            GuardianElderlyLink link = new GuardianElderlyLink(guardianUsername, elderlyId, "Primary");
            guardianElderlyLinkRepository.save(link);
        }

        if (guardian.getElderlyLinked() == null) {
            guardian.setElderlyLinked(new ArrayList<>());
        }
        if (!guardian.getElderlyLinked().contains(elderlyId)) {
            guardian.getElderlyLinked().add(elderlyId);
            guardianRepository.save(guardian);
        }

        try {
            firebaseEncryptionService.saveToFirebaseEncrypted("elderly_profiles", elderlyId, objectMapper.writeValueAsString(savedElderly));
        } catch (Exception ignored) {}

        return savedElderly;
    }

    public List<Elderly> getElderlyByGuardian(String guardianUsername) {
        return elderlyRepository.findByGuardianUsername(guardianUsername);
    }

    public List<Elderly> getLinkedElderlyForGuardian(String guardianUsername) {
        return getElderlyByGuardian(guardianUsername);
    }

    public Elderly getElderly(String elderlyId) {
        return elderlyRepository.findByElderlyId(elderlyId).orElse(null);
    }

    public Elderly loginElderly(ElderlyLoginRequest request) {
        String phone = request.getPhone();
        String name = request.getName();

        if (phone != null && !phone.trim().isEmpty() && name != null && !name.trim().isEmpty()) {
            List<Elderly> matches = elderlyRepository.findByPhoneAndNameIgnoreCase(phone.trim(), name.trim());
            if (!matches.isEmpty()) {
                return matches.get(0);
            }
        }
        if (name != null && !name.trim().isEmpty()) {
            String derivedId = name.toLowerCase().trim().replaceAll("\\s+", "_");
            Optional<Elderly> byId = elderlyRepository.findByElderlyId(derivedId);
            if (byId.isPresent()) return byId.get();
        }

        String searchId = (name != null && !name.trim().isEmpty()) ? name.toLowerCase().trim().replaceAll("\\s+", "_") : "elderly_unknown";
        return Elderly.builder()
                .elderlyId(searchId)
                .name(name != null ? name : "Senior Citizen")
                .phone(phone != null ? phone : "")
                .guardianUsername("john_guardian")
                .location("Home")
                .age(70)
                .build();
    }

    @Transactional
    public Elderly updateElderly(ElderlyUpdateRequest request) {
        String elderlyId = request.getElderlyId();
        if (elderlyId == null || elderlyId.trim().isEmpty()) {
            throw new IllegalArgumentException("Elderly ID is required for update");
        }

        Elderly elderly = elderlyRepository.findByElderlyId(elderlyId)
                .orElseThrow(() -> new IllegalArgumentException("Elderly profile not found for ID: " + elderlyId));

        if (request.getMedicalHistory() != null) {
            elderly.setMedicalHistory(request.getMedicalHistory());
        }
        if (request.getPhone() != null) {
            elderly.setPhone(request.getPhone());
        }
        if (request.getLocation() != null) {
            elderly.setLocation(request.getLocation());
        }
        if (request.getAge() != null) {
            elderly.setAge(request.getAge());
        }

        Elderly updated = elderlyRepository.save(elderly);

        try {
            firebaseEncryptionService.saveToFirebaseEncrypted("elderly_profiles", elderlyId, objectMapper.writeValueAsString(updated));
        } catch (Exception ignored) {}

        return updated;
    }
}
