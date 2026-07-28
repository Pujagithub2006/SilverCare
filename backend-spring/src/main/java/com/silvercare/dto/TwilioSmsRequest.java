package com.silvercare.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public class TwilioSmsRequest {
    @JsonProperty("alert_type")
    private String alertType;

    private String message;

    @JsonProperty("device_id")
    private String deviceId;

    public TwilioSmsRequest() {}

    public TwilioSmsRequest(String alertType, String message, String deviceId) {
        this.alertType = alertType;
        this.message = message;
        this.deviceId = deviceId;
    }

    public String getAlertType() { return alertType; }
    public void setAlertType(String alertType) { this.alertType = alertType; }

    public String getMessage() { return message; }
    public void setMessage(String message) { this.message = message; }

    public String getDeviceId() { return deviceId; }
    public void setDeviceId(String deviceId) { this.deviceId = deviceId; }
}
