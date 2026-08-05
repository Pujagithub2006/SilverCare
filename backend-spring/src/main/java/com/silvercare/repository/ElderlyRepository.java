package com.silvercare.repository;

import com.silvercare.entity.Elderly;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.List;
import java.util.Optional;

@Repository
public interface ElderlyRepository extends JpaRepository<Elderly, String> {
    Optional<Elderly> findByElderlyId(String elderlyId);
    List<Elderly> findByGuardianUsername(String guardianUsername);
    List<Elderly> findByPhoneAndNameIgnoreCase(String phone, String name);
    List<Elderly> findByNameIgnoreCase(String name);
    List<Elderly> findByPhone(String phone);
}
