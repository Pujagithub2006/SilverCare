package com.silvercare.service;

import com.silvercare.dto.ElderlyLoginRequest;
import com.silvercare.dto.ElderlyRegisterRequest;
import com.silvercare.dto.ElderlyUpdateRequest;
import com.silvercare.entity.Elderly;
import com.silvercare.entity.Guardian;
import com.silvercare.repository.ElderlyRepository;
import com.silvercare.repository.GuardianRepository;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;

import java.time.LocalDateTime;
import java.util.List;
import java.util.Optional;

@Service
public class ElderlyManagementService {

    @Autowired
    private ElderlyRepository elderlyRepository;

    @Autowired
    private GuardianRepository guardianRepository;

    @Autowired
    private GuardianAuthService guardianAuthService;

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

        // Check if guardian already has an elderly linked (1:1 relationship constraint)
        List<Elderly> existingElderly = elderlyRepository.findByGuardianUsername(guardianUsername);
        if (!existingElderly.isEmpty()) {
            throw new IllegalArgumentException(String.format(
                    "Guardian '%s' is already linked to '%s'. One guardian can only link to one elderly person.",
                    guardianUsername, existingElderly.get(0).getName()
            ));
        }

        // Create elderly_id = guardian_username + "_" + name_lowercase_replaced
        String cleanName = request.getName().toLowerCase().trim().replaceAll("\\s+", "_");
        String elderlyId = guardianUsername + "_" + cleanName;

        Elderly elderly = Elderly.builder()
                .elderlyId(elderlyId)
                .name(request.getName())
                .age(request.getAge())
                .medicalHistory(request.getMedicalHistory() != null ? request.getMedicalHistory() : "")
                .phone(request.getPhone() != null ? request.getPhone() : "")
                .location(request.getLocation() != null ? request.getLocation() : "Home")
                .guardianUsername(guardianUsername)
                .createdAt(LocalDateTime.now().toString())
                .build();

        Elderly saved = elderlyRepository.save(elderly);

        // Update guardian link list
        if (!guardian.getElderlyLinked().contains(elderlyId)) {
            guardian.getElderlyLinked().add(elderlyId);
            guardianRepository.save(guardian);
        }

        return saved;
    }

    public Elderly loginElderly(ElderlyLoginRequest request) {
        String phone = request.getPhone() != null ? request.getPhone().trim() : "";
        String name = request.getName() != null ? request.getName().trim() : "";

        if (phone.isEmpty() || name.isEmpty()) {
            throw new IllegalArgumentException("Phone number and name are required");
        }

        Optional<Elderly> elderlyOpt = elderlyRepository.findByPhoneAndNameIgnoreCase(phone, name);

        if (elderlyOpt.isEmpty()) {
            // Also try fuzzy search if exact phone match has leading/trailing differences
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
        return elderlyRepository.findByGuardianUsername(guardianUsername);
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

        return elderlyRepository.save(elderly);
    }
}
