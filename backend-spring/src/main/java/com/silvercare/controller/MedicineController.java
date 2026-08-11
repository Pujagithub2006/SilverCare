package com.silvercare.controller;

import com.silvercare.dto.*;
import com.silvercare.entity.GuardianSuggestion;
import com.silvercare.entity.Medicine;
import com.silvercare.service.ElderlyNotificationService;
import com.silvercare.service.MedicineService;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.List;
import java.util.Map;

@RestController
public class MedicineController {

    @Autowired
    private MedicineService medicineService;

    @Autowired
    private ElderlyNotificationService elderlyNotificationService;

    @PostMapping("/medicine/add")
    public ResponseEntity<ApiResponse<Object>> addMedicine(@RequestBody MedicineAddRequest request) {
        try {
            Medicine medicine = medicineService.addMedicine(request);
            ApiResponse<Object> response = ApiResponse.builder()
                    .status("success")
                    .message("Medicine schedule added successfully")
                    .medicine(medicine)
                    .build();
            return ResponseEntity.ok(response);
        } catch (SecurityException e) {
            return ResponseEntity.status(HttpStatus.FORBIDDEN).body(ApiResponse.error(e.getMessage()));
        } catch (IllegalArgumentException e) {
            return ResponseEntity.status(HttpStatus.NOT_FOUND).body(ApiResponse.error(e.getMessage()));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @PostMapping("/medicines")
    public ResponseEntity<ApiResponse<Object>> addMedicineLegacy(@RequestBody MedicineAddRequest request) {
        return addMedicine(request);
    }

    @RequestMapping(value = "/medicine/delete/{medicineId}", method = {RequestMethod.POST, RequestMethod.DELETE})
    public ResponseEntity<ApiResponse<Object>> deleteMedicine(@PathVariable Long medicineId,
                                                               @RequestBody Map<String, String> payload) {
        try {
            String guardianUsername = payload.get("guardian_username");
            String elderlyId = payload.get("elderly_id");
            medicineService.deleteMedicine(medicineId, guardianUsername, elderlyId);
            return ResponseEntity.ok(ApiResponse.success("Medicine deleted successfully"));
        } catch (SecurityException e) {
            return ResponseEntity.status(HttpStatus.FORBIDDEN).body(ApiResponse.error(e.getMessage()));
        } catch (IllegalArgumentException e) {
            return ResponseEntity.status(HttpStatus.NOT_FOUND).body(ApiResponse.error(e.getMessage()));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping("/medicines/{elderlyId}")
    public ResponseEntity<Map<String, Object>> getMedicines(@PathVariable String elderlyId) {
        List<Medicine> list = medicineService.getAllMedicines(elderlyId);
        return ResponseEntity.ok(Map.of("medicines", list));
    }

    @PostMapping("/medicine/confirm")
    public ResponseEntity<Map<String, String>> confirmMedicineTaken(@RequestBody MedicineConfirmRequest request) {
        try {
            String status = medicineService.confirmMedicineTaken(request);
            return ResponseEntity.ok(Map.of(
                    "message", "Medicine marked as " + status,
                    "status", status
            ));
        } catch (IllegalArgumentException e) {
            return ResponseEntity.status(HttpStatus.NOT_FOUND).body(Map.of("error", e.getMessage()));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(Map.of("error", e.getMessage()));
        }
    }

    @GetMapping("/medicine/suggestions/{elderlyId}")
    public ResponseEntity<Map<String, Object>> getSuggestions(@PathVariable String elderlyId) {
        List<GuardianSuggestion> list = medicineService.getSuggestions(elderlyId);
        return ResponseEntity.ok(Map.of("suggestions", list));
    }

    @PostMapping("/medicine/suggestions/{elderlyId}")
    public ResponseEntity<Map<String, Object>> addSuggestion(@PathVariable String elderlyId,
                                                             @RequestBody SuggestionRequest request) {
        try {
            GuardianSuggestion suggestion = medicineService.addSuggestion(elderlyId, request);
            return ResponseEntity.ok(Map.of(
                    "message", "Suggestion added successfully",
                    "suggestion", suggestion
            ));
        } catch (SecurityException e) {
            return ResponseEntity.status(HttpStatus.FORBIDDEN).body(Map.of("error", e.getMessage()));
        } catch (IllegalArgumentException e) {
            return ResponseEntity.status(HttpStatus.NOT_FOUND).body(Map.of("error", e.getMessage()));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(Map.of("error", e.getMessage()));
        }
    }

    @PostMapping("/medicine-reminder-response")
    public ResponseEntity<ApiResponse<Object>> medicineReminderResponse(@RequestBody MedicineResponseRequest request) {
        try {
            if (request.getElderlyId() == null || request.getMedicineId() == null || request.getResponse() == null) {
                return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(ApiResponse.error("Missing required fields"));
            }
            medicineService.handleMedicineResponse(request.getElderlyId(), request.getMedicineId(), request.getResponse());
            return ResponseEntity.ok(ApiResponse.success("Response '" + request.getResponse() + "' recorded successfully"));
        } catch (Exception e) {
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(ApiResponse.error(e.getMessage()));
        }
    }

    @GetMapping("/active-notifications")
    public ResponseEntity<ApiResponse<Object>> getActiveNotifications() {
        Map<String, Map<String, Object>> map = elderlyNotificationService.getAllActiveNotifications();
        ApiResponse<Object> response = ApiResponse.builder()
                .status("success")
                .notifications(map)
                .build();
        return ResponseEntity.ok(response);
    }
}
