package com.silvercare.entity;

import jakarta.persistence.*;
import java.time.LocalDateTime;

@Entity
@Table(name = "alert_records")
public class AlertRecord {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(name = "alert_id", nullable = false, unique = true)
    private String alertId;

    @Column(name = "elderly_id", nullable = false)
    private String elderlyId;

    @Column(name = "device_id")
    private String deviceId;

    @Column(name = "belt_type")
    private String beltType; // Waist Belt / Wrist Belt

    @Column(name = "alert_type", nullable = false)
    private String alertType; // PREFALL / FALL_DETECTED

    @Column(name = "status", nullable = false)
    private String status; // PENDING_ACK, ACKNOWLEDGED, ESCALATED_TO_NEIGHBOUR

    @Column(name = "audio_message", length = 5000)
    private String audioMessage; // Senior citizen mic voice message payload / URL

    @Column(name = "acknowledged_by")
    private String acknowledgedBy;

    @Column(name = "acknowledged_at")
    private String acknowledgedAt;

    private Double latitude;
    private Double longitude;

    @Column(name = "created_at")
    private String createdAt;

    public AlertRecord() {}

    public AlertRecord(String alertId, String elderlyId, String deviceId, String beltType, String alertType, String status, String audioMessage, Double latitude, Double longitude) {
        this.alertId = alertId;
        this.elderlyId = elderlyId;
        this.deviceId = deviceId;
        this.beltType = beltType;
        this.alertType = alertType;
        this.status = status;
        this.audioMessage = audioMessage;
        this.latitude = latitude;
        this.longitude = longitude;
        this.createdAt = LocalDateTime.now().toString();
    }

    @PrePersist
    public void prePersist() {
        if (createdAt == null) {
            createdAt = LocalDateTime.now().toString();
        }
        if (status == null) {
            status = "PENDING_ACK";
        }
    }

    public Long getId() { return id; }
    public void setId(Long id) { this.id = id; }

    public String getAlertId() { return alertId; }
    public void setAlertId(String alertId) { this.alertId = alertId; }

    public String getElderlyId() { return elderlyId; }
    public void setElderlyId(String elderlyId) { this.elderlyId = elderlyId; }

    public String getDeviceId() { return deviceId; }
    public void setDeviceId(String deviceId) { this.deviceId = deviceId; }

    public String getBeltType() { return beltType; }
    public void setBeltType(String beltType) { this.beltType = beltType; }

    public String getAlertType() { return alertType; }
    public void setAlertType(String alertType) { this.alertType = alertType; }

    public String getStatus() { return status; }
    public void setStatus(String status) { this.status = status; }

    public String getAudioMessage() { return audioMessage; }
    public void setAudioMessage(String audioMessage) { this.audioMessage = audioMessage; }

    public String getAcknowledgedBy() { return acknowledgedBy; }
    public void setAcknowledgedBy(String acknowledgedBy) { this.acknowledgedBy = acknowledgedBy; }

    public String getAcknowledgedAt() { return acknowledgedAt; }
    public void setAcknowledgedAt(String acknowledgedAt) { this.acknowledgedAt = acknowledgedAt; }

    public Double getLatitude() { return latitude; }
    public void setLatitude(Double latitude) { this.latitude = latitude; }

    public Double getLongitude() { return longitude; }
    public void setLongitude(Double longitude) { this.longitude = longitude; }

    public String getCreatedAt() { return createdAt; }
    public void setCreatedAt(String createdAt) { this.createdAt = createdAt; }
}
