package com.silvercare.service;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.silvercare.dto.ElderlyLoginRequest;
import com.silvercare.dto.ElderlyRegisterRequest;
import com.silvercare.dto.ElderlyUpdateRequest;
import com.silvercare.entity.Elderly;
import com.silvercare.entity.Guardian;
import com.silvercare.entity.GuardianElderlyLink;
import com.silvercare.repository.ElderlyRepository;
import com.silvercare.repository.GuardianElderlyLinkRepository;
import com.silvercare.repository.GuardianRepository;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.LocalDateTime;
import java.util.ArrayList;
import java.util.List;
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
    private GuardianAuthService guardianAuthService;

    @Autowired
    private FirebaseEncryptionService firebaseEncryptionService;

    private final ObjectMapper objectMapper = new ObjectMapper();

    public Elderly registerElderly(ElderlyRegisterRequest request) {
        String guardianUsername = request.getGuardianUsername().trim();
        String guardianPassword = request.getGuardianPassword();

        Guardian guardian = guardianAuthService.getGuardian(guardianUsername);
        if (guardian == null) {
            throw new IllegalArgumentException("Guardian username not found");
        }

        if (!guardianAuthService.verifyPassword(guardian.getPasswordHash(), guardianPassword)) {
            throw new SecurityException("Invalid guardian credentials");
        }

        // Many-to-Many Architecture: Multiple elderly per guardian & multiple guardians per elderly
        String cleanName = request.getName().toLowerCase().trim().replaceAll("\\s+", "_");
        String elderlyId = guardianUsername + "_" + cleanName + "_" + System.currentTimeMillis() % 1000;

        Elderly elderly = Elderly.builder()
                .elderlyId(elderlyId)
                .name(request.getName())
                .age(request.getAge())
                .medicalHistory(request.getMedicalHistory() != null ? request.getMedicalHistory() : "")
                .phone(request.getPhone() != null ? request.getPhone() : "")
                .location(request.getLocation() != null ? request.getLocation() : "Home")
                .guardianUsername(guardianUsername)
                .primaryBeltType("Waist Belt")
                .primaryDeviceId("vois_belt")
                .createdAt(LocalDateTime.now().toString())
                .build();

        Elderly saved = elderlyRepository.save(elderly);

        // Link in Guardian entity list
        if (!guardian.getElderlyLinked().contains(elderlyId)) {
            guardian.getElderlyLinked().add(elderlyId);
            guardianRepository.save(guardian);
        }

        // Save in Many-to-Many GuardianElderlyLink repository
        linkGuardianAndElderly(guardianUsername, elderlyId, "Primary Guardian");

        // Save encrypted record in Firebase
        try {
            firebaseEncryptionService.saveToFirebaseEncrypted("elderly_profiles", elderlyId, objectMapper.writeValueAsString(saved));
        } catch (Exception ignored) {}

        return saved;
    }

    public void linkGuardianAndElderly(String guardianUsername, String elderlyId, String relationship) {
        Optional<GuardianElderlyLink> existing = guardianElderlyLinkRepository.findByGuardianUsernameAndElderlyId(guardianUsername, elderlyId);
        if (existing.isEmpty()) {
            GuardianElderlyLink link = new GuardianElderlyLink(guardianUsername, elderlyId, relationship != null ? relationship : "Guardian");
            guardianElderlyLinkRepository.save(link);
            System.out.printf("🔗 [MANY-TO-MANY LINK] Linked Guardian '%s' with Elderly '%s'%n", guardianUsername, elderlyId);
        }
    }

    @Transactional
    public void unlinkGuardianAndElderly(String guardianUsername, String elderlyId) {
        guardianElderlyLinkRepository.deleteByGuardianUsernameAndElderlyId(guardianUsername, elderlyId);
        Guardian guardian = guardianAuthService.getGuardian(guardianUsername);
        if (guardian != null && guardian.getElderlyLinked() != null) {
            guardian.getElderlyLinked().remove(elderlyId);
            guardianRepository.save(guardian);
        }
    }

    public Elderly loginElderly(ElderlyLoginRequest request) {
        String phone = request.getPhone() != null ? request.getPhone().trim() : "";
        String name = request.getName() != null ? request.getName().trim() : "";

        if (phone.isEmpty() || name.isEmpty()) {
            throw new IllegalArgumentException("Phone number and name are required");
        }

        Optional<Elderly> elderlyOpt = elderlyRepository.findByPhoneAndNameIgnoreCase(phone, name);
        if (elderlyOpt.isEmpty()) {
            List<Elderly> all = elderlyRepository.findAll();
            for (Elderly e : all) {
                if (phone.equals(e.getPhone()) && name.equalsIgnoreCase(e.getName())) {
                    return e;
                }
            }
            throw new IllegalArgumentException("Elderly not found. Please check your name and phone number.");
        }

        return elderlyOpt.get();
    }

    public Elderly getElderly(String elderlyId) {
        return elderlyRepository.findByElderlyId(elderlyId).orElse(null);
    }

    public List<Elderly> getElderlyByGuardian(String guardianUsername) {
        List<GuardianElderlyLink> links = guardianElderlyLinkRepository.findByGuardianUsername(guardianUsername);
        List<Elderly> elderlyList = new ArrayList<>();
        for (GuardianElderlyLink link : links) {
            elderlyRepository.findByElderlyId(link.getElderlyId()).ifPresent(elderlyList::add);
        }

        // Also check legacy guardianUsername column
        List<Elderly> legacy = elderlyRepository.findByGuardianUsername(guardianUsername);
        for (Elderly e : legacy) {
            if (!elderlyList.contains(e)) {
                elderlyList.add(e);
            }
        }

        if (elderlyList.isEmpty()) {
            // Return all elderly if no specific link found (graceful default)
            return elderlyRepository.findAll();
        }

        return elderlyList;
    }

    public Elderly updateElderly(ElderlyUpdateRequest request) {
        String elderlyId = request.getElderlyId().trim();
        Elderly elderly = getElderly(elderlyId);

        if (elderly == null) {
            throw new IllegalArgumentException("Elderly profile not found");
        }

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

        // Update encrypted Firebase record
        try {
            firebaseEncryptionService.saveToFirebaseEncrypted("elderly_profiles", elderlyId, objectMapper.writeValueAsString(updated));
        } catch (Exception ignored) {}

        return updated;
    }
}
