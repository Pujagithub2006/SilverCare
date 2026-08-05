package com.silvercare.entity;

import jakarta.persistence.*;
import java.time.LocalDateTime;
import java.util.ArrayList;
import java.util.List;

@Entity
@Table(name = "guardians")
public class Guardian {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(unique = true, nullable = false)
    private String username;

    private String name;

    @Column(name = "password_hash", nullable = false)
    private String passwordHash;

    private String phone;

    private String email;

    @OneToMany(mappedBy = "guardian", fetch = FetchType.EAGER, cascade = CascadeType.ALL, orphanRemoval = true)
    private List<GuardianElderlyLink> elderlyLinks = new ArrayList<>();

    @Column(name = "created_at")
    private String createdAt;

    public Guardian() {}

    public Guardian(Long id, String username, String name, String passwordHash, String phone, String email, List<String> elderlyLinked, String createdAt) {
        this.id = id;
        this.username = username;
        this.name = name;
        this.passwordHash = passwordHash;
        this.phone = phone;
        this.email = email;
        this.createdAt = createdAt;
    }

    @PrePersist
    public void prePersist() {
        if (createdAt == null) {
            createdAt = LocalDateTime.now().toString();
        }
    }

    public Long getId() { return id; }
    public void setId(Long id) { this.id = id; }

    public String getUsername() { return username; }
    public void setUsername(String username) { this.username = username; }

    public String getName() { return name; }
    public void setName(String name) { this.name = name; }

    public String getPasswordHash() { return passwordHash; }
    public void setPasswordHash(String passwordHash) { this.passwordHash = passwordHash; }

    public String getPhone() { return phone; }
    public void setPhone(String phone) { this.phone = phone; }

    public String getEmail() { return email; }
    public void setEmail(String email) { this.email = email; }

    public List<GuardianElderlyLink> getElderlyLinks() {
        if (elderlyLinks == null) elderlyLinks = new ArrayList<>();
        return elderlyLinks;
    }
    public void setElderlyLinks(List<GuardianElderlyLink> elderlyLinks) { this.elderlyLinks = elderlyLinks; }

    public List<String> getElderlyLinked() {
        List<String> list = new ArrayList<>();
        if (elderlyLinks != null) {
            for (GuardianElderlyLink link : elderlyLinks) {
                if (link.getElderlyId() != null && !list.contains(link.getElderlyId())) {
                    list.add(link.getElderlyId());
                }
            }
        }
        return list;
    }

    public String getCreatedAt() { return createdAt; }
    public void setCreatedAt(String createdAt) { this.createdAt = createdAt; }

    public static GuardianBuilder builder() { return new GuardianBuilder(); }

    public static class GuardianBuilder {
        private Long id;
        private String username;
        private String name;
        private String passwordHash;
        private String phone;
        private String email;
        private List<String> elderlyLinked = new ArrayList<>();
        private String createdAt;

        public GuardianBuilder id(Long id) { this.id = id; return this; }
        public GuardianBuilder username(String username) { this.username = username; return this; }
        public GuardianBuilder name(String name) { this.name = name; return this; }
        public GuardianBuilder passwordHash(String passwordHash) { this.passwordHash = passwordHash; return this; }
        public GuardianBuilder phone(String phone) { this.phone = phone; return this; }
        public GuardianBuilder email(String email) { this.email = email; return this; }
        public GuardianBuilder elderlyLinked(List<String> elderlyLinked) { this.elderlyLinked = elderlyLinked; return this; }
        public GuardianBuilder createdAt(String createdAt) { this.createdAt = createdAt; return this; }

        public Guardian build() {
            return new Guardian(id, username, name, passwordHash, phone, email, elderlyLinked, createdAt);
        }
    }
}
