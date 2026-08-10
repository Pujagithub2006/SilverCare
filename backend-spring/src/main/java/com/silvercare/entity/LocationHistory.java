package com.silvercare.entity;

import jakarta.persistence.*;
import java.time.LocalDateTime;

@Entity
@Table(name = "location_history")
public class LocationHistory {
    
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;
    
    @Column(name = "elderly_id", nullable = false)
    private String elderlyId;
    
    @Column(name = "latitude", nullable = false)
    private Double latitude;
    
    @Column(name = "longitude", nullable = false)
    private Double longitude;
    
    @Column(name = "timestamp", nullable = false)
    private LocalDateTime timestamp;
    
    @Column(name = "accuracy")
    private Double accuracy; // GPS accuracy in meters
    
    @Column(name = "altitude")
    private Double altitude;
    
    @Column(name = "speed")
    private Double speed; // Speed in m/s
    
    @Column(name = "heading")
    private Double heading; // Direction in degrees
    
    @Column(name = "source")
    private String source; // "GPS", "WIFI", "CELL", "MANUAL"
    
    @Column(name = "is_unusual")
    private Boolean isUnusual;
    
    @Column(name = "alert_type")
    private String alertType; // "UNUSUAL_LOCATION", "OUT_OF_RANGE", "NO_INTERNET"
    
    @Column(name = "device_id")
    private String deviceId;
    
    @Column(name = "created_at")
    private LocalDateTime createdAt;
    
    public LocationHistory() {}

    public LocationHistory(Long id, String elderlyId, Double latitude, Double longitude, LocalDateTime timestamp, Double accuracy, Double altitude, Double speed, Double heading, String source, Boolean isUnusual, String alertType, String deviceId, LocalDateTime createdAt) {
        this.id = id;
        this.elderlyId = elderlyId;
        this.latitude = latitude;
        this.longitude = longitude;
        this.timestamp = timestamp;
        this.accuracy = accuracy;
        this.altitude = altitude;
        this.speed = speed;
        this.heading = heading;
        this.source = source;
        this.isUnusual = isUnusual;
        this.alertType = alertType;
        this.deviceId = deviceId;
        this.createdAt = createdAt;
    }

    @PrePersist
    protected void onCreate() {
        createdAt = LocalDateTime.now();
        if (timestamp == null) {
            timestamp = LocalDateTime.now();
        }
    }

    // Getters and Setters
    public Long getId() { return id; }
    public void setId(Long id) { this.id = id; }

    public String getElderlyId() { return elderlyId; }
    public void setElderlyId(String elderlyId) { this.elderlyId = elderlyId; }

    public Double getLatitude() { return latitude; }
    public void setLatitude(Double latitude) { this.latitude = latitude; }

    public Double getLongitude() { return longitude; }
    public void setLongitude(Double longitude) { this.longitude = longitude; }

    public LocalDateTime getTimestamp() { return timestamp; }
    public void setTimestamp(LocalDateTime timestamp) { this.timestamp = timestamp; }

    public Double getAccuracy() { return accuracy; }
    public void setAccuracy(Double accuracy) { this.accuracy = accuracy; }

    public Double getAltitude() { return altitude; }
    public void setAltitude(Double altitude) { this.altitude = altitude; }

    public Double getSpeed() { return speed; }
    public void setSpeed(Double speed) { this.speed = speed; }

    public Double getHeading() { return heading; }
    public void setHeading(Double heading) { this.heading = heading; }

    public String getSource() { return source; }
    public void setSource(String source) { this.source = source; }

    public Boolean getIsUnusual() { return isUnusual; }
    public void setIsUnusual(Boolean isUnusual) { this.isUnusual = isUnusual; }

    public String getAlertType() { return alertType; }
    public void setAlertType(String alertType) { this.alertType = alertType; }

    public String getDeviceId() { return deviceId; }
    public void setDeviceId(String deviceId) { this.deviceId = deviceId; }

    public LocalDateTime getCreatedAt() { return createdAt; }
    public void setCreatedAt(LocalDateTime createdAt) { this.createdAt = createdAt; }

    public static LocationHistoryBuilder builder() {
        return new LocationHistoryBuilder();
    }

    public static class LocationHistoryBuilder {
        private Long id;
        private String elderlyId;
        private Double latitude;
        private Double longitude;
        private LocalDateTime timestamp;
        private Double accuracy;
        private Double altitude;
        private Double speed;
        private Double heading;
        private String source;
        private Boolean isUnusual;
        private String alertType;
        private String deviceId;
        private LocalDateTime createdAt;

        public LocationHistoryBuilder id(Long id) { this.id = id; return this; }
        public LocationHistoryBuilder elderlyId(String elderlyId) { this.elderlyId = elderlyId; return this; }
        public LocationHistoryBuilder latitude(Double latitude) { this.latitude = latitude; return this; }
        public LocationHistoryBuilder longitude(Double longitude) { this.longitude = longitude; return this; }
        public LocationHistoryBuilder timestamp(LocalDateTime timestamp) { this.timestamp = timestamp; return this; }
        public LocationHistoryBuilder accuracy(Double accuracy) { this.accuracy = accuracy; return this; }
        public LocationHistoryBuilder altitude(Double altitude) { this.altitude = altitude; return this; }
        public LocationHistoryBuilder speed(Double speed) { this.speed = speed; return this; }
        public LocationHistoryBuilder heading(Double heading) { this.heading = heading; return this; }
        public LocationHistoryBuilder source(String source) { this.source = source; return this; }
        public LocationHistoryBuilder isUnusual(Boolean isUnusual) { this.isUnusual = isUnusual; return this; }
        public LocationHistoryBuilder alertType(String alertType) { this.alertType = alertType; return this; }
        public LocationHistoryBuilder deviceId(String deviceId) { this.deviceId = deviceId; return this; }
        public LocationHistoryBuilder createdAt(LocalDateTime createdAt) { this.createdAt = createdAt; return this; }

        public LocationHistory build() {
            return new LocationHistory(id, elderlyId, latitude, longitude, timestamp, accuracy, altitude, speed, heading, source, isUnusual, alertType, deviceId, createdAt);
        }
    }
}
