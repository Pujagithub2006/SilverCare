package com.silvercare.repository;

import com.silvercare.entity.GuardianElderlyLink;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.List;
import java.util.Optional;

@Repository
public interface GuardianElderlyLinkRepository extends JpaRepository<GuardianElderlyLink, Long> {
    List<GuardianElderlyLink> findByGuardianUsername(String guardianUsername);
    List<GuardianElderlyLink> findByElderlyId(String elderlyId);
    Optional<GuardianElderlyLink> findByGuardianUsernameAndElderlyId(String guardianUsername, String elderlyId);
    void deleteByGuardianUsernameAndElderlyId(String guardianUsername, String elderlyId);
}
