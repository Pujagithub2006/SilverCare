package com.silvercare.repository;

import com.silvercare.entity.Medicine;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.List;

@Repository
public interface MedicineRepository extends JpaRepository<Medicine, Long> {
    List<Medicine> findByElderlyId(String elderlyId);
    List<Medicine> findByElderlyIdAndActiveTrue(String elderlyId);
}
