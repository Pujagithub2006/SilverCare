package com.silvercare.service;

import com.silvercare.entity.HealthReport;
import com.silvercare.repository.ElderlyRepository;
import com.silvercare.repository.HealthReportRepository;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Service;

import java.time.LocalDateTime;
import java.util.List;

@Service
public class HealthReportScheduler {

    @Autowired
    private ElderlyRepository elderlyRepository;

    @Autowired
    private HealthReportRepository healthReportRepository;

    /**
     * Scheduled task to generate daily health reports for all elderly users every hour.
     * This ensures reports are always up-to-date without manual intervention.
     * Runs at the start of every hour (e.g., 1:00, 2:00, 3:00, etc.)
     */
    @Scheduled(cron = "0 0 * * * ?")
    public void generateHourlyHealthReports() {
        System.out.println("[SCHEDULER]: Starting hourly health report generation - " + LocalDateTime.now());
        
        List<String> allElderlyIds = elderlyRepository.findAllElderlyIds();
        
        for (String elderlyId : allElderlyIds) {
            try {
                generateDailyReportForElderly(elderlyId);
                generateWeeklyReportForElderly(elderlyId);
            } catch (Exception e) {
                System.err.println("[SCHEDULER]: Failed to generate report for " + elderlyId + ": " + e.getMessage());
            }
        }
        
        System.out.println("[SCHEDULER]: Completed hourly health report generation - " + LocalDateTime.now());
    }

    /**
     * Generate a daily report for a specific elderly user if one doesn't exist for today
     */
    private void generateDailyReportForElderly(String elderlyId) {
        LocalDateTime now = LocalDateTime.now();
        LocalDateTime startOfDay = now.toLocalDate().atStartOfDay();
        LocalDateTime endOfDay = now.toLocalDate().atTime(23, 59, 59);

        // Check if report already exists for today
        var existing = healthReportRepository.findByElderlyIdAndReportTypeAndReportDate(
            elderlyId, "DAILY", now
        );

        if (existing.isPresent()) {
            // Update existing report with latest data
            HealthReport report = existing.get();
            updateReportData(report, startOfDay, endOfDay);
            healthReportRepository.save(report);
            System.out.println("[SCHEDULER]: Updated daily report for " + elderlyId);
        } else {
            // Create new report
            HealthReport report = createReportData(elderlyId, "DAILY", startOfDay, endOfDay, now);
            healthReportRepository.save(report);
            System.out.println("[SCHEDULER]: Created daily report for " + elderlyId);
        }
    }

    /**
     * Generate a weekly report for a specific elderly user if one doesn't exist for this week
     */
    private void generateWeeklyReportForElderly(String elderlyId) {
        LocalDateTime now = LocalDateTime.now();
        LocalDateTime startOfWeek = now.minusDays(7).toLocalDate().atStartOfDay();
        LocalDateTime endOfDay = now.toLocalDate().atTime(23, 59, 59);

        // Check if report already exists for today (weekly reports are generated daily but cover 7 days)
        var existing = healthReportRepository.findByElderlyIdAndReportTypeAndReportDate(
            elderlyId, "WEEKLY", now
        );

        if (existing.isPresent()) {
            // Update existing report with latest data
            HealthReport report = existing.get();
            updateReportData(report, startOfWeek, endOfDay);
            healthReportRepository.save(report);
            System.out.println("[SCHEDULER]: Updated weekly report for " + elderlyId);
        } else {
            // Create new report
            HealthReport report = createReportData(elderlyId, "WEEKLY", startOfWeek, endOfDay, now);
            healthReportRepository.save(report);
            System.out.println("[SCHEDULER]: Created weekly report for " + elderlyId);
        }
    }

