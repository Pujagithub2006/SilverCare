package com.silvercare.repository;

import com.silvercare.entity.Guardian;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.Optional;

@Repository
public interface GuardianRepository extends JpaRepository<Guardian, Long> {
    Optional<Guardian> findByUsername(String username);
    boolean existsByUsername(String username);
}
