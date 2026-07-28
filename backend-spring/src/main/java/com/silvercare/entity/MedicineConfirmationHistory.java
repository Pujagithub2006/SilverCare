package com.silvercare.entity;

import jakarta.persistence.*;
import java.time.LocalDateTime;

@Entity
@Table(name = "medicine_confirmations")
public class MedicineConfirmationHistory {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(name = "time_taken")
    private String timeTaken;

    private Boolean taken;
    private String timestamp;
    private String type;

    public MedicineConfirmationHistory() {}

    public MedicineConfirmationHistory(Long id, String timeTaken, Boolean taken, String timestamp, String type) {
        this.id = id;
        this.timeTaken = timeTaken;
        this.taken = taken;
        this.timestamp = timestamp;
        this.type = type;
    }

    @PrePersist
    public void prePersist() {
        if (timestamp == null) {
            timestamp = LocalDateTime.now().toString();
        }
    }

    public Long getId() { return id; }
    public void setId(Long id) { this.id = id; }

    public String getTimeTaken() { return timeTaken; }
    public void setTimeTaken(String timeTaken) { this.timeTaken = timeTaken; }

    public Boolean getTaken() { return taken; }
    public void setTaken(Boolean taken) { this.taken = taken; }

    public String getTimestamp() { return timestamp; }
    public void setTimestamp(String timestamp) { this.timestamp = timestamp; }

    public String getType() { return type; }
    public void setType(String type) { this.type = type; }

    public static MedicineConfirmationHistoryBuilder builder() { return new MedicineConfirmationHistoryBuilder(); }

    public static class MedicineConfirmationHistoryBuilder {
        private Long id;
        private String timeTaken;
        private Boolean taken;
        private String timestamp;
        private String type;

        public MedicineConfirmationHistoryBuilder id(Long id) { this.id = id; return this; }
        public MedicineConfirmationHistoryBuilder timeTaken(String timeTaken) { this.timeTaken = timeTaken; return this; }
        public MedicineConfirmationHistoryBuilder taken(Boolean taken) { this.taken = taken; return this; }
        public MedicineConfirmationHistoryBuilder timestamp(String timestamp) { this.timestamp = timestamp; return this; }
        public MedicineConfirmationHistoryBuilder type(String type) { this.type = type; return this; }

        public MedicineConfirmationHistory build() {
            return new MedicineConfirmationHistory(id, timeTaken, taken, timestamp, type);
        }
    }
}
