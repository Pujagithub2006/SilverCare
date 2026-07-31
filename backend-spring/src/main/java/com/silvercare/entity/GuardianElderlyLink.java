package com.silvercare.entity;

import jakarta.persistence.*;
import java.time.LocalDateTime;

@Entity
@Table(name = "guardian_elderly_links", uniqueConstraints = {
    @UniqueConstraint(columnNames = {"guardian_id", "elderly_id"})
})
public class GuardianElderlyLink {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "guardian_id", nullable = false)
    private Guardian guardian;

    @Column(name = "elderly_id", nullable = false)
    private String elderlyId;

    @Column(name = "relationship")
    private String relationship;

    @Column(name = "created_at")
    private LocalDateTime createdAt;

    public GuardianElderlyLink() {}

    public GuardianElderlyLink(Long guardianId, String elderlyId, String relationship) {
        this.guardian = new Guardian();
        this.guardian.setId(guardianId);
        this.elderlyId = elderlyId;
        this.relationship = relationship;
        this.createdAt = LocalDateTime.now();
    }

    @PrePersist
    public void prePersist() {
        if (createdAt == null) {
            createdAt = LocalDateTime.now();
        }
    }

    public Long getId() { return id; }
    public void setId(Long id) { this.id = id; }

    public Guardian getGuardian() { return guardian; }
    public void setGuardian(Guardian guardian) { this.guardian = guardian; }

    public String getElderlyId() { return elderlyId; }
    public void setElderlyId(String elderlyId) { this.elderlyId = elderlyId; }

    public String getRelationship() { return relationship; }
    public void setRelationship(String relationship) { this.relationship = relationship; }

    public LocalDateTime getCreatedAt() { return createdAt; }
    public void setCreatedAt(LocalDateTime createdAt) { this.createdAt = createdAt; }
}
