package com.silvercare.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public class ElderlyUpdateRequest {
    @JsonProperty("elderly_id")
    private String elderlyId;

    @JsonProperty("medical_history")
    private String medicalHistory;

    private String phone;
    private String location;
    private Integer age;

    public ElderlyUpdateRequest() {}

    public ElderlyUpdateRequest(String elderlyId, String medicalHistory, String phone, String location, Integer age) {
        this.elderlyId = elderlyId;
        this.medicalHistory = medicalHistory;
        this.phone = phone;
        this.location = location;
        this.age = age;
    }

    public String getElderlyId() { return elderlyId; }
    public void setElderlyId(String elderlyId) { this.elderlyId = elderlyId; }

    public String getMedicalHistory() { return medicalHistory; }
    public void setMedicalHistory(String medicalHistory) { this.medicalHistory = medicalHistory; }

    public String getPhone() { return phone; }
    public void setPhone(String phone) { this.phone = phone; }

    public String getLocation() { return location; }
    public void setLocation(String location) { this.location = location; }

    public Integer getAge() { return age; }
    public void setAge(Integer age) { this.age = age; }
}
