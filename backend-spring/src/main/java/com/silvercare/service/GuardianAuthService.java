package com.silvercare.service;

import com.silvercare.dto.GuardianLoginRequest;
import com.silvercare.dto.GuardianRegisterRequest;
import com.silvercare.dto.GuardianUpdateRequest;
import com.silvercare.entity.Guardian;
import com.silvercare.repository.GuardianRepository;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.time.LocalDateTime;
import java.util.ArrayList;
import java.util.Optional;

@Service
public class GuardianAuthService {

    @Autowired
    private GuardianRepository guardianRepository;

    public String hashPassword(String password) {
        if (password == null) return "";
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            byte[] hash = digest.digest(password.getBytes(StandardCharsets.UTF_8));
            StringBuilder hexString = new StringBuilder();
            for (byte b : hash) {
                String hex = Integer.toHexString(0xff & b);
                if (hex.length() == 1) hexString.append('0');
                hexString.append(hex);
            }
            return hexString.toString();
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException("SHA-256 algorithm not available", e);
        }
    }

    public boolean verifyPassword(String storedHash, String providedPassword) {
        if (storedHash == null || providedPassword == null) return false;
        return storedHash.equals(hashPassword(providedPassword));
    }

    public Guardian registerGuardian(GuardianRegisterRequest request) {
        String username = request.getUsername().trim();
        
        if (guardianRepository.existsByUsername(username)) {
            throw new IllegalArgumentException("Username already exists");
        }

        if (request.getPassword() == null || request.getPassword().length() < 6) {
            throw new IllegalArgumentException("Password must be at least 6 characters long");
        }

        Guardian guardian = Guardian.builder()
                .username(username)
                .name(request.getName())
                .passwordHash(hashPassword(request.getPassword()))
                .phone(request.getPhone())
                .email(request.getEmail())
                .elderlyLinked(new ArrayList<>())
                .createdAt(LocalDateTime.now().toString())
                .build();

        return guardianRepository.save(guardian);
    }

    public Guardian loginGuardian(GuardianLoginRequest request) {
        String username = request.getUsername().trim();
        Optional<Guardian> guardianOpt = guardianRepository.findByUsername(username);

        if (guardianOpt.isEmpty()) {
            throw new SecurityException("Invalid username or password");
        }

        Guardian guardian = guardianOpt.get();
        if (!verifyPassword(guardian.getPasswordHash(), request.getPassword())) {
            throw new SecurityException("Invalid username or password");
        }

        return guardian;
    }

    public Guardian getGuardian(String username) {
        return guardianRepository.findByUsername(username.trim())
                .orElse(null);
    }

    public Guardian updateGuardian(GuardianUpdateRequest request) {
        String username = request.getUsername().trim();
        Guardian guardian = getGuardian(username);

        if (guardian == null) {
            throw new IllegalArgumentException("Guardian not found");
        }

        if (request.getPhone() != null) {
            guardian.setPhone(request.getPhone());
        }
        if (request.getEmail() != null) {
            guardian.setEmail(request.getEmail());
        }
        if (request.getName() != null) {
            guardian.setName(request.getName());
        }

        return guardianRepository.save(guardian);
    }
}
