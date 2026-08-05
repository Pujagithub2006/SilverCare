package com.silvercare.controller;

import com.silvercare.dto.ApiResponse;
import com.silvercare.service.ChatbotService;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.List;
import java.util.Map;

@RestController
@CrossOrigin(originPatterns = "*", allowedHeaders = "*")
public class ChatbotController {

    @Autowired(required = false)
    private ChatbotService chatbotService;

    @RequestMapping(value = {"/chat", "/api/chat", "/chatbot"}, method = {RequestMethod.POST, RequestMethod.GET})
    public ResponseEntity<ApiResponse<Object>> chat(
            @RequestBody(required = false) Map<String, Object> payload,
            @RequestParam(value = "message", required = false) String paramMessage,
            @RequestParam(value = "prompt", required = false) String paramPrompt) {

        String userMessage = "";
        String elderlyId = "default_senior";
        String elderlyName = "";

        List<Map<String, String>> history = null;
        try {
            if (payload != null) {
                if (payload.get("message") != null) userMessage = String.valueOf(payload.get("message"));
                else if (payload.get("prompt") != null) userMessage = String.valueOf(payload.get("prompt"));
                else if (payload.get("text") != null) userMessage = String.valueOf(payload.get("text"));
                else if (payload.get("query") != null) userMessage = String.valueOf(payload.get("query"));

                if (payload.get("elderly_id") != null) elderlyId = String.valueOf(payload.get("elderly_id"));
                else if (payload.get("elderlyId") != null) elderlyId = String.valueOf(payload.get("elderlyId"));

                if (payload.get("elderly_name") != null) elderlyName = String.valueOf(payload.get("elderly_name"));
                else if (payload.get("elderlyName") != null) elderlyName = String.valueOf(payload.get("elderlyName"));
                else if (payload.get("name") != null) elderlyName = String.valueOf(payload.get("name"));

                if (payload.get("history") instanceof List) {
                    history = (List<Map<String, String>>) payload.get("history");
                }
            }

            if ((userMessage == null || userMessage.isBlank()) && paramMessage != null) {
                userMessage = paramMessage;
            }
            if ((userMessage == null || userMessage.isBlank()) && paramPrompt != null) {
                userMessage = paramPrompt;
            }
        } catch (Exception e) {
            System.err.println("Error parsing chat request payload: " + e.getMessage());
        }

        String reply = "I am right here with you! How can I help you feel relaxed and healthy today?";
        try {
            if (chatbotService != null) {
                reply = chatbotService.getChatResponse(userMessage, elderlyId, elderlyName, history);
            }
        } catch (Throwable t) {
            System.err.println("⚠️ Error in ChatbotService: " + t.getMessage());
            t.printStackTrace();
        }

        ApiResponse<Object> response = ApiResponse.builder()
                .status("success")
                .message(reply)
                .reply(reply)
                .build();
        return ResponseEntity.ok(response);
    }
}
