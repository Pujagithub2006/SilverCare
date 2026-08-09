package com.silvercare.entity;

import jakarta.persistence.*;
import lombok.AllArgsConstructor;
import lombok.Builder;
import lombok.Data;
import lombok.NoArgsConstructor;

import java.time.LocalDateTime;

@Entity
@Table(name = "location_history")
@Data
@Builder
@NoArgsConstructor
@AllArgsConstructor
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
    
    @PrePersist
    protected void onCreate() {
        createdAt = LocalDateTime.now();
        if (timestamp == null) {
            timestamp = LocalDateTime.now();
        }
    }
}
