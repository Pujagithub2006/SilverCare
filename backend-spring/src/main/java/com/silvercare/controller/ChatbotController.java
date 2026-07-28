package com.silvercare.controller;

import com.silvercare.dto.ApiResponse;
import com.silvercare.dto.ChatRequest;
import com.silvercare.service.ChatbotService;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

@RestController
public class ChatbotController {

    @Autowired
    private ChatbotService chatbotService;

    @PostMapping("/chat")
    public ResponseEntity<ApiResponse<Object>> chat(@RequestBody ChatRequest request) {
        String reply = chatbotService.getChatResponse(request != null ? request.getMessage() : "");
        ApiResponse<Object> response = ApiResponse.builder()
                .reply(reply)
                .build();
        return ResponseEntity.ok(response);
    }
}
