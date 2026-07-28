package com.silvercare.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public class SwitchUserRequest {
    @JsonProperty("elderly_id")
    private String elderlyId;

    public SwitchUserRequest() {}

    public SwitchUserRequest(String elderlyId) {
        this.elderlyId = elderlyId;
    }

    public String getElderlyId() { return elderlyId; }
    public void setElderlyId(String elderlyId) { this.elderlyId = elderlyId; }
}
