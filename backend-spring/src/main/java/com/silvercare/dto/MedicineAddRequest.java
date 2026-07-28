package com.silvercare.dto;

import com.fasterxml.jackson.annotation.JsonProperty;
import java.util.List;

public class MedicineAddRequest {
    @JsonProperty("guardian_username")
    private String guardianUsername;

    @JsonProperty("elderly_id")
    private String elderlyId;

    @JsonProperty("medicine_name")
    private String medicineName;

    private String dosage;
    private List<String> times;
    private String instructions;

    @JsonProperty("start_date")
    private String startDate;

    @JsonProperty("end_date")
    private String endDate;

    public MedicineAddRequest() {}

    public MedicineAddRequest(String guardianUsername, String elderlyId, String medicineName, String dosage, List<String> times, String instructions, String startDate, String endDate) {
        this.guardianUsername = guardianUsername;
        this.elderlyId = elderlyId;
        this.medicineName = medicineName;
        this.dosage = dosage;
        this.times = times;
        this.instructions = instructions;
        this.startDate = startDate;
        this.endDate = endDate;
    }

    public String getGuardianUsername() { return guardianUsername; }
    public void setGuardianUsername(String guardianUsername) { this.guardianUsername = guardianUsername; }

    public String getElderlyId() { return elderlyId; }
    public void setElderlyId(String elderlyId) { this.elderlyId = elderlyId; }

    public String getMedicineName() { return medicineName; }
    public void setMedicineName(String medicineName) { this.medicineName = medicineName; }

    public String getDosage() { return dosage; }
    public void setDosage(String dosage) { this.dosage = dosage; }

    public List<String> getTimes() { return times; }
    public void setTimes(List<String> times) { this.times = times; }

    public String getInstructions() { return instructions; }
    public void setInstructions(String instructions) { this.instructions = instructions; }

    public String getStartDate() { return startDate; }
    public void setStartDate(String startDate) { this.startDate = startDate; }

    public String getEndDate() { return endDate; }
    public void setEndDate(String endDate) { this.endDate = endDate; }
}
