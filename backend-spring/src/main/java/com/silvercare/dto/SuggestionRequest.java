package com.silvercare.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public class SuggestionRequest {
    @JsonProperty("guardian_username")
    private String guardianUsername;

    private String suggestion;

    public SuggestionRequest() {}

    public SuggestionRequest(String guardianUsername, String suggestion) {
        this.guardianUsername = guardianUsername;
        this.suggestion = suggestion;
    }

    public String getGuardianUsername() { return guardianUsername; }
    public void setGuardianUsername(String guardianUsername) { this.guardianUsername = guardianUsername; }

    public String getSuggestion() { return suggestion; }
    public void setSuggestion(String suggestion) { this.suggestion = suggestion; }
}
