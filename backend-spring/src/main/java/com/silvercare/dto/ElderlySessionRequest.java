package com.silvercare.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public class ElderlySessionRequest {
    @JsonProperty("elderly_id")
    private String elderlyId;

    @JsonProperty("device_info")
    private String deviceInfo;

    public ElderlySessionRequest() {}

    public ElderlySessionRequest(String elderlyId, String deviceInfo) {
        this.elderlyId = elderlyId;
        this.deviceInfo = deviceInfo;
    }

    public String getElderlyId() { return elderlyId; }
    public void setElderlyId(String elderlyId) { this.elderlyId = elderlyId; }

    public String getDeviceInfo() { return deviceInfo; }
    public void setDeviceInfo(String deviceInfo) { this.deviceInfo = deviceInfo; }
}
