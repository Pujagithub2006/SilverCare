package com.silvercare.entity;

import jakarta.persistence.*;
import lombok.AllArgsConstructor;
import lombok.Builder;
import lombok.Data;
import lombok.NoArgsConstructor;

import java.time.LocalDateTime;

@Entity
@Table(name = "health_reports")
@Data
@Builder
@NoArgsConstructor
@AllArgsConstructor
public class HealthReport {
    
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;
    
    @Column(name = "elderly_id", nullable = false)
    private String elderlyId;
    
    @Column(name = "report_type", nullable = false)
    private String reportType; // "DAILY", "WEEKLY"
    
    @Column(name = "report_date", nullable = false)
    private LocalDateTime reportDate;
    
    @Column(name = "start_date", nullable = false)
    private LocalDateTime startDate;
    
    @Column(name = "end_date", nullable = false)
    private LocalDateTime endDate;
    
    @Column(name = "total_falls")
    private Integer totalFalls;
    
    @Column(name = "avg_heart_rate")
    private Double avgHeartRate;
    
    @Column(name = "min_heart_rate")
    private Double minHeartRate;
    
    @Column(name = "max_heart_rate")
    private Double maxHeartRate;
    
    @Column(name = "avg_spo2")
    private Double avgSpo2;
    
    @Column(name = "min_spo2")
    private Double minSpo2;
    
    @Column(name = "avg_temperature")
    private Double avgTemperature;
    
    @Column(name = "mitra_conversation_count")
    private Integer mitraConversationCount;
    
    @Column(name = "mitra_distress_count")
    private Integer mitraDistressCount;
    
    @Column(name = "mitra_wellness_count")
    private Integer mitraWellnessCount;
    
    @Column(name = "ml_state_summary")
    private String mlStateSummary; // JSON summary of ML states
    
    @Column(name = "medication_adherence")
    private Double medicationAdherence; // Percentage
    
    @Column(name = "report_data", columnDefinition = "TEXT")
    private String reportData; // Full JSON report data
    
    @Column(name = "pdf_generated")
    private Boolean pdfGenerated = false;
    
    @Column(name = "pdf_path")
    private String pdfPath;
    
    @Column(name = "created_at")
    private LocalDateTime createdAt;
    
    @PrePersist
    protected void onCreate() {
        createdAt = LocalDateTime.now();
        if (reportDate == null) {
            reportDate = LocalDateTime.now();
        }
    }
}
