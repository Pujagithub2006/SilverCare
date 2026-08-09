package com.silvercare.repository;

import com.silvercare.entity.HealthReport;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;
import org.springframework.stereotype.Repository;

import java.time.LocalDateTime;
import java.util.List;
import java.util.Optional;

@Repository
public interface HealthReportRepository extends JpaRepository<HealthReport, Long> {
    
    List<HealthReport> findByElderlyIdOrderByReportDateDesc(String elderlyId);
    
    Optional<HealthReport> findByElderlyIdAndReportTypeAndReportDate(
        String elderlyId, 
        String reportType, 
        LocalDateTime reportDate
    );
    
    @Query("SELECT r FROM HealthReport r WHERE r.elderlyId = :elderlyId AND r.reportType = :reportType ORDER BY r.reportDate DESC")
    List<HealthReport> findByElderlyIdAndReportType(@Param("elderlyId") String elderlyId, @Param("reportType") String reportType);
    
    @Query("SELECT r FROM HealthReport r WHERE r.elderlyId = :elderlyId AND r.reportDate >= :since ORDER BY r.reportDate DESC")
    List<HealthReport> findRecentReports(@Param("elderlyId") String elderlyId, @Param("since") LocalDateTime since);
}
