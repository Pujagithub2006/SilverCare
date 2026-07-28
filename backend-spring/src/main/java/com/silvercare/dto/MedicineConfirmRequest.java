package com.silvercare.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public class MedicineConfirmRequest {
    @JsonProperty("guardian_username")
    private String guardianUsername;

    @JsonProperty("elderly_id")
    private String elderlyId;

    @JsonProperty("medicine_id")
    private Long medicineId;

    @JsonProperty("time_taken")
    private String timeTaken;

    private Boolean taken;

    public MedicineConfirmRequest() {}

    public MedicineConfirmRequest(String guardianUsername, String elderlyId, Long medicineId, String timeTaken, Boolean taken) {
        this.guardianUsername = guardianUsername;
        this.elderlyId = elderlyId;
        this.medicineId = medicineId;
        this.timeTaken = timeTaken;
        this.taken = taken;
    }

    public String getGuardianUsername() { return guardianUsername; }
    public void setGuardianUsername(String guardianUsername) { this.guardianUsername = guardianUsername; }

    public String getElderlyId() { return elderlyId; }
    public void setElderlyId(String elderlyId) { this.elderlyId = elderlyId; }

    public Long getMedicineId() { return medicineId; }
    public void setMedicineId(Long medicineId) { this.medicineId = medicineId; }

    public String getTimeTaken() { return timeTaken; }
    public void setTimeTaken(String timeTaken) { this.timeTaken = timeTaken; }

    public Boolean getTaken() { return taken; }
    public void setTaken(Boolean taken) { this.taken = taken; }
}
