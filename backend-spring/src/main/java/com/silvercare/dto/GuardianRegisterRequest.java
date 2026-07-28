package com.silvercare.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

public class GuardianRegisterRequest {
    private String name;

    @JsonProperty("fullName")
    private String fullName;

    private String username;
    private String password;
    private String phone;
    private String email;
    private String address;
    private String emergencyContact;

    public GuardianRegisterRequest() {}

    public GuardianRegisterRequest(String name, String fullName, String username, String password, String phone, String email, String address, String emergencyContact) {
        this.name = name;
        this.fullName = fullName;
        this.username = username;
        this.password = password;
        this.phone = phone;
        this.email = email;
        this.address = address;
        this.emergencyContact = emergencyContact;
    }

    public String getName() { return name != null ? name : fullName; }
    public void setName(String name) { this.name = name; }

    public String getFullName() { return fullName; }
    public void setFullName(String fullName) { this.fullName = fullName; }

    public String getUsername() { return username; }
    public void setUsername(String username) { this.username = username; }

    public String getPassword() { return password; }
    public void setPassword(String password) { this.password = password; }

    public String getPhone() { return phone; }
    public void setPhone(String phone) { this.phone = phone; }

    public String getEmail() { return email; }
    public void setEmail(String email) { this.email = email; }

    public String getAddress() { return address; }
    public void setAddress(String address) { this.address = address; }

    public String getEmergencyContact() { return emergencyContact; }
    public void setEmergencyContact(String emergencyContact) { this.emergencyContact = emergencyContact; }
}
