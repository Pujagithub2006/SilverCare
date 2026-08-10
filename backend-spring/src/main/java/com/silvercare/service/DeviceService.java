package com.silvercare.service;

import com.silvercare.entity.Device;
import com.silvercare.entity.Elderly;
import com.silvercare.repository.DeviceRepository;
import com.silvercare.repository.ElderlyRepository;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.LocalDateTime;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;

@Service
public class DeviceService {

    @Autowired
    private DeviceRepository deviceRepository;

    @Autowired
    private ElderlyRepository elderlyRepository;
    
    private static final int HEARTBEAT_TIMEOUT_MINUTES = 30; // Device considered broken after 30 minutes without heartbeat

    public Device registerDevice(Device device) {
        if (deviceRepository.existsByDeviceId(device.getDeviceId())) {
            throw new RuntimeException("Device with ID " + device.getDeviceId() + " already exists");
        }
        return deviceRepository.save(device);
    }

    public Optional<Device> getDevice(String deviceId) {
        return deviceRepository.findByDeviceId(deviceId);
    }

    public Optional<Device> getDeviceByMac(String macAddress) {
        return deviceRepository.findByMacAddress(macAddress);
    }

    public List<Device> getAllDevices() {
        return deviceRepository.findAll();
    }

    public List<Device> getUnassignedDevices() {
        return deviceRepository.findByAssignedElderlyIdIsNull();
    }

    public List<Device> getDevicesByElderly(String elderlyId) {
        return deviceRepository.findByAssignedElderlyId(elderlyId);
    }

    public Map<String, List<Device>> getElderlyDevicesStructured(String elderlyId) {
        Map<String, List<Device>> result = new HashMap<>();
        List<Device> currentDevices = deviceRepository.findByAssignedElderlyId(elderlyId);
        List<Device> previousDevices = deviceRepository.findAll().stream()
            .filter(d -> d.getAssignedElderlyId() == null || !d.getAssignedElderlyId().equals(elderlyId))
            .filter(d -> d.getPreviouslyAssignedTo() != null && d.getPreviouslyAssignedTo().contains(elderlyId))
            .toList();

        result.put("currentDevices", currentDevices != null ? currentDevices : new ArrayList<>());
        result.put("previousDevices", previousDevices != null ? previousDevices : new ArrayList<>());
        return result;
    }

    @Transactional
    public Device pairDeviceToElderly(String macAddress, String elderlyId, String deviceType) {
        if (macAddress == null || macAddress.trim().isEmpty()) {
            throw new RuntimeException("MAC address is required for pairing");
        }

        String cleanMac = macAddress.trim();
        Optional<Elderly> elderlyOpt = elderlyRepository.findById(elderlyId);
        if (elderlyOpt.isEmpty()) {
            throw new RuntimeException("Elderly not found with ID: " + elderlyId);
        }
        Elderly elderly = elderlyOpt.get();

        // Search existing device by MAC or deviceId
        Optional<Device> deviceOpt = deviceRepository.findByMacAddress(cleanMac);
        if (deviceOpt.isEmpty()) {
            deviceOpt = deviceRepository.findByDeviceId(cleanMac);
        }

        Device device;
        if (deviceOpt.isPresent()) {
            device = deviceOpt.get();
        } else {
            // Auto-register new device
            String devId = "DEV_" + cleanMac.replaceAll("[^a-zA-Z0-9]", "").toUpperCase();
            device = new Device();
            device.setDeviceId(devId);
            device.setMacAddress(cleanMac);
            device.setRegisteredAt(LocalDateTime.now());
        }

        if (deviceType != null && !deviceType.trim().isEmpty()) {
            device.setDeviceType(deviceType);
        } else if (device.getDeviceType() == null) {
            device.setDeviceType("Waist Belt");
        }

        // Maintain history if previously assigned elsewhere
        if (device.getAssignedElderlyId() != null && !device.getAssignedElderlyId().equals(elderlyId)) {
            String prev = device.getPreviouslyAssignedTo();
            device.setPreviouslyAssignedTo(prev != null ? prev + "," + device.getAssignedElderlyId() : device.getAssignedElderlyId());
        }

        device.setAssignedElderlyId(elderlyId);
        device.setStatus("ACTIVE");
        device.setPairingDate(LocalDateTime.now());
        device.setLastSeen(LocalDateTime.now());

        // Update Elderly's primary device info
        elderly.setPrimaryDeviceId(device.getDeviceId());
        elderly.setPrimaryBeltType(device.getDeviceType());
        elderlyRepository.save(elderly);

        return deviceRepository.save(device);
    }

