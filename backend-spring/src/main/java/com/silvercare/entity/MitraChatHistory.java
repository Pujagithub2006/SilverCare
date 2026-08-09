package com.silvercare.entity;

import jakarta.persistence.*;
import java.time.LocalDateTime;

@Entity
@Table(name = "mitra_chat_history")
public class MitraChatHistory {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    @Column(name = "chat_id")
    private Long chatId;

    @Column(name = "elderly_id", nullable = false)
    private String elderlyId;

    @Column(name = "guardian_username")
    private String guardianUsername;

    @Column(name = "message", nullable = false, length = 2000)
    private String message;

    @Column(name = "sender", nullable = false)
    private String sender; // ELDERLY, GUARDIAN, MITRA

    @Column(name = "ml_state_at_time")
    private String mlStateAtTime; // NORMAL, PREFALL, FALL, SUDDEN_MOVEMENT

    @Column(name = "ml_confidence")
    private Double mlConfidence;

    @Column(name = "heart_rate_at_time")
    private Integer heartRateAtTime;

    @Column(name = "spo2_at_time")
    private Integer spo2AtTime;

    @Column(name = "timestamp", nullable = false)
    private LocalDateTime timestamp;

    @Column(name = "context_tags", length = 500)
    private String contextTags; // JSON array of tags for ML learning

    @Column(name = "sentiment")
    private String sentiment; // POSITIVE, NEGATIVE, NEUTRAL, URGENT

    @Column(name = "is_learned")
    private Boolean isLearned = false; // Whether ML has processed this for learning

    public MitraChatHistory() {}

    public MitraChatHistory(String elderlyId, String message, String sender) {
        this.elderlyId = elderlyId;
        this.message = message;
        this.sender = sender;
        this.timestamp = LocalDateTime.now();
    }

    @PrePersist
    public void prePersist() {
        if (timestamp == null) {
            timestamp = LocalDateTime.now();
        }
        if (isLearned == null) {
            isLearned = false;
        }
    }

    // Getters and Setters
    public Long getChatId() { return chatId; }
    public void setChatId(Long chatId) { this.chatId = chatId; }

    public String getElderlyId() { return elderlyId; }
    public void setElderlyId(String elderlyId) { this.elderlyId = elderlyId; }

    public String getGuardianUsername() { return guardianUsername; }
    public void setGuardianUsername(String guardianUsername) { this.guardianUsername = guardianUsername; }

    public String getMessage() { return message; }
    public void setMessage(String message) { this.message = message; }

    public String getSender() { return sender; }
    public void setSender(String sender) { this.sender = sender; }

    public String getMlStateAtTime() { return mlStateAtTime; }
    public void setMlStateAtTime(String mlStateAtTime) { this.mlStateAtTime = mlStateAtTime; }

    public Double getMlConfidence() { return mlConfidence; }
    public void setMlConfidence(Double mlConfidence) { this.mlConfidence = mlConfidence; }

    public Integer getHeartRateAtTime() { return heartRateAtTime; }
    public void setHeartRateAtTime(Integer heartRateAtTime) { this.heartRateAtTime = heartRateAtTime; }

    public Integer getSpo2AtTime() { return spo2AtTime; }
    public void setSpo2AtTime(Integer spo2AtTime) { this.spo2AtTime = spo2AtTime; }

    public LocalDateTime getTimestamp() { return timestamp; }
    public void setTimestamp(LocalDateTime timestamp) { this.timestamp = timestamp; }

    public String getContextTags() { return contextTags; }
    public void setContextTags(String contextTags) { this.contextTags = contextTags; }

    public String getSentiment() { return sentiment; }
    public void setSentiment(String sentiment) { this.sentiment = sentiment; }

    public Boolean getIsLearned() { return isLearned; }
    public void setIsLearned(Boolean isLearned) { this.isLearned = isLearned; }

    public static MitraChatHistoryBuilder builder() { return new MitraChatHistoryBuilder(); }

    public static class MitraChatHistoryBuilder {
        private String elderlyId;
        private String guardianUsername;
        private String message;
        private String sender;
        private String mlStateAtTime;
        private Double mlConfidence;
        private Integer heartRateAtTime;
        private Integer spo2AtTime;
        private LocalDateTime timestamp;
        private String contextTags;
        private String sentiment;
        private Boolean isLearned;

        public MitraChatHistoryBuilder elderlyId(String elderlyId) { this.elderlyId = elderlyId; return this; }
        public MitraChatHistoryBuilder guardianUsername(String guardianUsername) { this.guardianUsername = guardianUsername; return this; }
        public MitraChatHistoryBuilder message(String message) { this.message = message; return this; }
        public MitraChatHistoryBuilder sender(String sender) { this.sender = sender; return this; }
        public MitraChatHistoryBuilder mlStateAtTime(String mlStateAtTime) { this.mlStateAtTime = mlStateAtTime; return this; }
        public MitraChatHistoryBuilder mlConfidence(Double mlConfidence) { this.mlConfidence = mlConfidence; return this; }
        public MitraChatHistoryBuilder heartRateAtTime(Integer heartRateAtTime) { this.heartRateAtTime = heartRateAtTime; return this; }
        public MitraChatHistoryBuilder spo2AtTime(Integer spo2AtTime) { this.spo2AtTime = spo2AtTime; return this; }
        public MitraChatHistoryBuilder timestamp(LocalDateTime timestamp) { this.timestamp = timestamp; return this; }
        public MitraChatHistoryBuilder contextTags(String contextTags) { this.contextTags = contextTags; return this; }
        public MitraChatHistoryBuilder sentiment(String sentiment) { this.sentiment = sentiment; return this; }
        public MitraChatHistoryBuilder isLearned(Boolean isLearned) { this.isLearned = isLearned; return this; }

        public MitraChatHistory build() {
            MitraChatHistory chat = new MitraChatHistory();
            chat.setElderlyId(elderlyId);
            chat.setGuardianUsername(guardianUsername);
            chat.setMessage(message);
            chat.setSender(sender);
            chat.setMlStateAtTime(mlStateAtTime);
            chat.setMlConfidence(mlConfidence);
            chat.setHeartRateAtTime(heartRateAtTime);
            chat.setSpo2AtTime(spo2AtTime);
            chat.setTimestamp(timestamp);
            chat.setContextTags(contextTags);
            chat.setSentiment(sentiment);
            chat.setIsLearned(isLearned);
            return chat;
        }
    }
}
