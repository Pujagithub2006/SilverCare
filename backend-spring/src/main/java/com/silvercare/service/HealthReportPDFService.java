package com.silvercare.service;

import com.silvercare.entity.Elderly;
import com.silvercare.entity.HealthReport;
import com.silvercare.repository.ElderlyRepository;
import org.apache.pdfbox.pdmodel.PDDocument;
import org.apache.pdfbox.pdmodel.PDPage;
import org.apache.pdfbox.pdmodel.PDPageContentStream;
import org.apache.pdfbox.pdmodel.font.PDType1Font;
import org.apache.pdfbox.pdmodel.font.PDFont;
import org.apache.pdfbox.pdmodel.common.PDRectangle;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Service;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.time.format.DateTimeFormatter;

@Service
public class HealthReportPDFService {

    @Autowired
    private ElderlyRepository elderlyRepository;

    private static final DateTimeFormatter DATE_FORMATTER = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm");
    private static final DateTimeFormatter DATE_ONLY_FORMATTER = DateTimeFormatter.ofPattern("yyyy-MM-dd");

    public byte[] generateHealthReportPDF(HealthReport report) throws IOException {
        try (PDDocument document = new PDDocument()) {
            PDPage page = new PDPage(PDRectangle.A4);
            document.addPage(page);

            Elderly elderly = elderlyRepository.findById(report.getElderlyId()).orElse(null);

            try (PDPageContentStream contentStream = new PDPageContentStream(document, page)) {
                PDFont font = PDType1Font.HELVETICA_BOLD;
                PDFont normalFont = PDType1Font.HELVETICA;

                float margin = 50;
                float yPosition = page.getMediaBox().getHeight() - margin;
                float startX = margin;

                // Title
                contentStream.setFont(font, 20);
                contentStream.beginText();
                contentStream.newLineAtOffset(startX, yPosition);
                contentStream.showText("SilverCare Health Report");
                contentStream.endText();
                yPosition -= 40;

                // Report Type and Date
                contentStream.setFont(font, 14);
                contentStream.beginText();
                contentStream.newLineAtOffset(startX, yPosition);
                contentStream.showText(report.getReportType() + " Report - " + report.getReportDate().format(DATE_ONLY_FORMATTER));
                contentStream.endText();
                yPosition -= 30;

                // Elderly Information
                if (elderly != null) {
                    contentStream.setFont(font, 12);
                    contentStream.beginText();
                    contentStream.newLineAtOffset(startX, yPosition);
                    contentStream.showText("Patient: " + elderly.getName());
                    contentStream.endText();
                    yPosition -= 20;

                    contentStream.setFont(normalFont, 11);
                    contentStream.beginText();
                    contentStream.newLineAtOffset(startX, yPosition);
                    contentStream.showText("ID: " + elderly.getElderlyId());
                    contentStream.endText();
                    yPosition -= 20;

                    contentStream.beginText();
                    contentStream.newLineAtOffset(startX, yPosition);
                    contentStream.showText("Age: " + (elderly.getAge() != null ? elderly.getAge() : "N/A"));
                    contentStream.endText();
                    yPosition -= 20;

                    contentStream.beginText();
                    contentStream.newLineAtOffset(startX, yPosition);
                    contentStream.showText("Location: " + (elderly.getLocation() != null ? elderly.getLocation() : "N/A"));
                    contentStream.endText();
                    yPosition -= 30;
                }

                // Report Period
                contentStream.setFont(font, 12);
                contentStream.beginText();
                contentStream.newLineAtOffset(startX, yPosition);
                contentStream.showText("Report Period:");
                contentStream.endText();
                yPosition -= 20;

                contentStream.setFont(normalFont, 11);
                contentStream.beginText();
                contentStream.newLineAtOffset(startX, yPosition);
                contentStream.showText("From: " + report.getStartDate().format(DATE_FORMATTER));
                contentStream.endText();
                yPosition -= 20;

                contentStream.beginText();
                contentStream.newLineAtOffset(startX, yPosition);
                contentStream.showText("To: " + report.getEndDate().format(DATE_FORMATTER));
                contentStream.endText();
                yPosition -= 40;

                // Health Vitals Section
                contentStream.setFont(font, 14);
                contentStream.beginText();
                contentStream.newLineAtOffset(startX, yPosition);
                contentStream.showText("Health Vitals Summary");
                contentStream.endText();
                yPosition -= 25;

                contentStream.setFont(normalFont, 11);

                contentStream.beginText();
                contentStream.newLineAtOffset(startX, yPosition);
                if (report.getAvgHeartRate() != null && report.getAvgHeartRate() > 0) {
                    contentStream.showText("Heart Rate: Avg " + report.getAvgHeartRate() + " BPM (Min: " + report.getMinHeartRate() + ", Max: " + report.getMaxHeartRate() + ")");
                } else {
                    contentStream.showText("Heart Rate: Data not available from hardware");
                }
                contentStream.endText();
                yPosition -= 20;

                contentStream.beginText();
                contentStream.newLineAtOffset(startX, yPosition);
                if (report.getAvgSpo2() != null && report.getAvgSpo2() > 0) {
                    contentStream.showText("SpO2: Avg " + report.getAvgSpo2() + "% (Min: " + report.getMinSpo2() + "%)");
                } else {
                    contentStream.showText("SpO2: Data not available from hardware");
                }
                contentStream.endText();
                yPosition -= 20;

                contentStream.beginText();
                contentStream.newLineAtOffset(startX, yPosition);
                if (report.getAvgTemperature() != null && report.getAvgTemperature() > 0) {
                    contentStream.showText("Temperature: Avg " + report.getAvgTemperature() + " C");
                } else {
                    contentStream.showText("Temperature: Data not available from hardware");
                }
                contentStream.endText();
                yPosition -= 40;

                // Fall Detection Section
                contentStream.setFont(font, 14);
                contentStream.beginText();
                contentStream.newLineAtOffset(startX, yPosition);
                contentStream.showText("Fall Detection");
                contentStream.endText();
                yPosition -= 25;

                contentStream.setFont(normalFont, 11);
                contentStream.beginText();
                contentStream.newLineAtOffset(startX, yPosition);
                contentStream.showText("Total Falls Detected: " + report.getTotalFalls());
                contentStream.endText();
                yPosition -= 40;

                // Mitra Chat Section
                contentStream.setFont(font, 14);
                contentStream.beginText();
                contentStream.newLineAtOffset(startX, yPosition);
                contentStream.showText("Mitra AI Assistant Interactions");
                contentStream.endText();
                yPosition -= 25;

                contentStream.setFont(normalFont, 11);
                contentStream.beginText();
                contentStream.newLineAtOffset(startX, yPosition);
                contentStream.showText("Total Conversations: " + report.getMitraConversationCount());
                contentStream.endText();
                yPosition -= 20;

                contentStream.beginText();
                contentStream.newLineAtOffset(startX, yPosition);
                contentStream.showText("Distress Indicators: " + report.getMitraDistressCount());
                contentStream.endText();
                yPosition -= 20;

                contentStream.beginText();
                contentStream.newLineAtOffset(startX, yPosition);
                contentStream.showText("Wellness Indicators: " + report.getMitraWellnessCount());
                contentStream.endText();
                yPosition -= 40;

                // Medication Adherence
                contentStream.setFont(font, 14);
                contentStream.beginText();
                contentStream.newLineAtOffset(startX, yPosition);
                contentStream.showText("Medication Adherence");
                contentStream.endText();
                yPosition -= 25;

                contentStream.setFont(normalFont, 11);
                contentStream.beginText();
                contentStream.newLineAtOffset(startX, yPosition);
                contentStream.showText("Adherence Rate: " + report.getMedicationAdherence() + "%");
                contentStream.endText();
                yPosition -= 40;

                // ML State Summary
                contentStream.setFont(font, 14);
                contentStream.beginText();
                contentStream.newLineAtOffset(startX, yPosition);
                contentStream.showText("Activity State Distribution");
                contentStream.endText();
                yPosition -= 25;

                contentStream.setFont(normalFont, 11);
                contentStream.beginText();
                contentStream.newLineAtOffset(startX, yPosition);
                contentStream.showText(report.getMlStateSummary());
                contentStream.endText();
                yPosition -= 40;

                // Footer
                contentStream.setFont(normalFont, 9);
                contentStream.beginText();
                contentStream.newLineAtOffset(startX, 50);
                contentStream.showText("Generated by SilverCare Elderly Monitoring System");
                contentStream.endText();
            }

            ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
            document.save(outputStream);
            return outputStream.toByteArray();
        }
    }
}
