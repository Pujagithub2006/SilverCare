package com.silvercare.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public class ElderlyRegisterRequest {
    private String name;
    private Integer age;

    @JsonProperty("medical_history")
    private String medicalHistory;

    private String phone;
    private String location;

    @JsonProperty("guardian_username")
    private String guardianUsername;

    @JsonProperty("guardian_password")
    private String guardianPassword;

    public ElderlyRegisterRequest() {}

    public ElderlyRegisterRequest(String name, Integer age, String medicalHistory, String phone, String location, String guardianUsername, String guardianPassword) {
        this.name = name;
        this.age = age;
        this.medicalHistory = medicalHistory;
        this.phone = phone;
        this.location = location;
        this.guardianUsername = guardianUsername;
        this.guardianPassword = guardianPassword;
    }

    public String getName() { return name; }
    public void setName(String name) { this.name = name; }

    public Integer getAge() { return age; }
    public void setAge(Integer age) { this.age = age; }

    public String getMedicalHistory() { return medicalHistory; }
    public void setMedicalHistory(String medicalHistory) { this.medicalHistory = medicalHistory; }

    public String getPhone() { return phone; }
    public void setPhone(String phone) { this.phone = phone; }

    public String getLocation() { return location; }
    public void setLocation(String location) { this.location = location; }

    public String getGuardianUsername() { return guardianUsername; }
    public void setGuardianUsername(String guardianUsername) { this.guardianUsername = guardianUsername; }

    public String getGuardianPassword() { return guardianPassword; }
    public void setGuardianPassword(String guardianPassword) { this.guardianPassword = guardianPassword; }
}
