package com.silvercare.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public class MedicineResponseRequest {
    @JsonProperty("elderly_id")
    private String elderlyId;

    @JsonProperty("medicine_id")
    private Long medicineId;

    private String response;

    public MedicineResponseRequest() {}

    public MedicineResponseRequest(String elderlyId, Long medicineId, String response) {
        this.elderlyId = elderlyId;
        this.medicineId = medicineId;
        this.response = response;
    }

    public String getElderlyId() { return elderlyId; }
    public void setElderlyId(String elderlyId) { this.elderlyId = elderlyId; }

    public Long getMedicineId() { return medicineId; }
    public void setMedicineId(Long medicineId) { this.medicineId = medicineId; }

    public String getResponse() { return response; }
    public void setResponse(String response) { this.response = response; }
}
