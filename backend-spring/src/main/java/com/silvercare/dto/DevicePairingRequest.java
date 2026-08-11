package com.silvercare.dto;

public class DevicePairingRequest {
    private String macAddress;
    private String deviceType; // Waist Belt, Wrist Band
    private String elderlyId;
    private String guardianUsername;

    public DevicePairingRequest() {}

    public String getMacAddress() { return macAddress; }
    public void setMacAddress(String macAddress) { this.macAddress = macAddress; }

    public String getDeviceType() { return deviceType; }
    public void setDeviceType(String deviceType) { this.deviceType = deviceType; }

    public String getElderlyId() { return elderlyId; }
    public void setElderlyId(String elderlyId) { this.elderlyId = elderlyId; }

    public String getGuardianUsername() { return guardianUsername; }
    public void setGuardianUsername(String guardianUsername) { this.guardianUsername = guardianUsername; }
}
