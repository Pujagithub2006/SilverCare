package com.silvercare.repository;

import com.silvercare.entity.GuardianElderlyLink;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Modifying;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;
import org.springframework.stereotype.Repository;

import java.util.List;
import java.util.Optional;

@Repository
public interface GuardianElderlyLinkRepository extends JpaRepository<GuardianElderlyLink, Long> {

    @Query("SELECT g FROM GuardianElderlyLink g WHERE g.guardian.username = :guardianUsername")
    List<GuardianElderlyLink> findByGuardianUsername(@Param("guardianUsername") String guardianUsername);

    List<GuardianElderlyLink> findByElderlyId(String elderlyId);

    @Query("SELECT g FROM GuardianElderlyLink g WHERE g.guardian.username = :guardianUsername AND g.elderlyId = :elderlyId")
    Optional<GuardianElderlyLink> findByGuardianUsernameAndElderlyId(@Param("guardianUsername") String guardianUsername, @Param("elderlyId") String elderlyId);

    @Modifying
    @Query("DELETE FROM GuardianElderlyLink g WHERE g.guardian.username = :guardianUsername AND g.elderlyId = :elderlyId")
    void deleteByGuardianUsernameAndElderlyId(@Param("guardianUsername") String guardianUsername, @Param("elderlyId") String elderlyId);
}

