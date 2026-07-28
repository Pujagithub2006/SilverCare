package com.silvercare.controller;

import com.silvercare.dto.ApiResponse;
import org.springframework.core.io.FileSystemResource;
import org.springframework.core.io.Resource;
import org.springframework.http.HttpHeaders;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.io.File;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.LinkedHashMap;
import java.util.Map;

@RestController
public class HomeController {

    @GetMapping("/")
    public ResponseEntity<ApiResponse<Object>> home() {
        Map<String, Object> endpoints = new LinkedHashMap<>();

        Map<String, String> guardianMap = Map.of(
                "register", "POST /guardian-register",
                "login", "POST /guardian-login",
                "info", "GET /guardian-info/<username>",
                "update", "POST /guardian-update",
                "elderly", "GET /guardian-elderly/<username>"
        );
        endpoints.put("guardian", guardianMap);

        Map<String, String> elderlyMap = Map.of(
                "register", "POST /elderly-register",
                "info", "GET /elderly-info/<elderly_id>",
                "update", "POST /elderly-update",
                "by_guardian", "GET /guardian-elderly/<username>"
        );
        endpoints.put("elderly", elderlyMap);

        Map<String, String> fallMap = Map.of(
                "detect", "POST /detect-fall",
                "status", "GET /fall-status",
                "clear", "POST /clear-fall",
                "notify_fall", "POST /notify-guardian-fall",
                "notify_no_response", "POST /notify-guardian-no-response",
                "notify_safe", "POST /notify-guardian-safe"
        );
        endpoints.put("fall_detection", fallMap);

        endpoints.put("chatbot", Map.of("chat", "POST /chat"));

        Map<String, String> medMap = Map.of(
                "add_medicine", "POST /medicine/add",
                "get_medicines", "GET /medicines/<elderly_id>",
                "confirm_medicine", "POST /medicine/confirm",
                "manage_suggestions", "GET/POST /medicine/suggestions/<elderly_id>"
        );
        endpoints.put("medicine_management", medMap);

        ApiResponse<Object> response = ApiResponse.builder()
                .status("success")
                .message("SilverCare Main Backend (Java Spring Boot)")
                .endpoints(endpoints)
                .build();
        return ResponseEntity.ok(response);
    }

    @GetMapping("/manifest.json")
    public ResponseEntity<Resource> serveManifest() {
        Path manifestPath = Paths.get("..", "frontend", "manifest.json").toAbsolutePath().normalize();
        File file = manifestPath.toFile();
        if (!file.exists()) {
            manifestPath = Paths.get("frontend", "manifest.json").toAbsolutePath().normalize();
            file = manifestPath.toFile();
        }

        if (file.exists()) {
            return ResponseEntity.ok()
                    .contentType(MediaType.APPLICATION_JSON)
                    .body(new FileSystemResource(file));
        }
        return ResponseEntity.status(HttpStatus.NOT_FOUND).build();
    }

    @GetMapping("/frontend/{filename:.+}")
    public ResponseEntity<Resource> serveFrontendFile(@PathVariable String filename) {
        Path filePath = Paths.get("..", "frontend", filename).toAbsolutePath().normalize();
        File file = filePath.toFile();
        if (!file.exists()) {
            filePath = Paths.get("frontend", filename).toAbsolutePath().normalize();
            file = filePath.toFile();
        }

        if (file.exists()) {
            return ResponseEntity.ok()
                    .header(HttpHeaders.CONTENT_DISPOSITION, "inline; filename=\"" + file.getName() + "\"")
                    .body(new FileSystemResource(file));
        }
        return ResponseEntity.status(HttpStatus.NOT_FOUND).build();
    }
}
