package com.silvercare.config;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.silvercare.entity.Elderly;
import com.silvercare.entity.Guardian;
import com.silvercare.entity.GuardianSuggestion;
import com.silvercare.entity.Medicine;
import com.silvercare.repository.ElderlyRepository;
import com.silvercare.repository.GuardianRepository;
import com.silvercare.repository.GuardianSuggestionRepository;
import com.silvercare.repository.MedicineRepository;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.CommandLineRunner;
import org.springframework.stereotype.Component;

import java.io.File;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

@Component
public class DataInitializer implements CommandLineRunner {

    @Autowired
    private GuardianRepository guardianRepository;

    @Autowired
    private ElderlyRepository elderlyRepository;

    @Autowired
    private MedicineRepository medicineRepository;

    @Autowired
    private GuardianSuggestionRepository suggestionRepository;

    private final ObjectMapper objectMapper = new ObjectMapper();

    @Override
    public void run(String... args) throws Exception {
        System.out.println("🌱 [DATA INITIALIZER] Checking legacy data files for seeding...");
        Path rootPath = Paths.get("..", "backend", "data").toAbsolutePath().normalize();
        if (!rootPath.toFile().exists()) {
            rootPath = Paths.get("backend", "data").toAbsolutePath().normalize();
        }

        File guardiansFile = rootPath.resolve("guardians.json").toFile();
        File elderlyFile = rootPath.resolve("elderly.json").toFile();
        File medicinesFile = rootPath.resolve("medicines.json").toFile();

        // 1. Seed Guardians
        if (guardiansFile.exists() && guardiansFile.length() > 0) {
            try {
                JsonNode node = objectMapper.readTree(guardiansFile);
                Iterator<String> fieldNames = node.fieldNames();
                while (fieldNames.hasNext()) {
                    String username = fieldNames.next();
                    if (!guardianRepository.existsByUsername(username)) {
                        JsonNode gNode = node.get(username);
                        List<String> linked = new ArrayList<>();
                        if (gNode.has("elderly_linked")) {
                            for (JsonNode e : gNode.get("elderly_linked")) {
                                linked.add(e.asText());
                            }
                        }
                        Guardian guardian = Guardian.builder()
                                .username(username)
                                .name(gNode.path("name").asText(username))
                                .passwordHash(gNode.path("password_hash").asText(""))
                                .phone(gNode.path("phone").asText(""))
                                .email(gNode.path("email").asText(""))
                                .elderlyLinked(linked)
                                .createdAt(gNode.path("created_at").asText(null))
                                .build();
                        guardianRepository.save(guardian);
                        System.out.println("  └─ Seeded Guardian: " + username);
                    }
                }
            } catch (Exception e) {
                System.err.println("Error seeding guardians: " + e.getMessage());
            }
        }

        // 2. Seed Elderly
        if (elderlyFile.exists() && elderlyFile.length() > 0) {
            try {
                JsonNode node = objectMapper.readTree(elderlyFile);
                Iterator<String> fieldNames = node.fieldNames();
                while (fieldNames.hasNext()) {
                    String elderlyId = fieldNames.next();
                    if ("vois_belt".equalsIgnoreCase(elderlyId)) continue; // skip metadata entries

                    if (elderlyRepository.findByElderlyId(elderlyId).isEmpty()) {
                        JsonNode eNode = node.get(elderlyId);
                        Elderly elderly = Elderly.builder()
                                .elderlyId(elderlyId)
                                .name(eNode.path("name").asText(elderlyId))
                                .age(eNode.path("age").asInt(65))
                                .medicalHistory(eNode.path("medical_history").asText(""))
                                .phone(eNode.path("phone").asText(""))
                                .location(eNode.path("location").asText("Home"))
                                .guardianUsername(eNode.path("guardian_username").asText("john_guardian"))
                                .createdAt(eNode.path("created_at").asText(null))
                                .build();
                        elderlyRepository.save(elderly);
                        System.out.println("  └─ Seeded Elderly: " + elderlyId);
                    }
                }
            } catch (Exception e) {
                System.err.println("Error seeding elderly: " + e.getMessage());
            }
        }

        // 3. Seed Medicines
        if (medicinesFile.exists() && medicinesFile.length() > 0) {
            try {
                JsonNode node = objectMapper.readTree(medicinesFile);
                Iterator<String> fieldNames = node.fieldNames();
                while (fieldNames.hasNext()) {
                    String elderlyId = fieldNames.next();
                    JsonNode listNode = node.get(elderlyId);
                    if (listNode.isArray()) {
                        for (JsonNode mNode : listNode) {
                            String medName = mNode.path("medicine_name").asText();
                            List<String> times = new ArrayList<>();
                            if (mNode.has("times")) {
                                for (JsonNode t : mNode.get("times")) {
                                    times.add(t.asText());
                                }
                            }
                            Medicine medicine = Medicine.builder()
                                    .elderlyId(elderlyId)
                                    .medicineName(medName)
                                    .dosage(mNode.path("dosage").asText(""))
                                    .times(times)
                                    .instructions(mNode.path("instructions").asText(""))
                                    .startDate(mNode.path("start_date").asText(""))
                                    .endDate(mNode.path("end_date").asText(""))
                                    .active(mNode.path("active").asBoolean(true))
                                    .createdAt(mNode.path("created_at").asText(null))
                                    .build();
                            medicineRepository.save(medicine);
                            System.out.println("  └─ Seeded Medicine: " + medName + " for " + elderlyId);
                        }
                    }
                }
            } catch (Exception e) {
                System.err.println("Error seeding medicines: " + e.getMessage());
            }
        }
    }
}
