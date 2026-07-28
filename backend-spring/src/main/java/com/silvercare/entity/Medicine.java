package com.silvercare.entity;

import jakarta.persistence.*;
import java.time.LocalDateTime;
import java.util.ArrayList;
import java.util.List;

@Entity
@Table(name = "medicines")
public class Medicine {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(name = "elderly_id", nullable = false)
    private String elderlyId;

    @Column(name = "medicine_name", nullable = false)
    private String medicineName;

    private String dosage;

    @ElementCollection(fetch = FetchType.EAGER)
    @CollectionTable(name = "medicine_times", joinColumns = @JoinColumn(name = "medicine_id"))
    @Column(name = "time_slot")
    private List<String> times = new ArrayList<>();

    private String instructions;

    @Column(name = "start_date")
    private String startDate;

    @Column(name = "end_date")
    private String endDate;

    private Boolean active = true;

    @Column(name = "created_at")
    private String createdAt;

    @OneToMany(cascade = CascadeType.ALL, fetch = FetchType.EAGER, orphanRemoval = true)
    @JoinColumn(name = "medicine_id")
    private List<MedicineConfirmationHistory> confirmationHistory = new ArrayList<>();

    public Medicine() {}

    public Medicine(Long id, String elderlyId, String medicineName, String dosage, List<String> times, String instructions, String startDate, String endDate, Boolean active, String createdAt, List<MedicineConfirmationHistory> confirmationHistory) {
        this.id = id;
        this.elderlyId = elderlyId;
        this.medicineName = medicineName;
        this.dosage = dosage;
        this.times = times != null ? times : new ArrayList<>();
        this.instructions = instructions;
        this.startDate = startDate;
        this.endDate = endDate;
        this.active = active != null ? active : true;
        this.createdAt = createdAt;
        this.confirmationHistory = confirmationHistory != null ? confirmationHistory : new ArrayList<>();
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

    public String getMedicineName() { return medicineName; }
    public void setMedicineName(String medicineName) { this.medicineName = medicineName; }

    public String getDosage() { return dosage; }
    public void setDosage(String dosage) { this.dosage = dosage; }

    public List<String> getTimes() {
        if (times == null) times = new ArrayList<>();
        return times;
    }
    public void setTimes(List<String> times) { this.times = times; }

    public String getInstructions() { return instructions; }
    public void setInstructions(String instructions) { this.instructions = instructions; }

    public String getStartDate() { return startDate; }
    public void setStartDate(String startDate) { this.startDate = startDate; }

    public String getEndDate() { return endDate; }
    public void setEndDate(String endDate) { this.endDate = endDate; }

    public Boolean getActive() { return active; }
    public void setActive(Boolean active) { this.active = active; }

    public String getCreatedAt() { return createdAt; }
    public void setCreatedAt(String createdAt) { this.createdAt = createdAt; }

    public List<MedicineConfirmationHistory> getConfirmationHistory() {
        if (confirmationHistory == null) confirmationHistory = new ArrayList<>();
        return confirmationHistory;
    }
    public void setConfirmationHistory(List<MedicineConfirmationHistory> confirmationHistory) {
        this.confirmationHistory = confirmationHistory;
    }

    public static MedicineBuilder builder() { return new MedicineBuilder(); }

    public static class MedicineBuilder {
        private Long id;
        private String elderlyId;
        private String medicineName;
        private String dosage;
        private List<String> times = new ArrayList<>();
        private String instructions;
        private String startDate;
        private String endDate;
        private Boolean active = true;
        private String createdAt;
        private List<MedicineConfirmationHistory> confirmationHistory = new ArrayList<>();

        public MedicineBuilder id(Long id) { this.id = id; return this; }
        public MedicineBuilder elderlyId(String elderlyId) { this.elderlyId = elderlyId; return this; }
        public MedicineBuilder medicineName(String medicineName) { this.medicineName = medicineName; return this; }
        public MedicineBuilder dosage(String dosage) { this.dosage = dosage; return this; }
        public MedicineBuilder times(List<String> times) { this.times = times; return this; }
        public MedicineBuilder instructions(String instructions) { this.instructions = instructions; return this; }
        public MedicineBuilder startDate(String startDate) { this.startDate = startDate; return this; }
        public MedicineBuilder endDate(String endDate) { this.endDate = endDate; return this; }
        public MedicineBuilder active(Boolean active) { this.active = active; return this; }
        public MedicineBuilder createdAt(String createdAt) { this.createdAt = createdAt; return this; }
        public MedicineBuilder confirmationHistory(List<MedicineConfirmationHistory> confirmationHistory) {
            this.confirmationHistory = confirmationHistory; return this;
        }

        public Medicine build() {
            return new Medicine(id, elderlyId, medicineName, dosage, times, instructions, startDate, endDate, active, createdAt, confirmationHistory);
        }
    }
}
