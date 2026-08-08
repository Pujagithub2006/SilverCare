package com.silvercare.repository;

import com.silvercare.entity.Device;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.List;
import java.util.Optional;

@Repository
public interface DeviceRepository extends JpaRepository<Device, String> {
    
    Optional<Device> findByDeviceId(String deviceId);
    
    List<Device> findByDeviceType(String deviceType);
    
    List<Device> findByAssignedElderlyId(String elderlyId);
    
    List<Device> findByStatus(String status);
    
    Optional<Device> findByMacAddress(String macAddress);
    
    List<Device> findByAssignedElderlyIdIsNull();
    
    boolean existsByDeviceId(String deviceId);
}
