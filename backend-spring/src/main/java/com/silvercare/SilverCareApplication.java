package com.silvercare;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.scheduling.annotation.EnableScheduling;

@SpringBootApplication
@EnableScheduling
public class SilverCareApplication {

    public static void main(String[] args) {
        SpringApplication.run(SilverCareApplication.class, args);
        System.out.println("🚀 [SERVER] SilverCare Spring Boot Backend initialized successfully on port 5001");
    }
}
