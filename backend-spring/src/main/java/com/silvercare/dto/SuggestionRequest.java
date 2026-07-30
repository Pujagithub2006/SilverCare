package com.silvercare.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public class SuggestionRequest {
    @JsonProperty("guardian_username")
    private String guardianUsername;

    @JsonProperty("suggestion")
    private String suggestion;

    @JsonProperty("notes")
    private String notes;

    public SuggestionRequest() {}

    public SuggestionRequest(String guardianUsername, String suggestion) {
        this.guardianUsername = guardianUsername;
        this.suggestion = suggestion;
    }

    public String getGuardianUsername() { return guardianUsername; }
    public void setGuardianUsername(String guardianUsername) { this.guardianUsername = guardianUsername; }

    public String getSuggestion() {
        if (suggestion != null && !suggestion.trim().isEmpty()) return suggestion.trim();
        if (notes != null && !notes.trim().isEmpty()) return notes.trim();
        return "";
    }
    public void setSuggestion(String suggestion) { this.suggestion = suggestion; }

    public String getNotes() { return notes; }
    public void setNotes(String notes) { this.notes = notes; }
}
