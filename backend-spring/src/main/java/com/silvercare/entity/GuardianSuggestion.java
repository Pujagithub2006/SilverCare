package com.silvercare.entity;

import jakarta.persistence.*;
import java.time.LocalDateTime;

@Entity
@Table(name = "guardian_suggestions")
public class GuardianSuggestion {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(name = "elderly_id", nullable = false)
    private String elderlyId;

    @Column(name = "guardian_username", nullable = false)
    private String guardianUsername;

    @Column(length = 2000)
    private String suggestion;

    @Column(name = "created_at")
    private String createdAt;

    private Boolean active = true;

    public GuardianSuggestion() {}

    public GuardianSuggestion(Long id, String elderlyId, String guardianUsername, String suggestion, String createdAt, Boolean active) {
        this.id = id;
        this.elderlyId = elderlyId;
        this.guardianUsername = guardianUsername;
        this.suggestion = suggestion;
        this.createdAt = createdAt;
        this.active = active != null ? active : true;
    }

    @PrePersist
    public void prePersist() {
        if (createdAt == null) {
            createdAt = LocalDateTime.now().toString();
        }
        if (active == null) {
            active = true;
        }
    }

    public Long getId() { return id; }
    public void setId(Long id) { this.id = id; }

    public String getElderlyId() { return elderlyId; }
    public void setElderlyId(String elderlyId) { this.elderlyId = elderlyId; }

    public String getGuardianUsername() { return guardianUsername; }
    public void setGuardianUsername(String guardianUsername) { this.guardianUsername = guardianUsername; }

    public String getSuggestion() { return suggestion; }
    public void setSuggestion(String suggestion) { this.suggestion = suggestion; }

    public String getCreatedAt() { return createdAt; }
    public void setCreatedAt(String createdAt) { this.createdAt = createdAt; }

    public Boolean getActive() { return active; }
    public void setActive(Boolean active) { this.active = active; }

    public static GuardianSuggestionBuilder builder() { return new GuardianSuggestionBuilder(); }

    public static class GuardianSuggestionBuilder {
        private Long id;
        private String elderlyId;
        private String guardianUsername;
        private String suggestion;
        private String createdAt;
        private Boolean active = true;

        public GuardianSuggestionBuilder id(Long id) { this.id = id; return this; }
        public GuardianSuggestionBuilder elderlyId(String elderlyId) { this.elderlyId = elderlyId; return this; }
        public GuardianSuggestionBuilder guardianUsername(String guardianUsername) { this.guardianUsername = guardianUsername; return this; }
        public GuardianSuggestionBuilder suggestion(String suggestion) { this.suggestion = suggestion; return this; }
        public GuardianSuggestionBuilder createdAt(String createdAt) { this.createdAt = createdAt; return this; }
        public GuardianSuggestionBuilder active(Boolean active) { this.active = active; return this; }

        public GuardianSuggestion build() {
            return new GuardianSuggestion(id, elderlyId, guardianUsername, suggestion, createdAt, active);
        }
    }
}
