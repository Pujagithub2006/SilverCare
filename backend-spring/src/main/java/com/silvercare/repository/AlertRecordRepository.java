package com.silvercare.repository;

import com.silvercare.entity.AlertRecord;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.List;
import java.util.Optional;

@Repository
public interface AlertRecordRepository extends JpaRepository<AlertRecord, Long> {
    Optional<AlertRecord> findByAlertId(String alertId);
    List<AlertRecord> findByElderlyIdOrderByCreatedAtDesc(String elderlyId);
    List<AlertRecord> findByStatus(String status);
    List<AlertRecord> findByElderlyIdAndStatus(String elderlyId, String status);
}
