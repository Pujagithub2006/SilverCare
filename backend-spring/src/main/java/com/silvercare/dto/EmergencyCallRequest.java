package com.silvercare.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public class EmergencyCallRequest {
    @JsonProperty("elderly_id")
    private String elderlyId;

    @JsonProperty("elderly_name")
    private String elderlyName;

    @JsonProperty("guardian_username")
    private String guardianUsername;

    private String location;

    public EmergencyCallRequest() {}

    public EmergencyCallRequest(String elderlyId, String elderlyName, String guardianUsername, String location) {
        this.elderlyId = elderlyId;
        this.elderlyName = elderlyName;
        this.guardianUsername = guardianUsername;
        this.location = location;
    }

    public String getElderlyId() { return elderlyId; }
    public void setElderlyId(String elderlyId) { this.elderlyId = elderlyId; }

    public String getElderlyName() { return elderlyName; }
    public void setElderlyName(String elderlyName) { this.elderlyName = elderlyName; }

    public String getGuardianUsername() { return guardianUsername; }
    public void setGuardianUsername(String guardianUsername) { this.guardianUsername = guardianUsername; }

    public String getLocation() { return location; }
    public void setLocation(String location) { this.location = location; }
}