    @Transactional
    public Device unpairDeviceWithReason(String deviceId, String elderlyId, String reason) {
        Optional<Device> deviceOpt = deviceRepository.findByDeviceId(deviceId);
        if (deviceOpt.isEmpty()) {
            deviceOpt = deviceRepository.findByMacAddress(deviceId);
        }
        if (deviceOpt.isEmpty()) {
            throw new RuntimeException("Device not found: " + deviceId);
        }

        Device device = deviceOpt.get();
        String assignedElderly = device.getAssignedElderlyId() != null ? device.getAssignedElderlyId() : elderlyId;

        if (assignedElderly != null) {
            String prev = device.getPreviouslyAssignedTo();
            device.setPreviouslyAssignedTo(prev != null ? prev + "," + assignedElderly : assignedElderly);

            Optional<Elderly> elderlyOpt = elderlyRepository.findById(assignedElderly);
            if (elderlyOpt.isPresent()) {
                Elderly elderly = elderlyOpt.get();
                if (device.getDeviceId().equals(elderly.getPrimaryDeviceId())) {
                    elderly.setPrimaryDeviceId(null);
                    elderlyRepository.save(elderly);
                }
            }
        }

        device.setAssignedElderlyId(null);
        device.setStatus("UNASSIGNED");
        device.setUnpairingDate(LocalDateTime.now());
        device.setUnpairingReason(reason != null ? reason : "OTHER");

        return deviceRepository.save(device);
    }

    @Transactional
    public Device assignDeviceToElderly(String deviceId, String elderlyId) {
        Optional<Device> deviceOpt = deviceRepository.findByDeviceId(deviceId);
        if (deviceOpt.isEmpty()) {
            throw new RuntimeException("Device not found: " + deviceId);
        }

        Optional<Elderly> elderlyOpt = elderlyRepository.findById(elderlyId);
        if (elderlyOpt.isEmpty()) {
            throw new RuntimeException("Elderly not found: " + elderlyId);
        }

        Device device = deviceOpt.get();
        Elderly elderly = elderlyOpt.get();

        // Unassign device from previous elderly if any
        if (device.getAssignedElderlyId() != null) {
            Optional<Elderly> prevElderly = elderlyRepository.findById(device.getAssignedElderlyId());
            if (prevElderly.isPresent() && prevElderly.get().getPrimaryDeviceId() != null 
                && prevElderly.get().getPrimaryDeviceId().equals(deviceId)) {
                prevElderly.get().setPrimaryDeviceId(null);
                elderlyRepository.save(prevElderly.get());
            }
        }

        // Assign device to new elderly
        device.setAssignedElderlyId(elderlyId);
        device.setStatus("ACTIVE");
        device.setLastSeen(LocalDateTime.now());
        
        // Update elderly's primary device
        elderly.setPrimaryDeviceId(deviceId);
        elderly.setPrimaryBeltType(device.getDeviceType());
        elderlyRepository.save(elderly);

        return deviceRepository.save(device);
    }

    @Transactional
    public Device unassignDevice(String deviceId) {
        Optional<Device> deviceOpt = deviceRepository.findByDeviceId(deviceId);
        if (deviceOpt.isEmpty()) {
            throw new RuntimeException("Device not found: " + deviceId);
        }

        Device device = deviceOpt.get();
        
        // Remove from elderly if assigned
        if (device.getAssignedElderlyId() != null) {
            Optional<Elderly> elderlyOpt = elderlyRepository.findById(device.getAssignedElderlyId());
            if (elderlyOpt.isPresent()) {
                Elderly elderly = elderlyOpt.get();
                if (device.getDeviceId().equals(elderly.getPrimaryDeviceId())) {
                    elderly.setPrimaryDeviceId(null);
                    elderlyRepository.save(elderly);
                }
            }
        }

        device.setAssignedElderlyId(null);
        device.setStatus("UNASSIGNED");
        return deviceRepository.save(device);
    }

    @Transactional
    public Device markDeviceBroken(String deviceId, String notes) {
        Optional<Device> deviceOpt = deviceRepository.findByDeviceId(deviceId);
        if (deviceOpt.isEmpty()) {
            throw new RuntimeException("Device not found: " + deviceId);
        }

        Device device = deviceOpt.get();
        device.setStatus("BROKEN");
        device.setNotes(notes);
        
        // Unassign from elderly if broken
        if (device.getAssignedElderlyId() != null) {
            Optional<Elderly> elderlyOpt = elderlyRepository.findById(device.getAssignedElderlyId());
            if (elderlyOpt.isPresent()) {
                Elderly elderly = elderlyOpt.get();
                if (device.getDeviceId().equals(elderly.getPrimaryDeviceId())) {
                    elderly.setPrimaryDeviceId(null);
                    elderlyRepository.save(elderly);
                }
            }
        }
        
        device.setAssignedElderlyId(null);
        return deviceRepository.save(device);
    }

