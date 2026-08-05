package com.silvercare.repository;

import com.silvercare.entity.ChatMemory;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.List;

@Repository
public interface ChatMemoryRepository extends JpaRepository<ChatMemory, Long> {
    List<ChatMemory> findTop10ByElderlyIdOrderByIdDesc(String elderlyId);
    List<ChatMemory> findTop20ByOrderByIdDesc();
}
