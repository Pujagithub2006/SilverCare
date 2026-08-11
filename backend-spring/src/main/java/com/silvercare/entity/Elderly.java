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

    // Health history fields for ML personalization
    @Column(name = "weight")
    private Double weight;

    @Column(name = "height")
    private Double height;

    @Column(name = "health_conditions", length = 1000)
    private String healthConditions; // JSON array of conditions

    @Column(name = "mobility_level")
    private String mobilityLevel; // independent, cane, walker, wheelchair, bedridden

    @Column(name = "fall_count")
    private Integer fallCount;

    @Column(name = "last_fall_days")
    private Integer lastFallDays;

    @Column(name = "medications", length = 2000)
    private String medications;

    @Column(name = "is_calibrated")
    private Boolean isCalibrated = false;

    @Column(name = "calibrated_at")
    private String calibratedAt;

    private String phone;
    private String location;

    // Alert system contacts
    @Column(name = "neighbor_phone")
    private String neighborPhone;

    @Column(name = "neighbor_name")
    private String neighborName;

    @Column(name = "secondary_guardian_phone")
    private String secondaryGuardianPhone;

    @Column(name = "secondary_guardian_name")
    private String secondaryGuardianName;

    @Column(name = "hospital_phone")
    private String hospitalPhone;

    @Column(name = "hospital_name")
    private String hospitalName;

    @Column(name = "ambulance_phone")
    private String ambulancePhone;

    @Column(name = "emergency_enabled")
    private Boolean emergencyEnabled = true;

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

    @Column(name = "primary_device_mac")
    private String primaryDeviceMac; // MAC address of assigned device for direct connection

    @Column(name = "preferred_language")
    private String preferredLanguage = "en";

    private Double latitude;
    private Double longitude;

    @Column(name = "geofence_enabled")
    private Boolean geofenceEnabled = true;

    @Column(name = "geofence_radius_km")
    private Double geofenceRadiusKm = 5.0;

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

    public Double getWeight() { return weight; }
    public void setWeight(Double weight) { this.weight = weight; }

    public Double getHeight() { return height; }
    public void setHeight(Double height) { this.height = height; }

    public String getHealthConditions() { return healthConditions; }
    public void setHealthConditions(String healthConditions) { this.healthConditions = healthConditions; }

    public String getMobilityLevel() { return mobilityLevel; }
    public void setMobilityLevel(String mobilityLevel) { this.mobilityLevel = mobilityLevel; }

    public Integer getFallCount() { return fallCount; }
    public void setFallCount(Integer fallCount) { this.fallCount = fallCount; }

    public Integer getLastFallDays() { return lastFallDays; }
    public void setLastFallDays(Integer lastFallDays) { this.lastFallDays = lastFallDays; }

    public String getMedications() { return medications; }
    public void setMedications(String medications) { this.medications = medications; }

    public Boolean getIsCalibrated() { return isCalibrated; }
    public void setIsCalibrated(Boolean isCalibrated) { this.isCalibrated = isCalibrated; }

    public String getCalibratedAt() { return calibratedAt; }
    public void setCalibratedAt(String calibratedAt) { this.calibratedAt = calibratedAt; }

    public String getPhone() { return phone; }
    public void setPhone(String phone) { this.phone = phone; }

    public String getLocation() { return location; }
    public void setLocation(String location) { this.location = location; }

    public String getGuardianUsername() { return guardianUsername; }
    public void setGuardianUsername(String guardianUsername) { this.guardianUsername = guardianUsername; }

    public String getNeighbourName() { return neighbourName != null ? neighbourName : neighborName; }
    public void setNeighbourName(String neighbourName) { 
        this.neighbourName = neighbourName; 
        if (this.neighborName == null) this.neighborName = neighbourName;
    }

    public String getNeighbourPhone() { return neighbourPhone != null ? neighbourPhone : neighborPhone; }
    public void setNeighbourPhone(String neighbourPhone) { 
        this.neighbourPhone = neighbourPhone; 
        if (this.neighborPhone == null) this.neighborPhone = neighbourPhone;
    }

    public String getNeighborPhone() { return neighborPhone != null ? neighborPhone : neighbourPhone; }
    public void setNeighborPhone(String neighborPhone) { 
        this.neighborPhone = neighborPhone; 
        this.neighbourPhone = neighborPhone;
    }

    public String getNeighborName() { return neighborName != null ? neighborName : neighbourName; }
    public void setNeighborName(String neighborName) { 
        this.neighborName = neighborName; 
        this.neighbourName = neighborName;
    }

    public String getSecondaryGuardianPhone() { return secondaryGuardianPhone; }
    public void setSecondaryGuardianPhone(String secondaryGuardianPhone) { this.secondaryGuardianPhone = secondaryGuardianPhone; }

    public String getSecondaryGuardianName() { return secondaryGuardianName; }
    public void setSecondaryGuardianName(String secondaryGuardianName) { this.secondaryGuardianName = secondaryGuardianName; }

    public String getHospitalPhone() { return hospitalPhone; }
    public void setHospitalPhone(String hospitalPhone) { this.hospitalPhone = hospitalPhone; }

    public String getHospitalName() { return hospitalName; }
    public void setHospitalName(String hospitalName) { this.hospitalName = hospitalName; }

    public String getAmbulancePhone() { return ambulancePhone; }
    public void setAmbulancePhone(String ambulancePhone) { this.ambulancePhone = ambulancePhone; }

    public Boolean getEmergencyEnabled() { return emergencyEnabled; }
    public void setEmergencyEnabled(Boolean emergencyEnabled) { this.emergencyEnabled = emergencyEnabled; }

    public String getPrimaryBeltType() { return primaryBeltType; }
    public void setPrimaryBeltType(String primaryBeltType) { this.primaryBeltType = primaryBeltType; }

    public String getPrimaryDeviceId() { return primaryDeviceId; }
    public void setPrimaryDeviceId(String primaryDeviceId) { this.primaryDeviceId = primaryDeviceId; }

    public String getPrimaryDeviceMac() { return primaryDeviceMac; }
    public void setPrimaryDeviceMac(String primaryDeviceMac) { this.primaryDeviceMac = primaryDeviceMac; }

    public String getPreferredLanguage() { return preferredLanguage; }
    public void setPreferredLanguage(String preferredLanguage) { this.preferredLanguage = preferredLanguage; }

    public Double getLatitude() { return latitude; }
    public void setLatitude(Double latitude) { this.latitude = latitude; }

    public Double getLongitude() { return longitude; }
    public void setLongitude(Double longitude) { this.longitude = longitude; }

    public Boolean getGeofenceEnabled() { return geofenceEnabled; }
    public void setGeofenceEnabled(Boolean geofenceEnabled) { this.geofenceEnabled = geofenceEnabled; }

    public Double getGeofenceRadiusKm() { return geofenceRadiusKm; }
    public void setGeofenceRadiusKm(Double geofenceRadiusKm) { this.geofenceRadiusKm = geofenceRadiusKm; }

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
        private Boolean geofenceEnabled = true;
        private Double geofenceRadiusKm = 5.0;
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
        public ElderlyBuilder geofenceEnabled(Boolean geofenceEnabled) { this.geofenceEnabled = geofenceEnabled; return this; }
        public ElderlyBuilder geofenceRadiusKm(Double geofenceRadiusKm) { this.geofenceRadiusKm = geofenceRadiusKm; return this; }
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
            e.setGeofenceEnabled(geofenceEnabled != null ? geofenceEnabled : true);
            e.setGeofenceRadiusKm(geofenceRadiusKm != null ? geofenceRadiusKm : 5.0);
            return e;
        }
    }
}