    @Transactional
    public Device replaceDevice(String oldDeviceId, String newDeviceId) {
        Optional<Device> oldDeviceOpt = deviceRepository.findByDeviceId(oldDeviceId);
        Optional<Device> newDeviceOpt = deviceRepository.findByDeviceId(newDeviceId);

        if (oldDeviceOpt.isEmpty()) {
            throw new RuntimeException("Old device not found: " + oldDeviceId);
        }
        if (newDeviceOpt.isEmpty()) {
            throw new RuntimeException("New device not found: " + newDeviceId);
        }

        Device oldDevice = oldDeviceOpt.get();
        Device newDevice = newDeviceOpt.get();

        if (oldDevice.getAssignedElderlyId() == null) {
            throw new RuntimeException("Old device is not assigned to any elderly");
        }

        String elderlyId = oldDevice.getAssignedElderlyId();

        // Mark old device as broken/retired
        oldDevice.setStatus("RETIRED");
        oldDevice.setAssignedElderlyId(null);
        deviceRepository.save(oldDevice);

        // Assign new device to the same elderly
        return assignDeviceToElderly(newDeviceId, elderlyId);
    }

    public Device updateLastSeen(String deviceId) {
        Optional<Device> deviceOpt = deviceRepository.findByDeviceId(deviceId);
        if (deviceOpt.isEmpty()) {
            throw new RuntimeException("Device not found: " + deviceId);
        }

        Device device = deviceOpt.get();
        device.setLastSeen(LocalDateTime.now());
        return deviceRepository.save(device);
    }

    public Device updateDevice(String deviceId, Device deviceUpdates) {
        Optional<Device> deviceOpt = deviceRepository.findByDeviceId(deviceId);
        if (deviceOpt.isEmpty()) {
            throw new RuntimeException("Device not found: " + deviceId);
        }

        Device device = deviceOpt.get();
        
        if (deviceUpdates.getFirmwareVersion() != null) {
            device.setFirmwareVersion(deviceUpdates.getFirmwareVersion());
        }
        if (deviceUpdates.getBatteryLevel() != null) {
            device.setBatteryLevel(deviceUpdates.getBatteryLevel());
        }
        if (deviceUpdates.getNotes() != null) {
            device.setNotes(deviceUpdates.getNotes());
        }
        if (deviceUpdates.getStatus() != null) {
            device.setStatus(deviceUpdates.getStatus());
        }

        return deviceRepository.save(device);
    }

    public void deleteDevice(String deviceId) {
        if (!deviceRepository.existsByDeviceId(deviceId)) {
            throw new RuntimeException("Device not found: " + deviceId);
        }
        deviceRepository.deleteById(deviceId);
    }
    
    @Scheduled(fixedRate = 300000) // Run every 5 minutes
    @Transactional
    public void checkDeviceHeartbeats() {
        LocalDateTime timeoutThreshold = LocalDateTime.now().minusMinutes(HEARTBEAT_TIMEOUT_MINUTES);
        
        // Find devices that haven't sent heartbeat recently
        List<Device> inactiveDevices = deviceRepository.findAll().stream()
            .filter(device -> device.getStatus().equals("ACTIVE"))
            .filter(device -> device.getLastSeen() != null)
            .filter(device -> device.getLastSeen().isBefore(timeoutThreshold))
            .toList();
        
        for (Device device : inactiveDevices) {
            System.out.println("⚠️ Device " + device.getDeviceId() + " has not sent heartbeat, marking as potentially broken");
            
            // Mark as broken and unassign from elderly
            device.setStatus("BROKEN");
            device.setNotes("Auto-marked as broken due to heartbeat timeout");
            
            if (device.getAssignedElderlyId() != null) {
                Optional<Elderly> elderlyOpt = elderlyRepository.findById(device.getAssignedElderlyId());
                if (elderlyOpt.isPresent()) {
                    Elderly elderly = elderlyOpt.get();
                    if (device.getDeviceId().equals(elderly.getPrimaryDeviceId())) {
                        elderly.setPrimaryDeviceId(null);
                        elderlyRepository.save(elderly);
                    }
                }
            }
            
            device.setAssignedElderlyId(null);
            deviceRepository.save(device);
        }
    }
    
    public List<Device> getPotentiallyBrokenDevices() {
        LocalDateTime timeoutThreshold = LocalDateTime.now().minusMinutes(HEARTBEAT_TIMEOUT_MINUTES);
        
        return deviceRepository.findAll().stream()
            .filter(device -> device.getStatus().equals("ACTIVE"))
            .filter(device -> device.getLastSeen() != null)
            .filter(device -> device.getLastSeen().isBefore(timeoutThreshold))
            .toList();
    }
}
