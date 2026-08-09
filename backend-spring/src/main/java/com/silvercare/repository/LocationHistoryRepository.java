package com.silvercare.repository;

import com.silvercare.entity.LocationHistory;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;
import org.springframework.stereotype.Repository;

import java.time.LocalDateTime;
import java.util.List;

@Repository
public interface LocationHistoryRepository extends JpaRepository<LocationHistory, Long> {
    
    List<LocationHistory> findByElderlyIdOrderByTimestampDesc(String elderlyId);
    
    List<LocationHistory> findByElderlyIdAndTimestampBetweenOrderByTimestampAsc(
        String elderlyId, 
        LocalDateTime start, 
        LocalDateTime end
    );
    
    @Query("SELECT l FROM LocationHistory l WHERE l.elderlyId = :elderlyId AND l.timestamp >= :since ORDER BY l.timestamp ASC")
    List<LocationHistory> findRecentLocations(@Param("elderlyId") String elderlyId, @Param("since") LocalDateTime since);
    
    @Query("SELECT l FROM LocationHistory l WHERE l.elderlyId = :elderlyId AND l.isUnusual = true ORDER BY l.timestamp DESC")
    List<LocationHistory> findUnusualLocations(@Param("elderlyId") String elderlyId);
    
    @Query("SELECT l FROM LocationHistory l WHERE l.elderlyId = :elderlyId AND l.timestamp >= :since ORDER BY l.timestamp DESC")
    List<LocationHistory> findLatestLocations(@Param("elderlyId") String elderlyId, @Param("since") LocalDateTime since);
}
