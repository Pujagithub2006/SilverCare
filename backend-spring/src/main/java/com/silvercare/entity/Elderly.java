package com.silvercare.entity;

import jakarta.persistence.*;
import java.time.LocalDateTime;

@Entity
@Table(name = "elderly_members")
public class Elderly {

    @Id
    @Column(name = "elderly_id", nullable = false, unique = true)
    private String elderlyId;

    private String name;
    private Integer age;

    @Column(name = "medical_history", length = 2000)
    private String medicalHistory;

    private String phone;
    private String location;

    @Column(name = "guardian_username")
    private String guardianUsername; // Kept for legacy compatibility / primary contact

    @Column(name = "neighbour_name")
    private String neighbourName;

    @Column(name = "neighbour_phone")
    private String neighbourPhone;

    @Column(name = "primary_belt_type")
    private String primaryBeltType; // Waist Belt / Wrist Belt

    @Column(name = "primary_device_id")
    private String primaryDeviceId; // e.g. vois_belt / esp32c3_wrist

    @Column(name = "preferred_language")
    private String preferredLanguage = "en";

    private Double latitude;
    private Double longitude;

    @Column(name = "created_at")
    private String createdAt;

    public Elderly() {}

    public Elderly(String elderlyId, String name, Integer age, String medicalHistory, String phone, String location, String guardianUsername, String createdAt) {
        this.elderlyId = elderlyId;
        this.name = name;
        this.age = age;
        this.medicalHistory = medicalHistory;
        this.phone = phone;
        this.location = location;
        this.guardianUsername = guardianUsername;
        this.createdAt = createdAt;
    }

    @PrePersist
    public void prePersist() {
        if (createdAt == null) {
            createdAt = LocalDateTime.now().toString();
        }
        if (location == null) {
            location = "Home";
        }
        if (primaryBeltType == null) {
            primaryBeltType = "Waist Belt";
        }
        if (preferredLanguage == null || preferredLanguage.trim().isEmpty()) {
            preferredLanguage = "en";
        }
    }

    public String getElderlyId() { return elderlyId; }
    public void setElderlyId(String elderlyId) { this.elderlyId = elderlyId; }

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

    public String getNeighbourName() { return neighbourName; }
    public void setNeighbourName(String neighbourName) { this.neighbourName = neighbourName; }

    public String getNeighbourPhone() { return neighbourPhone; }
    public void setNeighbourPhone(String neighbourPhone) { this.neighbourPhone = neighbourPhone; }

    public String getPrimaryBeltType() { return primaryBeltType; }
    public void setPrimaryBeltType(String primaryBeltType) { this.primaryBeltType = primaryBeltType; }

    public String getPrimaryDeviceId() { return primaryDeviceId; }
    public void setPrimaryDeviceId(String primaryDeviceId) { this.primaryDeviceId = primaryDeviceId; }

    public String getPreferredLanguage() { return preferredLanguage; }
    public void setPreferredLanguage(String preferredLanguage) { this.preferredLanguage = preferredLanguage; }

    public Double getLatitude() { return latitude; }
    public void setLatitude(Double latitude) { this.latitude = latitude; }

    public Double getLongitude() { return longitude; }
    public void setLongitude(Double longitude) { this.longitude = longitude; }

    public String getCreatedAt() { return createdAt; }
    public void setCreatedAt(String createdAt) { this.createdAt = createdAt; }

    public static ElderlyBuilder builder() { return new ElderlyBuilder(); }

    public static class ElderlyBuilder {
        private String elderlyId;
        private String name;
        private Integer age;
        private String medicalHistory;
        private String phone;
        private String location;
        private String guardianUsername;
        private String neighbourName;
        private String neighbourPhone;
        private String primaryBeltType;
        private String primaryDeviceId;
        private String preferredLanguage = "en";
        private Double latitude;
        private Double longitude;
        private String createdAt;

        public ElderlyBuilder elderlyId(String elderlyId) { this.elderlyId = elderlyId; return this; }
        public ElderlyBuilder name(String name) { this.name = name; return this; }
        public ElderlyBuilder age(Integer age) { this.age = age; return this; }
        public ElderlyBuilder medicalHistory(String medicalHistory) { this.medicalHistory = medicalHistory; return this; }
        public ElderlyBuilder phone(String phone) { this.phone = phone; return this; }
        public ElderlyBuilder location(String location) { this.location = location; return this; }
        public ElderlyBuilder guardianUsername(String guardianUsername) { this.guardianUsername = guardianUsername; return this; }
        public ElderlyBuilder neighbourName(String neighbourName) { this.neighbourName = neighbourName; return this; }
        public ElderlyBuilder neighbourPhone(String neighbourPhone) { this.neighbourPhone = neighbourPhone; return this; }
        public ElderlyBuilder primaryBeltType(String primaryBeltType) { this.primaryBeltType = primaryBeltType; return this; }
        public ElderlyBuilder primaryDeviceId(String primaryDeviceId) { this.primaryDeviceId = primaryDeviceId; return this; }
        public ElderlyBuilder preferredLanguage(String preferredLanguage) { this.preferredLanguage = preferredLanguage; return this; }
        public ElderlyBuilder latitude(Double latitude) { this.latitude = latitude; return this; }
        public ElderlyBuilder longitude(Double longitude) { this.longitude = longitude; return this; }
        public ElderlyBuilder createdAt(String createdAt) { this.createdAt = createdAt; return this; }

        public Elderly build() {
            Elderly e = new Elderly(elderlyId, name, age, medicalHistory, phone, location, guardianUsername, createdAt);
            e.setNeighbourName(neighbourName);
            e.setNeighbourPhone(neighbourPhone);
            e.setPrimaryBeltType(primaryBeltType);
            e.setPrimaryDeviceId(primaryDeviceId);
            e.setPreferredLanguage(preferredLanguage != null ? preferredLanguage : "en");
            e.setLatitude(latitude);
            e.setLongitude(longitude);
            return e;
        }
    }
}
