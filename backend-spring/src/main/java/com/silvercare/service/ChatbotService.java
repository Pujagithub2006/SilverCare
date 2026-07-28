package com.silvercare.service;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.*;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestTemplate;

import java.util.*;

@Service
public class ChatbotService {

    @Value("${google.ai.api.key:YOUR_GEMINI_API_KEY}")
    private String apiKey;

    private final RestTemplate restTemplate = new RestTemplate();
    private final ObjectMapper objectMapper = new ObjectMapper();

    public String getChatResponse(String userMessage) {
        if (userMessage == null || userMessage.trim().isEmpty()) {
            return "I'm right here. How can I help you feel better today?";
        }

        if (apiKey != null && !apiKey.isEmpty() && !apiKey.startsWith("YOUR_")) {
            String[] models = {"gemini-2.5-flash", "gemini-2.0-flash", "gemini-1.5-flash"};
            for (String model : models) {
                try {
                    String url = String.format("https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s", model, apiKey);
                    HttpHeaders headers = new HttpHeaders();
                    headers.setContentType(MediaType.APPLICATION_JSON);

                    Map<String, Object> body = Map.of(
                            "contents", List.of(
                                    Map.of("parts", List.of(
                                            Map.of("text", userMessage)
                                    ))
                            )
                    );

                    HttpEntity<Map<String, Object>> entity = new HttpEntity<>(body, headers);
                    ResponseEntity<String> response = restTemplate.exchange(url, HttpMethod.POST, entity, String.class);

                    if (response.getStatusCode().is2xxSuccessful() && response.getBody() != null) {
                        JsonNode root = objectMapper.readTree(response.getBody());
                        JsonNode textNode = root.path("candidates").get(0).path("content").path("parts").get(0).path("text");
                        if (!textNode.isMissingNode()) {
                            return textNode.asText();
                        }
                    }
                } catch (Exception e) {
                    System.err.println("⚠️ Gemini API model (" + model + ") failed: " + e.getMessage());
                }
            }
        }

        return "I'm right here with you! I am your SilverCare assistant. How can I assist you with your health or medicine schedule today?";
    }
}
