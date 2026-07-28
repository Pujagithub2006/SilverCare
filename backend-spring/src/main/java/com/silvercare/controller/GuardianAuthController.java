package com.silvercare.controller;

import com.silvercare.dto.*;
import com.silvercare.entity.Guardian;
import com.silvercare.service.GuardianAuthService;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.LinkedHashMap;
import java.util.Map;

@RestController
public class GuardianAuthController {

    @Autowired
    private GuardianAuthService guardianAuthService;

    @PostMapping("/guardian-register")
    public ResponseEntity<ApiResponse<Object>> registerGuardian(@RequestBody GuardianRegisterRequest request) {
        try {
            Guardian guardian = guardianAuthService.registerGuardian(request);
            ApiResponse<Object> response = ApiResponse.builder()
                    .status("success")
                    .message("Guardian registered successfully")
                    .username(guardian.getUsername())
                    .build();
            return ResponseEntity.status(HttpStatus.CREATED).body(response);
        } catch (IllegalArgumentException e) {
            int code = e.getMessage().contains("already exists") ? 409 : 400;
            return ResponseEntity.status(code).body(ApiResponse.error(e.getMessage()));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @PostMapping("/guardian-login")
    public ResponseEntity<ApiResponse<Object>> loginGuardian(@RequestBody GuardianLoginRequest request) {
        try {
            Guardian guardian = guardianAuthService.loginGuardian(request);
            ApiResponse<Object> response = ApiResponse.builder()
                    .status("success")
                    .message("Login successful")
                    .username(guardian.getUsername())
                    .name(guardian.getName())
                    .phone(guardian.getPhone())
                    .email(guardian.getEmail())
                    .elderly_linked(guardian.getElderlyLinked())
                    .build();
            return ResponseEntity.ok(response);
        } catch (SecurityException e) {
            return ResponseEntity.status(HttpStatus.UNAUTHORIZED).body(ApiResponse.error(e.getMessage()));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping("/guardian-info/{username}")
    public ResponseEntity<ApiResponse<Object>> getGuardianInfo(@PathVariable String username) {
        Guardian guardian = guardianAuthService.getGuardian(username);
        if (guardian == null) {
            return ResponseEntity.status(HttpStatus.NOT_FOUND).body(ApiResponse.error("Guardian not found"));
        }

        Map<String, Object> data = new LinkedHashMap<>();
        data.put("name", guardian.getName());
        data.put("username", guardian.getUsername());
        data.put("phone", guardian.getPhone());
        data.put("email", guardian.getEmail());
        data.put("elderly_linked", guardian.getElderlyLinked());
        data.put("created_at", guardian.getCreatedAt());

        ApiResponse<Object> response = ApiResponse.builder()
                .status("success")
                .data(data)
                .build();
        return ResponseEntity.ok(response);
    }

    @PostMapping("/guardian-update")
    public ResponseEntity<ApiResponse<Object>> updateGuardian(@RequestBody GuardianUpdateRequest request) {
        try {
            guardianAuthService.updateGuardian(request);
            return ResponseEntity.ok(ApiResponse.success("Guardian information updated"));
        } catch (IllegalArgumentException e) {
            return ResponseEntity.status(HttpStatus.NOT_FOUND).body(ApiResponse.error(e.getMessage()));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }
}
