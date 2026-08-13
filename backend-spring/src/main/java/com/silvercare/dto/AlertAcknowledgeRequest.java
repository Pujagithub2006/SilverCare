package com.silvercare.dto;

public class AlertAcknowledgeRequest {
    private String alertId;
    private String guardianUsername;
    private String responseMessage; // e.g. "I am Fine" / "Acknowledged"
    private String responseType; // "YES" or "NO" for guardian acknowledgment

    public AlertAcknowledgeRequest() {}

    public AlertAcknowledgeRequest(String alertId, String guardianUsername, String responseMessage) {
        this.alertId = alertId;
        this.guardianUsername = guardianUsername;
        this.responseMessage = responseMessage;
    }

    public String getAlertId() { return alertId; }
    public void setAlertId(String alertId) { this.alertId = alertId; }

    public String getGuardianUsername() { return guardianUsername; }
    public void setGuardianUsername(String guardianUsername) { this.guardianUsername = guardianUsername; }

    public String getResponseMessage() { return responseMessage; }
    public void setResponseMessage(String responseMessage) { this.responseMessage = responseMessage; }

    public String getResponseType() { return responseType; }
    public void setResponseType(String responseType) { this.responseType = responseType; }
}
