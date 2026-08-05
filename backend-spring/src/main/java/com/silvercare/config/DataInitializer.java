package com.silvercare.config;

import org.springframework.boot.CommandLineRunner;
import org.springframework.stereotype.Component;

@Component
public class DataInitializer implements CommandLineRunner {

    @Override
    public void run(String... args) throws Exception {
        System.out.println("🌱 [DATA INITIALIZER] Spring Boot Java backend initialized successfully with persistent H2 storage.");
    }
}