    /**
     * Create a new health report with data from the specified time range
     */
    private HealthReport createReportData(String elderlyId, String reportType, 
                                          LocalDateTime startDate, LocalDateTime endDate, LocalDateTime reportDate) {
        // Fetch alert records for falls
        List<com.silvercare.entity.AlertRecord> allAlerts = alertRecordRepository.findByElderlyIdOrderByCreatedAtDesc(elderlyId);
        List<com.silvercare.entity.AlertRecord> alerts = allAlerts.stream()
            .filter(a -> {
                try {
                    LocalDateTime alertTime = LocalDateTime.parse(a.getCreatedAt());
                    return !alertTime.isBefore(startDate) && !alertTime.isAfter(endDate);
                } catch (Exception e) {
                    return false;
                }
            })
            .toList();
        int totalFalls = (int) alerts.stream().filter(a -> a.getAlertType() != null && 
            a.getAlertType().contains("FALL")).count();

        // Fetch Mitra chat history
        List<com.silvercare.entity.MitraChatHistory> allChats = mitraChatHistoryRepository.findByElderlyIdOrderByTimestampDesc(elderlyId);
        List<com.silvercare.entity.MitraChatHistory> chats = allChats.stream()
            .filter(c -> !c.getTimestamp().isBefore(startDate) && !c.getTimestamp().isAfter(endDate))
            .toList();
        int mitraConversations = chats.size();
        long mitraDistress = chats.stream().filter(c -> "DISTRESS".equals(c.getMlStateAtTime())).count();
        long mitraWellness = chats.stream().filter(c -> "WELLNESS".equals(c.getMlStateAtTime())).count();

        // Calculate averages (placeholder values - would need sensor data table for accurate values)
        double avgHeartRate = 75.0;
        double minHeartRate = 60.0;
        double maxHeartRate = 100.0;
        double avgSpo2 = 97.0;
        double minSpo2 = 94.0;
        double avgTemperature = 36.6;
        double medicationAdherence = 85.0;

        return HealthReport.builder()
            .elderlyId(elderlyId)
            .reportType(reportType)
            .reportDate(reportDate)
            .startDate(startDate)
            .endDate(endDate)
            .totalFalls(totalFalls)
            .avgHeartRate(avgHeartRate)
            .minHeartRate(minHeartRate)
            .maxHeartRate(maxHeartRate)
            .avgSpo2(avgSpo2)
            .minSpo2(minSpo2)
            .avgTemperature(avgTemperature)
            .mitraConversationCount(mitraConversations)
            .mitraDistressCount((int) mitraDistress)
            .mitraWellnessCount((int) mitraWellness)
            .mlStateSummary("{\"NORMAL\": 80, \"PREFALL\": 15, \"FALL\": 5}")
            .medicationAdherence(medicationAdherence)
            .pdfGenerated(false)
            .build();
    }

    /**
     * Update an existing health report with fresh data
     */
    private void updateReportData(HealthReport report, LocalDateTime startDate, LocalDateTime endDate) {
        String elderlyId = report.getElderlyId();

        // Fetch alert records for falls
        List<com.silvercare.entity.AlertRecord> allAlerts = alertRecordRepository.findByElderlyIdOrderByCreatedAtDesc(elderlyId);
        List<com.silvercare.entity.AlertRecord> alerts = allAlerts.stream()
            .filter(a -> {
                try {
                    LocalDateTime alertTime = LocalDateTime.parse(a.getCreatedAt());
                    return !alertTime.isBefore(startDate) && !alertTime.isAfter(endDate);
                } catch (Exception e) {
                    return false;
                }
            })
            .toList();
        report.setTotalFalls((int) alerts.stream().filter(a -> a.getAlertType() != null && 
            a.getAlertType().contains("FALL")).count());

        // Fetch Mitra chat history
        List<com.silvercare.entity.MitraChatHistory> allChats = mitraChatHistoryRepository.findByElderlyIdOrderByTimestampDesc(elderlyId);
        List<com.silvercare.entity.MitraChatHistory> chats = allChats.stream()
            .filter(c -> !c.getTimestamp().isBefore(startDate) && !c.getTimestamp().isAfter(endDate))
            .toList();
        report.setMitraConversationCount(chats.size());
        report.setMitraDistressCount((int) chats.stream().filter(c -> "DISTRESS".equals(c.getMlStateAtTime())).count());
        report.setMitraWellnessCount((int) chats.stream().filter(c -> "WELLNESS".equals(c.getMlStateAtTime())).count());

        // Update vitals averages (placeholder values)
        report.setAvgHeartRate(75.0);
        report.setMinHeartRate(60.0);
        report.setMaxHeartRate(100.0);
        report.setAvgSpo2(97.0);
        report.setMinSpo2(94.0);
        report.setAvgTemperature(36.6);
        report.setMedicationAdherence(85.0);
    }

    @Autowired
    private com.silvercare.repository.AlertRecordRepository alertRecordRepository;

    @Autowired
    private com.silvercare.repository.MitraChatHistoryRepository mitraChatHistoryRepository;
}
