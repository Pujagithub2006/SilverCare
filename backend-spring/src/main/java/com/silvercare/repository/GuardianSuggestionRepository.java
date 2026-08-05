package com.silvercare.repository;

import com.silvercare.entity.GuardianSuggestion;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.List;

@Repository
public interface GuardianSuggestionRepository extends JpaRepository<GuardianSuggestion, Long> {
    List<GuardianSuggestion> findByElderlyId(String elderlyId);
    List<GuardianSuggestion> findByElderlyIdIgnoreCase(String elderlyId);
}
