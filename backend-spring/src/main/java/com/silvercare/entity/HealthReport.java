package com.silvercare.entity;

import jakarta.persistence.*;
import java.time.LocalDateTime;

@Entity
@Table(name = "health_reports")
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
    
    public HealthReport() {}

    public HealthReport(Long id, String elderlyId, String reportType, LocalDateTime reportDate, LocalDateTime startDate, LocalDateTime endDate, Integer totalFalls, Double avgHeartRate, Double minHeartRate, Double maxHeartRate, Double avgSpo2, Double minSpo2, Double avgTemperature, Integer mitraConversationCount, Integer mitraDistressCount, Integer mitraWellnessCount, String mlStateSummary, Double medicationAdherence, String reportData, Boolean pdfGenerated, String pdfPath, LocalDateTime createdAt) {
        this.id = id;
        this.elderlyId = elderlyId;
        this.reportType = reportType;
        this.reportDate = reportDate;
        this.startDate = startDate;
        this.endDate = endDate;
        this.totalFalls = totalFalls;
        this.avgHeartRate = avgHeartRate;
        this.minHeartRate = minHeartRate;
        this.maxHeartRate = maxHeartRate;
        this.avgSpo2 = avgSpo2;
        this.minSpo2 = minSpo2;
        this.avgTemperature = avgTemperature;
        this.mitraConversationCount = mitraConversationCount;
        this.mitraDistressCount = mitraDistressCount;
        this.mitraWellnessCount = mitraWellnessCount;
        this.mlStateSummary = mlStateSummary;
        this.medicationAdherence = medicationAdherence;
        this.reportData = reportData;
        this.pdfGenerated = pdfGenerated;
        this.pdfPath = pdfPath;
        this.createdAt = createdAt;
    }

    @PrePersist
    protected void onCreate() {
        createdAt = LocalDateTime.now();
        if (reportDate == null) {
            reportDate = LocalDateTime.now();
        }
    }

    // Getters and Setters
    public Long getId() { return id; }
    public void setId(Long id) { this.id = id; }

    public String getElderlyId() { return elderlyId; }
    public void setElderlyId(String elderlyId) { this.elderlyId = elderlyId; }

    public String getReportType() { return reportType; }
    public void setReportType(String reportType) { this.reportType = reportType; }

    public LocalDateTime getReportDate() { return reportDate; }
    public void setReportDate(LocalDateTime reportDate) { this.reportDate = reportDate; }

    public LocalDateTime getStartDate() { return startDate; }
    public void setStartDate(LocalDateTime startDate) { this.startDate = startDate; }

    public LocalDateTime getEndDate() { return endDate; }
    public void setEndDate(LocalDateTime endDate) { this.endDate = endDate; }

    public Integer getTotalFalls() { return totalFalls; }
    public void setTotalFalls(Integer totalFalls) { this.totalFalls = totalFalls; }

    public Double getAvgHeartRate() { return avgHeartRate; }
    public void setAvgHeartRate(Double avgHeartRate) { this.avgHeartRate = avgHeartRate; }

    public Double getMinHeartRate() { return minHeartRate; }
    public void setMinHeartRate(Double minHeartRate) { this.minHeartRate = minHeartRate; }

    public Double getMaxHeartRate() { return maxHeartRate; }
    public void setMaxHeartRate(Double maxHeartRate) { this.maxHeartRate = maxHeartRate; }

    public Double getAvgSpo2() { return avgSpo2; }
    public void setAvgSpo2(Double avgSpo2) { this.avgSpo2 = avgSpo2; }

    public Double getMinSpo2() { return minSpo2; }
    public void setMinSpo2(Double minSpo2) { this.minSpo2 = minSpo2; }

    public Double getAvgTemperature() { return avgTemperature; }
    public void setAvgTemperature(Double avgTemperature) { this.avgTemperature = avgTemperature; }

    public Integer getMitraConversationCount() { return mitraConversationCount; }
    public void setMitraConversationCount(Integer mitraConversationCount) { this.mitraConversationCount = mitraConversationCount; }

    public Integer getMitraDistressCount() { return mitraDistressCount; }
    public void setMitraDistressCount(Integer mitraDistressCount) { this.mitraDistressCount = mitraDistressCount; }

    public Integer getMitraWellnessCount() { return mitraWellnessCount; }
    public void setMitraWellnessCount(Integer mitraWellnessCount) { this.mitraWellnessCount = mitraWellnessCount; }

    public String getMlStateSummary() { return mlStateSummary; }
    public void setMlStateSummary(String mlStateSummary) { this.mlStateSummary = mlStateSummary; }

    public Double getMedicationAdherence() { return medicationAdherence; }
    public void setMedicationAdherence(Double medicationAdherence) { this.medicationAdherence = medicationAdherence; }

    public String getReportData() { return reportData; }
    public void setReportData(String reportData) { this.reportData = reportData; }

    public Boolean getPdfGenerated() { return pdfGenerated; }
    public void setPdfGenerated(Boolean pdfGenerated) { this.pdfGenerated = pdfGenerated; }

    public String getPdfPath() { return pdfPath; }
    public void setPdfPath(String pdfPath) { this.pdfPath = pdfPath; }

    public LocalDateTime getCreatedAt() { return createdAt; }
    public void setCreatedAt(LocalDateTime createdAt) { this.createdAt = createdAt; }

    public static HealthReportBuilder builder() {
        return new HealthReportBuilder();
    }

    public static class HealthReportBuilder {
        private Long id;
        private String elderlyId;
        private String reportType;
        private LocalDateTime reportDate;
        private LocalDateTime startDate;
        private LocalDateTime endDate;
        private Integer totalFalls;
        private Double avgHeartRate;
        private Double minHeartRate;
        private Double maxHeartRate;
        private Double avgSpo2;
        private Double minSpo2;
        private Double avgTemperature;
        private Integer mitraConversationCount;
        private Integer mitraDistressCount;
        private Integer mitraWellnessCount;
        private String mlStateSummary;
        private Double medicationAdherence;
        private String reportData;
        private Boolean pdfGenerated = false;
        private String pdfPath;
        private LocalDateTime createdAt;

        public HealthReportBuilder id(Long id) { this.id = id; return this; }
        public HealthReportBuilder elderlyId(String elderlyId) { this.elderlyId = elderlyId; return this; }
        public HealthReportBuilder reportType(String reportType) { this.reportType = reportType; return this; }
        public HealthReportBuilder reportDate(LocalDateTime reportDate) { this.reportDate = reportDate; return this; }
        public HealthReportBuilder startDate(LocalDateTime startDate) { this.startDate = startDate; return this; }
        public HealthReportBuilder endDate(LocalDateTime endDate) { this.endDate = endDate; return this; }
        public HealthReportBuilder totalFalls(Integer totalFalls) { this.totalFalls = totalFalls; return this; }
        public HealthReportBuilder avgHeartRate(Double avgHeartRate) { this.avgHeartRate = avgHeartRate; return this; }
        public HealthReportBuilder minHeartRate(Double minHeartRate) { this.minHeartRate = minHeartRate; return this; }
        public HealthReportBuilder maxHeartRate(Double maxHeartRate) { this.maxHeartRate = maxHeartRate; return this; }
        public HealthReportBuilder avgSpo2(Double avgSpo2) { this.avgSpo2 = avgSpo2; return this; }
        public HealthReportBuilder minSpo2(Double minSpo2) { this.minSpo2 = minSpo2; return this; }
        public HealthReportBuilder avgTemperature(Double avgTemperature) { this.avgTemperature = avgTemperature; return this; }
        public HealthReportBuilder mitraConversationCount(Integer mitraConversationCount) { this.mitraConversationCount = mitraConversationCount; return this; }
        public HealthReportBuilder mitraDistressCount(Integer mitraDistressCount) { this.mitraDistressCount = mitraDistressCount; return this; }
        public HealthReportBuilder mitraWellnessCount(Integer mitraWellnessCount) { this.mitraWellnessCount = mitraWellnessCount; return this; }
        public HealthReportBuilder mlStateSummary(String mlStateSummary) { this.mlStateSummary = mlStateSummary; return this; }
        public HealthReportBuilder medicationAdherence(Double medicationAdherence) { this.medicationAdherence = medicationAdherence; return this; }
        public HealthReportBuilder reportData(String reportData) { this.reportData = reportData; return this; }
        public HealthReportBuilder pdfGenerated(Boolean pdfGenerated) { this.pdfGenerated = pdfGenerated; return this; }
        public HealthReportBuilder pdfPath(String pdfPath) { this.pdfPath = pdfPath; return this; }
        public HealthReportBuilder createdAt(LocalDateTime createdAt) { this.createdAt = createdAt; return this; }

        public HealthReport build() {
            return new HealthReport(id, elderlyId, reportType, reportDate, startDate, endDate, totalFalls, avgHeartRate, minHeartRate, maxHeartRate, avgSpo2, minSpo2, avgTemperature, mitraConversationCount, mitraDistressCount, mitraWellnessCount, mlStateSummary, medicationAdherence, reportData, pdfGenerated, pdfPath, createdAt);
        }
    }
}
