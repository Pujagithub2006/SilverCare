package com.silvercare.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public class NotifyGuardianFallRequest {
    @JsonProperty("elderly_name")
    private String elderlyName;

    @JsonProperty("device_id")
    private String deviceId;

    private String location;

    public NotifyGuardianFallRequest() {}

    public NotifyGuardianFallRequest(String elderlyName, String deviceId, String location) {
        this.elderlyName = elderlyName;
        this.deviceId = deviceId;
        this.location = location;
    }

    public String getElderlyName() { return elderlyName; }
    public void setElderlyName(String elderlyName) { this.elderlyName = elderlyName; }

    public String getDeviceId() { return deviceId; }
    public void setDeviceId(String deviceId) { this.deviceId = deviceId; }

    public String getLocation() { return location; }
    public void setLocation(String location) { this.location = location; }
}
