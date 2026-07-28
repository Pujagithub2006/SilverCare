package com.silvercare.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public class FallDetectRequest {
    @JsonProperty("device_id")
    private String deviceId;

    @JsonProperty("deviceId")
    private String deviceIdCamel;

    private String timestamp;
    private Double confidence;

    public FallDetectRequest() {}

    public FallDetectRequest(String deviceId, String deviceIdCamel, String timestamp, Double confidence) {
        this.deviceId = deviceId;
        this.deviceIdCamel = deviceIdCamel;
        this.timestamp = timestamp;
        this.confidence = confidence;
    }

    public String getDeviceId() { return deviceId != null ? deviceId : deviceIdCamel; }
    public void setDeviceId(String deviceId) { this.deviceId = deviceId; }

    public String getDeviceIdCamel() { return deviceIdCamel; }
    public void setDeviceIdCamel(String deviceIdCamel) { this.deviceIdCamel = deviceIdCamel; }

    public String getTimestamp() { return timestamp; }
    public void setTimestamp(String timestamp) { this.timestamp = timestamp; }

    public Double getConfidence() { return confidence; }
    public void setConfidence(Double confidence) { this.confidence = confidence; }
}
