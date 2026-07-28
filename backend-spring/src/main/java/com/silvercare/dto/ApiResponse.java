package com.silvercare.dto;

import com.fasterxml.jackson.annotation.JsonInclude;

@JsonInclude(JsonInclude.Include.NON_NULL)
public class ApiResponse<T> {
    private String status;
    private String message;
    private String username;
    private String name;
    private String phone;
    private String email;
    private String elderly_id;
    private String guardian_username;
    private String guardian_name;
    private String guardian_phone;
    private Boolean fall_detected;
    private String current_user;
    private Object available_users;
    private Object elderly_linked;
    private Object notifications;
    private Object devices;
    private Object current_status;
    private Object hardware_integration;
    private Object endpoints;
    private String version;
    private Integer count;
    private T data;
    private String reply;
    private Object medicine;
    private Object suggestions;
    private String alert_type;

    public ApiResponse() {}

    public ApiResponse(String status, String message, String username, String name, String phone, String email, String elderly_id, String guardian_username, String guardian_name, String guardian_phone, Boolean fall_detected, String current_user, Object available_users, Object elderly_linked, Object notifications, Object devices, Object current_status, Object hardware_integration, Object endpoints, String version, Integer count, T data, String reply, Object medicine, Object suggestions, String alert_type) {
        this.status = status;
        this.message = message;
        this.username = username;
        this.name = name;
        this.phone = phone;
        this.email = email;
        this.elderly_id = elderly_id;
        this.guardian_username = guardian_username;
        this.guardian_name = guardian_name;
        this.guardian_phone = guardian_phone;
        this.fall_detected = fall_detected;
        this.current_user = current_user;
        this.available_users = available_users;
        this.elderly_linked = elderly_linked;
        this.notifications = notifications;
        this.devices = devices;
        this.current_status = current_status;
        this.hardware_integration = hardware_integration;
        this.endpoints = endpoints;
        this.version = version;
        this.count = count;
        this.data = data;
        this.reply = reply;
        this.medicine = medicine;
        this.suggestions = suggestions;
        this.alert_type = alert_type;
    }

    public static <T> ApiResponse<T> success(String message) {
        return ApiResponse.<T>builder().status("success").message(message).build();
    }

    public static <T> ApiResponse<T> success(String message, T data) {
        return ApiResponse.<T>builder().status("success").message(message).data(data).build();
    }

    public static <T> ApiResponse<T> error(String message) {
        return ApiResponse.<T>builder().status("error").message(message).build();
    }

    public String getStatus() { return status; }
    public void setStatus(String status) { this.status = status; }

    public String getMessage() { return message; }
    public void setMessage(String message) { this.message = message; }

    public String getUsername() { return username; }
    public void setUsername(String username) { this.username = username; }

    public String getName() { return name; }
    public void setName(String name) { this.name = name; }

    public String getPhone() { return phone; }
    public void setPhone(String phone) { this.phone = phone; }

    public String getEmail() { return email; }
    public void setEmail(String email) { this.email = email; }

    public String getElderly_id() { return elderly_id; }
    public void setElderly_id(String elderly_id) { this.elderly_id = elderly_id; }

    public String getGuardian_username() { return guardian_username; }
    public void setGuardian_username(String guardian_username) { this.guardian_username = guardian_username; }

    public String getGuardian_name() { return guardian_name; }
    public void setGuardian_name(String guardian_name) { this.guardian_name = guardian_name; }

    public String getGuardian_phone() { return guardian_phone; }
    public void setGuardian_phone(String guardian_phone) { this.guardian_phone = guardian_phone; }

    public Boolean getFall_detected() { return fall_detected; }
    public void setFall_detected(Boolean fall_detected) { this.fall_detected = fall_detected; }

    public String getCurrent_user() { return current_user; }
    public void setCurrent_user(String current_user) { this.current_user = current_user; }

    public Object getAvailable_users() { return available_users; }
    public void setAvailable_users(Object available_users) { this.available_users = available_users; }

    public Object getElderly_linked() { return elderly_linked; }
    public void setElderly_linked(Object elderly_linked) { this.elderly_linked = elderly_linked; }

    public Object getNotifications() { return notifications; }
    public void setNotifications(Object notifications) { this.notifications = notifications; }

    public Object getDevices() { return devices; }
    public void setDevices(Object devices) { this.devices = devices; }

    public Object getCurrent_status() { return current_status; }
    public void setCurrent_status(Object current_status) { this.current_status = current_status; }

    public Object getHardware_integration() { return hardware_integration; }
    public void setHardware_integration(Object hardware_integration) { this.hardware_integration = hardware_integration; }

