package com.silvercare.dto;

public class ElderlyVoiceMessageRequest {
    private String elderlyId;
    private String deviceId;
    private String audioData; // Base64 audio string or text transcript
    private String triggerEvent; // "FALL", "PREFALL", "MANUAL"

    public ElderlyVoiceMessageRequest() {}

    public ElderlyVoiceMessageRequest(String elderlyId, String deviceId, String audioData, String triggerEvent) {
        this.elderlyId = elderlyId;
        this.deviceId = deviceId;
        this.audioData = audioData;
        this.triggerEvent = triggerEvent;
    }

    public String getElderlyId() { return elderlyId; }
    public void setElderlyId(String elderlyId) { this.elderlyId = elderlyId; }

    public String getDeviceId() { return deviceId; }
    public void setDeviceId(String deviceId) { this.deviceId = deviceId; }

    public String getAudioData() { return audioData; }
    public void setAudioData(String audioData) { this.audioData = audioData; }

    public String getTriggerEvent() { return triggerEvent; }
    public void setTriggerEvent(String triggerEvent) { this.triggerEvent = triggerEvent; }
}
