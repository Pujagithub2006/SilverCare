package com.silvercare.entity;

import jakarta.persistence.*;
import java.time.LocalDateTime;

@Entity
@Table(name = "devices")
public class Device {

    @Id
    @Column(name = "device_id", nullable = false, unique = true)
    private String deviceId;

    @Column(name = "device_type", nullable = false)
    private String deviceType; // Waist Belt, Wrist Belt

    @Column(name = "mac_address")
    private String macAddress;

    @Column(name = "assigned_elderly_id")
    private String assignedElderlyId;

    @Column(name = "status", nullable = false)
    private String status; // ACTIVE, BROKEN, RETIRED, UNASSIGNED

    @Column(name = "firmware_version")
    private String firmwareVersion;

    @Column(name = "last_seen")
    private LocalDateTime lastSeen;

    @Column(name = "registered_at")
    private LocalDateTime registeredAt;

    @Column(name = "battery_level")
    private Integer batteryLevel;

    @Column(name = "notes")
    private String notes;

    public Device() {}

    public Device(String deviceId, String deviceType, String macAddress) {
        this.deviceId = deviceId;
        this.deviceType = deviceType;
        this.macAddress = macAddress;
        this.status = "UNASSIGNED";
        this.registeredAt = LocalDateTime.now();
    }

    @PrePersist
    public void prePersist() {
        if (registeredAt == null) {
            registeredAt = LocalDateTime.now();
        }
        if (status == null) {
            status = "UNASSIGNED";
        }
        if (deviceType == null) {
            deviceType = "Waist Belt";
        }
    }

    // Getters and Setters
    public String getDeviceId() { return deviceId; }
    public void setDeviceId(String deviceId) { this.deviceId = deviceId; }

    public String getDeviceType() { return deviceType; }
    public void setDeviceType(String deviceType) { this.deviceType = deviceType; }

    public String getMacAddress() { return macAddress; }
    public void setMacAddress(String macAddress) { this.macAddress = macAddress; }

    public String getAssignedElderlyId() { return assignedElderlyId; }
    public void setAssignedElderlyId(String assignedElderlyId) { this.assignedElderlyId = assignedElderlyId; }

    public String getStatus() { return status; }
    public void setStatus(String status) { this.status = status; }

    public String getFirmwareVersion() { return firmwareVersion; }
    public void setFirmwareVersion(String firmwareVersion) { this.firmwareVersion = firmwareVersion; }

    public LocalDateTime getLastSeen() { return lastSeen; }
    public void setLastSeen(LocalDateTime lastSeen) { this.lastSeen = lastSeen; }

    public LocalDateTime getRegisteredAt() { return registeredAt; }
    public void setRegisteredAt(LocalDateTime registeredAt) { this.registeredAt = registeredAt; }

    public Integer getBatteryLevel() { return batteryLevel; }
    public void setBatteryLevel(Integer batteryLevel) { this.batteryLevel = batteryLevel; }

    public String getNotes() { return notes; }
    public void setNotes(String notes) { this.notes = notes; }

    public static DeviceBuilder builder() { return new DeviceBuilder(); }

    public static class DeviceBuilder {
        private String deviceId;
        private String deviceType;
        private String macAddress;
        private String assignedElderlyId;
        private String status;
        private String firmwareVersion;
        private LocalDateTime lastSeen;
        private LocalDateTime registeredAt;
        private Integer batteryLevel;
        private String notes;

        public DeviceBuilder deviceId(String deviceId) { this.deviceId = deviceId; return this; }
        public DeviceBuilder deviceType(String deviceType) { this.deviceType = deviceType; return this; }
        public DeviceBuilder macAddress(String macAddress) { this.macAddress = macAddress; return this; }
        public DeviceBuilder assignedElderlyId(String assignedElderlyId) { this.assignedElderlyId = assignedElderlyId; return this; }
        public DeviceBuilder status(String status) { this.status = status; return this; }
        public DeviceBuilder firmwareVersion(String firmwareVersion) { this.firmwareVersion = firmwareVersion; return this; }
        public DeviceBuilder lastSeen(LocalDateTime lastSeen) { this.lastSeen = lastSeen; return this; }
        public DeviceBuilder registeredAt(LocalDateTime registeredAt) { this.registeredAt = registeredAt; return this; }
        public DeviceBuilder batteryLevel(Integer batteryLevel) { this.batteryLevel = batteryLevel; return this; }
        public DeviceBuilder notes(String notes) { this.notes = notes; return this; }

        public Device build() {
            Device device = new Device();
            device.setDeviceId(deviceId);
            device.setDeviceType(deviceType);
            device.setMacAddress(macAddress);
            device.setAssignedElderlyId(assignedElderlyId);
            device.setStatus(status);
            device.setFirmwareVersion(firmwareVersion);
            device.setLastSeen(lastSeen);
            device.setRegisteredAt(registeredAt);
            device.setBatteryLevel(batteryLevel);
            device.setNotes(notes);
            return device;
        }
    }
}
