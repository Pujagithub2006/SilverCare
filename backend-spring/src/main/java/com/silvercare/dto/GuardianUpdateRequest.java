package com.silvercare.dto;

public class GuardianUpdateRequest {
    private String username;
    private String name;
    private String phone;
    private String email;

    public GuardianUpdateRequest() {}

    public GuardianUpdateRequest(String username, String name, String phone, String email) {
        this.username = username;
        this.name = name;
        this.phone = phone;
        this.email = email;
    }

    public String getUsername() { return username; }
    public void setUsername(String username) { this.username = username; }

    public String getName() { return name; }
    public void setName(String name) { this.name = name; }

    public String getPhone() { return phone; }
    public void setPhone(String phone) { this.phone = phone; }

    public String getEmail() { return email; }
    public void setEmail(String email) { this.email = email; }
}
