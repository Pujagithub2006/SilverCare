package com.silvercare.repository;

import com.silvercare.entity.MitraChatHistory;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;
import org.springframework.stereotype.Repository;

import java.time.LocalDateTime;
import java.util.List;

@Repository
public interface MitraChatHistoryRepository extends JpaRepository<MitraChatHistory, Long> {

    List<MitraChatHistory> findByElderlyIdOrderByTimestampDesc(String elderlyId);

    List<MitraChatHistory> findByElderlyIdAndIsLearnedFalse(String elderlyId);

    @Query("SELECT m FROM MitraChatHistory m WHERE m.elderlyId = :elderlyId AND m.timestamp >= :since ORDER BY m.timestamp DESC")
    List<MitraChatHistory> findRecentChatsForML(@Param("elderlyId") String elderlyId, @Param("since") LocalDateTime since);

    @Query("SELECT m FROM MitraChatHistory m WHERE m.elderlyId = :elderlyId AND m.mlStateAtTime = :state ORDER BY m.timestamp DESC")
    List<MitraChatHistory> findByElderlyIdAndMLState(@Param("elderlyId") String elderlyId, @Param("state") String state);

    @Query("SELECT COUNT(m) FROM MitraChatHistory m WHERE m.elderlyId = :elderlyId AND m.sender = 'ELDERLY' AND m.timestamp >= :since")
    Long countElderlyMessagesSince(@Param("elderlyId") String elderlyId, @Param("since") LocalDateTime since);

    @Query("SELECT m FROM MitraChatHistory m WHERE m.elderlyId = :elderlyId AND m.sentiment = 'URGENT' ORDER BY m.timestamp DESC")
    List<MitraChatHistory> findUrgentMessages(@Param("elderlyId") String elderlyId);
}
