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

    @Column(name = "guardian_username", nullable = false)
    private String guardianUsername;

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
        private String createdAt;

        public ElderlyBuilder elderlyId(String elderlyId) { this.elderlyId = elderlyId; return this; }
        public ElderlyBuilder name(String name) { this.name = name; return this; }
        public ElderlyBuilder age(Integer age) { this.age = age; return this; }
        public ElderlyBuilder medicalHistory(String medicalHistory) { this.medicalHistory = medicalHistory; return this; }
        public ElderlyBuilder phone(String phone) { this.phone = phone; return this; }
        public ElderlyBuilder location(String location) { this.location = location; return this; }
        public ElderlyBuilder guardianUsername(String guardianUsername) { this.guardianUsername = guardianUsername; return this; }
        public ElderlyBuilder createdAt(String createdAt) { this.createdAt = createdAt; return this; }

        public Elderly build() {
            return new Elderly(elderlyId, name, age, medicalHistory, phone, location, guardianUsername, createdAt);
        }
    }
}
