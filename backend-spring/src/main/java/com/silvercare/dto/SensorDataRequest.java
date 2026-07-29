package com.silvercare.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public class SensorDataRequest {
    @JsonProperty("deviceId")
    private String deviceId;

    private Integer state;

    @JsonProperty("stateName")
    private String stateName;

    @JsonProperty("heartRate")
    private Double heartRate;

    private Double spo2;
    private Double temperature;

    @JsonProperty("beltWorn")
    private Boolean beltWorn;

    @JsonProperty("beltType")
    private String beltType; // Waist Belt vs Wrist Belt

    private Double acceleration;
    private Long timestamp;

    private Double latitude;
    private Double longitude;

    @JsonProperty("micMessageAudio")
    private String micMessageAudio; // Base64 or voice message

    public SensorDataRequest() {}

    public SensorDataRequest(String deviceId, Integer state, String stateName, Double heartRate, Double spo2, Double temperature, Boolean beltWorn, Double acceleration, Long timestamp) {
        this.deviceId = deviceId;
        this.state = state;
        this.stateName = stateName;
        this.heartRate = heartRate;
        this.spo2 = spo2;
        this.temperature = temperature;
        this.beltWorn = beltWorn;
        this.acceleration = acceleration;
        this.timestamp = timestamp;
    }

    public String getDeviceId() { return deviceId; }
    public void setDeviceId(String deviceId) { this.deviceId = deviceId; }

    public Integer getState() { return state; }
    public void setState(Integer state) { this.state = state; }

    public String getStateName() { return stateName; }
    public void setStateName(String stateName) { this.stateName = stateName; }

    public Double getHeartRate() { return heartRate; }
    public void setHeartRate(Double heartRate) { this.heartRate = heartRate; }

    public Double getSpo2() { return spo2; }
    public void setSpo2(Double spo2) { this.spo2 = spo2; }

    public Double getTemperature() { return temperature; }
    public void setTemperature(Double temperature) { this.temperature = temperature; }

    public Boolean getBeltWorn() { return beltWorn; }
    public void setBeltWorn(Boolean beltWorn) { this.beltWorn = beltWorn; }

    public String getBeltType() { return beltType; }
    public void setBeltType(String beltType) { this.beltType = beltType; }

    public Double getAcceleration() { return acceleration; }
    public void setAcceleration(Double acceleration) { this.acceleration = acceleration; }

    public Long getTimestamp() { return timestamp; }
    public void setTimestamp(Long timestamp) { this.timestamp = timestamp; }

    public Double getLatitude() { return latitude; }
    public void setLatitude(Double latitude) { this.latitude = latitude; }

    public Double getLongitude() { return longitude; }
    public void setLongitude(Double longitude) { this.longitude = longitude; }

    public String getMicMessageAudio() { return micMessageAudio; }
    public void setMicMessageAudio(String micMessageAudio) { this.micMessageAudio = micMessageAudio; }
}
