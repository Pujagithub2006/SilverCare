package com.silvercare.controller;

import com.silvercare.dto.AlertAcknowledgeRequest;
import com.silvercare.dto.ApiResponse;
import com.silvercare.dto.ElderlyVoiceMessageRequest;
import com.silvercare.entity.AlertRecord;
import com.silvercare.repository.AlertRecordRepository;
import com.silvercare.service.FallDetectionService;
import com.silvercare.service.FirebaseEncryptionService;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.List;
import java.util.Map;

@RestController
@RequestMapping("/api/alerts")
public class AlertController {

    @Autowired
    private FallDetectionService fallDetectionService;

    @Autowired
    private AlertRecordRepository alertRecordRepository;

    @Autowired
    private FirebaseEncryptionService firebaseEncryptionService;

    @Autowired
    private com.silvercare.service.SensorDataService sensorDataService;

    @PostMapping("/acknowledge")
    public ResponseEntity<ApiResponse<Object>> acknowledgeAlert(@RequestBody AlertAcknowledgeRequest request) {
        try {
            String alertId = request.getAlertId();
            String guardianUser = request.getGuardianUsername() != null ? request.getGuardianUsername() : "guardian";
            String msg = request.getResponseMessage() != null ? request.getResponseMessage() : "I am Fine";

            AlertRecord alert = fallDetectionService.acknowledgeAlert(alertId, guardianUser, msg);
            ApiResponse<Object> response = ApiResponse.builder()
                    .status("success")
                    .message("Alert acknowledged by guardian. 'I am fine' recorded successfully!")
                    .data(alert)
                    .build();
            return ResponseEntity.ok(response);
        } catch (IllegalArgumentException e) {
            return ResponseEntity.status(HttpStatus.NOT_FOUND).body(ApiResponse.error(e.getMessage()));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping("/active/{elderlyId}")
    public ResponseEntity<ApiResponse<Object>> getActiveAlertsForElderly(@PathVariable String elderlyId) {
        List<AlertRecord> pendingAlerts = alertRecordRepository.findByElderlyIdAndStatus(elderlyId, "PENDING_ACK");
        ApiResponse<Object> response = ApiResponse.builder()
                .status("success")
                .data(pendingAlerts)
                .count(pendingAlerts.size())
                .build();
        return ResponseEntity.ok(response);
    }

    @GetMapping("/history/{elderlyId}")
    public ResponseEntity<ApiResponse<Object>> getAlertHistoryForElderly(@PathVariable String elderlyId) {
        List<AlertRecord> history = alertRecordRepository.findByElderlyIdOrderByCreatedAtDesc(elderlyId);
        ApiResponse<Object> response = ApiResponse.builder()
                .status("success")
                .data(history)
                .count(history.size())
                .build();
        return ResponseEntity.ok(response);
    }

    @PostMapping("/voice-message")
    public ResponseEntity<ApiResponse<Object>> receiveElderlyVoiceMessage(@RequestBody ElderlyVoiceMessageRequest request) {
        try {
            String elderlyId = request.getElderlyId() != null ? request.getElderlyId() : "senior_user";
            String audioData = request.getAudioData() != null ? request.getAudioData() : "HELP! I need assistance!";

            // Attach to active alert or create prefall/fall record
            List<AlertRecord> active = alertRecordRepository.findByElderlyIdAndStatus(elderlyId, "PENDING_ACK");
            AlertRecord alert;
            if (!active.isEmpty()) {
                alert = active.get(0);
                alert.setAudioMessage(audioData);
            } else {
                double[] lastCoords = sensorDataService.getLastKnownCoordinates(request.getDeviceId());
                Double lat = lastCoords != null ? lastCoords[0] : null;
                Double lng = lastCoords != null ? lastCoords[1] : null;
                alert = new AlertRecord("VOICE_" + System.currentTimeMillis(), elderlyId, request.getDeviceId(), "Microphone", "VOICE_TRIGGER", "PENDING_ACK", audioData, lat, lng);
            }
            alertRecordRepository.save(alert);

            ApiResponse<Object> response = ApiResponse.builder()
                    .status("success")
                    .message("Senior citizen microphone message received and saved successfully")
                    .data(alert)
                    .build();
            return ResponseEntity.ok(response);
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping("/firebase-encrypted-storage")
    public ResponseEntity<ApiResponse<Object>> getFirebaseEncryptedRecords() {
        Map<String, String> records = firebaseEncryptionService.getAllEncryptedFirebaseRecords();
        ApiResponse<Object> response = ApiResponse.builder()
                .status("success")
                .message("Encrypted Firebase records store")
                .data(records)
                .count(records.size())
                .build();
        return ResponseEntity.ok(response);
    }
}