    public Object getEndpoints() { return endpoints; }
    public void setEndpoints(Object endpoints) { this.endpoints = endpoints; }

    public String getVersion() { return version; }
    public void setVersion(String version) { this.version = version; }

    public Integer getCount() { return count; }
    public void setCount(Integer count) { this.count = count; }

    public T getData() { return data; }
    public void setData(T data) { this.data = data; }

    public String getReply() { return reply; }
    public void setReply(String reply) { this.reply = reply; }

    public Object getMedicine() { return medicine; }
    public void setMedicine(Object medicine) { this.medicine = medicine; }

    public Object getSuggestions() { return suggestions; }
    public void setSuggestions(Object suggestions) { this.suggestions = suggestions; }

    public String getAlert_type() { return alert_type; }
    public void setAlert_type(String alert_type) { this.alert_type = alert_type; }

    public static <T> ApiResponseBuilder<T> builder() { return new ApiResponseBuilder<>(); }

    public static class ApiResponseBuilder<T> {
        private String status;
        private String message;
        private String username;
        private String name;
        private String phone;
        private String email;
        private String elderly_id;
        private String guardian_username;
        private String guardian_name;
        private String guardian_phone;
        private Boolean fall_detected;
        private String current_user;
        private Object available_users;
        private Object elderly_linked;
        private Object notifications;
        private Object devices;
        private Object current_status;
        private Object hardware_integration;
        private Object endpoints;
        private String version;
        private Integer count;
        private T data;
        private String reply;
        private Object medicine;
        private Object suggestions;
        private String alert_type;

        public ApiResponseBuilder<T> status(String status) { this.status = status; return this; }
        public ApiResponseBuilder<T> message(String message) { this.message = message; return this; }
        public ApiResponseBuilder<T> username(String username) { this.username = username; return this; }
        public ApiResponseBuilder<T> name(String name) { this.name = name; return this; }
        public ApiResponseBuilder<T> phone(String phone) { this.phone = phone; return this; }
        public ApiResponseBuilder<T> email(String email) { this.email = email; return this; }
        public ApiResponseBuilder<T> elderly_id(String elderly_id) { this.elderly_id = elderly_id; return this; }
        public ApiResponseBuilder<T> guardian_username(String guardian_username) { this.guardian_username = guardian_username; return this; }
        public ApiResponseBuilder<T> guardian_name(String guardian_name) { this.guardian_name = guardian_name; return this; }
        public ApiResponseBuilder<T> guardian_phone(String guardian_phone) { this.guardian_phone = guardian_phone; return this; }
        public ApiResponseBuilder<T> fall_detected(Boolean fall_detected) { this.fall_detected = fall_detected; return this; }
        public ApiResponseBuilder<T> current_user(String current_user) { this.current_user = current_user; return this; }
        public ApiResponseBuilder<T> available_users(Object available_users) { this.available_users = available_users; return this; }
        public ApiResponseBuilder<T> elderly_linked(Object elderly_linked) { this.elderly_linked = elderly_linked; return this; }
        public ApiResponseBuilder<T> notifications(Object notifications) { this.notifications = notifications; return this; }
        public ApiResponseBuilder<T> devices(Object devices) { this.devices = devices; return this; }
        public ApiResponseBuilder<T> current_status(Object current_status) { this.current_status = current_status; return this; }
        public ApiResponseBuilder<T> hardware_integration(Object hardware_integration) { this.hardware_integration = hardware_integration; return this; }
        public ApiResponseBuilder<T> endpoints(Object endpoints) { this.endpoints = endpoints; return this; }
        public ApiResponseBuilder<T> version(String version) { this.version = version; return this; }
        public ApiResponseBuilder<T> count(Integer count) { this.count = count; return this; }
        public ApiResponseBuilder<T> data(T data) { this.data = data; return this; }
        public ApiResponseBuilder<T> reply(String reply) { this.reply = reply; return this; }
        public ApiResponseBuilder<T> medicine(Object medicine) { this.medicine = medicine; return this; }
        public ApiResponseBuilder<T> suggestions(Object suggestions) { this.suggestions = suggestions; return this; }
        public ApiResponseBuilder<T> alert_type(String alert_type) { this.alert_type = alert_type; return this; }

        public ApiResponse<T> build() {
            return new ApiResponse<>(status, message, username, name, phone, email, elderly_id, guardian_username, guardian_name, guardian_phone, fall_detected, current_user, available_users, elderly_linked, notifications, devices, current_status, hardware_integration, endpoints, version, count, data, reply, medicine, suggestions, alert_type);
        }
    }
}
