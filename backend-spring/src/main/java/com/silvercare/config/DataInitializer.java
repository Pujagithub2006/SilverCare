package com.silvercare.config;

import org.springframework.boot.CommandLineRunner;
import org.springframework.stereotype.Component;

@Component
public class DataInitializer implements CommandLineRunner {

    @Override
    public void run(String... args) throws Exception {
        System.out.println("🌱 [DATA INITIALIZER] Spring Boot standalone backend ready (independent from legacy backend).");
    }
}
