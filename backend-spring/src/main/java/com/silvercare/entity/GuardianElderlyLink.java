package com.silvercare.entity;

import jakarta.persistence.*;
import java.time.LocalDateTime;

@Entity
@Table(name = "guardian_elderly_links", uniqueConstraints = {
    @UniqueConstraint(columnNames = {"guardian_username", "elderly_id"})
})
public class GuardianElderlyLink {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(name = "guardian_username", nullable = false)
    private String guardianUsername;

    @Column(name = "elderly_id", nullable = false)
    private String elderlyId;

    @Column(name = "relationship")
    private String relationship;

    @Column(name = "created_at")
    private String createdAt;

    public GuardianElderlyLink() {}

    public GuardianElderlyLink(String guardianUsername, String elderlyId, String relationship) {
        this.guardianUsername = guardianUsername;
        this.elderlyId = elderlyId;
        this.relationship = relationship;
        this.createdAt = LocalDateTime.now().toString();
    }

    @PrePersist
    public void prePersist() {
        if (createdAt == null) {
            createdAt = LocalDateTime.now().toString();
        }
    }

    public Long getId() { return id; }
    public void setId(Long id) { this.id = id; }

    public String getGuardianUsername() { return guardianUsername; }
    public void setGuardianUsername(String guardianUsername) { this.guardianUsername = guardianUsername; }

    public String getElderlyId() { return elderlyId; }
    public void setElderlyId(String elderlyId) { this.elderlyId = elderlyId; }

    public String getRelationship() { return relationship; }
    public void setRelationship(String relationship) { this.relationship = relationship; }

    public String getCreatedAt() { return createdAt; }
    public void setCreatedAt(String createdAt) { this.createdAt = createdAt; }
}
