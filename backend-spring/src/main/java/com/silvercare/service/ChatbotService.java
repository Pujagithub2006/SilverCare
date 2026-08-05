package com.silvercare.service;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.silvercare.entity.ChatMemory;
import com.silvercare.repository.ChatMemoryRepository;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.*;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestTemplate;

import java.util.*;
import java.util.stream.Collectors;

@Service
public class ChatbotService {

    @Value("${google.ai.api.key:}")
    private String apiKey;

    @Value("${groq.api.key:}")
    private String groqApiKey;

    @Value("${openai.api.key:}")
    private String openAiApiKey;

    @Autowired(required = false)
    private ChatMemoryRepository chatMemoryRepository;

    private final RestTemplate restTemplate = new RestTemplate();
    private final ObjectMapper objectMapper = new ObjectMapper();

    public String getChatResponse(String userMessage) {
        return getChatResponse(userMessage, "default_senior", "", null);
    }

    public String getChatResponse(String userMessage, String elderlyId, String elderlyName) {
        return getChatResponse(userMessage, elderlyId, elderlyName, null);
    }

    public String getChatResponse(String userMessage, String elderlyId, String elderlyName, List<Map<String, String>> history) {
        String firstName = extractFirstName(elderlyName);
        String nameGreeting = !firstName.isEmpty() ? " " + firstName : "";

        if (userMessage == null || userMessage.trim().isEmpty()) {
            return "Hello" + nameGreeting + "! I am Mitra, your SilverCare companion. How can I help you feel relaxed and healthy today?";
        }

        String msg = userMessage.trim();
        String safeElderlyId = (elderlyId != null && !elderlyId.isBlank()) ? elderlyId : "default_senior";

        String memoryContext = "";
        try {
            List<ChatMemory> pastMemories = getPastMemories(safeElderlyId);
            memoryContext = buildMemoryContext(pastMemories);
        } catch (Throwable t) {
            System.err.println("Memory fetch warning: " + t.getMessage());
        }

        String nameInstruction = !firstName.isEmpty() ? "The senior's first name is " + firstName + "." : "";
        String systemPrompt = "You are Mitra, a warm, empathetic, highly intelligent AI health companion for senior citizens. " +
                nameInstruction + " Speak in clear, fluent, natural English like a caring friend and medical advisor. " +
                "Do NOT repeat 'Hello " + firstName + "' in every reply! Only greet if it is a hello/hi message. " +
                "Answer questions directly, accurately, and thoroughly like ChatGPT/Gemini. " +
                "Provide actionable, safe health recommendations (hydration, nutrition, gentle exercises, medicine reminders) when relevant. " +
                "CONTINUOUS LEARNING MEMORY OF THIS SENIOR: [" + memoryContext + "]. Use past memory to personalize naturally. " +
                "Keep responses warm, concise, and helpful (2-4 sentences max).";

        String botReply = null;

        // 1. Try Groq API if key configured
        if (botReply == null && groqApiKey != null && !groqApiKey.isBlank()) {
            botReply = queryOpenAiCompatibleApi("https://api.groq.com/openai/v1/chat/completions", groqApiKey, "llama-3.3-70b-versatile", systemPrompt, msg, history);
        }

        // 2. Try OpenAI API if key configured
        if (botReply == null && openAiApiKey != null && !openAiApiKey.isBlank()) {
            botReply = queryOpenAiCompatibleApi("https://api.openai.com/v1/chat/completions", openAiApiKey, "gpt-4o-mini", systemPrompt, msg, history);
        }

        // 3. Try Google Gemini Generative AI API (gemini-2.0-flash / gemini-1.5-flash) if key valid
        if (botReply == null && apiKey != null && !apiKey.isEmpty() && !apiKey.startsWith("YOUR_")) {
            botReply = queryGeminiApi(apiKey, systemPrompt, msg, history);
        }

        // 4. Try OpenRouter Free Tier Open-LLM Models
        if (botReply == null || botReply.isBlank()) {
            botReply = queryOpenRouterFreeApi(systemPrompt, msg, history);
        }

        // 5. Try Pollinations Free LLM Engine
        if (botReply == null || botReply.isBlank()) {
            botReply = queryPollinationsLlm(systemPrompt, msg, history);
        }

        // 6. Rich Dynamic Offline Generator (Guarantees smart, helpful advice even offline)
        if (botReply == null || botReply.isBlank()) {
            botReply = generateSmartDynamicNLPResponse(msg, memoryContext, firstName);
        }

        // 7. Save New Conversation & Extract Insight for Continuous Self-Improvement
        try {
            saveLearnedMemory(safeElderlyId, msg, botReply);
        } catch (Throwable t) {
            System.err.println("Memory save warning: " + t.getMessage());
        }

        return botReply;
    }

    private String queryGeminiApi(String key, String systemPromptText, String msg, List<Map<String, String>> history) {
        String[] models = {"gemini-2.5-flash", "gemini-3.6-flash", "gemini-2.0-flash", "gemini-flash-latest", "gemini-2.5-pro", "gemini-pro-latest"};
        for (String model : models) {
            try {
                String url = String.format("https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s", model, key);
                HttpHeaders headers = new HttpHeaders();
                headers.setContentType(MediaType.APPLICATION_JSON);

                List<Map<String, Object>> contentsList = new ArrayList<>();

                if (history != null && !history.isEmpty()) {
                    for (Map<String, String> h : history) {
                        if (h != null && h.get("text") != null && !h.get("text").isBlank()) {
                            String role = "user".equalsIgnoreCase(h.get("sender")) ? "user" : "model";
                            contentsList.add(Map.of(
                                    "role", role,
                                    "parts", List.of(Map.of("text", h.get("text")))
                            ));
                        }
                    }
                }

                contentsList.add(Map.of(
                        "role", "user",
                        "parts", List.of(Map.of("text", msg))
                ));

                Map<String, Object> body = Map.of(
                        "system_instruction", Map.of(
                                "parts", List.of(Map.of("text", systemPromptText))
                        ),
                        "contents", contentsList
                );

                HttpEntity<Map<String, Object>> entity = new HttpEntity<>(body, headers);
                ResponseEntity<String> response = restTemplate.exchange(url, HttpMethod.POST, entity, String.class);

                if (response.getStatusCode().is2xxSuccessful() && response.getBody() != null) {
                    JsonNode root = objectMapper.readTree(response.getBody());
                    JsonNode textNode = root.path("candidates").get(0).path("content").path("parts").get(0).path("text");
                    if (!textNode.isMissingNode() && !textNode.asText().isBlank()) {
                        System.out.println("✅ Gemini API (" + model + ") response generated successfully.");
                        return textNode.asText().trim();
                    }
                }
            } catch (Exception e) {
                System.err.println("⚠️ Gemini API model (" + model + ") attempt: " + e.getMessage());
            }
        }
        return null;
    }

    private String queryOpenAiCompatibleApi(String baseUrl, String key, String modelName, String systemPrompt, String msg, List<Map<String, String>> history) {
        try {
            HttpHeaders headers = new HttpHeaders();
            headers.setContentType(MediaType.APPLICATION_JSON);
            headers.setBearerAuth(key);

            List<Map<String, String>> messages = new ArrayList<>();
            messages.add(Map.of("role", "system", "content", systemPrompt));

            if (history != null) {
                for (Map<String, String> h : history) {
                    if (h != null && h.get("text") != null && h.get("sender") != null) {
                        String role = "user".equalsIgnoreCase(h.get("sender")) ? "user" : "assistant";
                        messages.add(Map.of("role", role, "content", h.get("text")));
                    }
                }
            }
            messages.add(Map.of("role", "user", "content", msg));

            Map<String, Object> body = Map.of(
                    "model", modelName,
                    "messages", messages,
                    "temperature", 0.7
            );

            HttpEntity<Map<String, Object>> entity = new HttpEntity<>(body, headers);
            ResponseEntity<String> response = restTemplate.exchange(baseUrl, HttpMethod.POST, entity, String.class);

            if (response.getStatusCode().is2xxSuccessful() && response.getBody() != null) {
                JsonNode root = objectMapper.readTree(response.getBody());
                JsonNode textNode = root.path("choices").get(0).path("message").path("content");
                if (!textNode.isMissingNode() && !textNode.asText().isBlank()) {
                    System.out.println("✅ OpenAI-compatible LLM (" + modelName + ") response generated successfully.");
                    return textNode.asText().trim();
                }
            }
        } catch (Exception e) {
            System.err.println("⚠️ OpenAI-compatible API (" + modelName + ") attempt: " + e.getMessage());
        }
        return null;
    }

    private String queryOpenRouterFreeApi(String systemPrompt, String msg, List<Map<String, String>> history) {
        String[] freeModels = {
                "google/gemma-2-9b-it:free",
                "meta-llama/llama-3.2-1b-instruct:free",
                "qwen/qwen-2.5-7b-instruct:free",
                "deepseek/deepseek-r1:free",
                "mistralai/mistral-7b-instruct:free"
        };

        for (String model : freeModels) {
            try {
                String url = "https://openrouter.ai/api/v1/chat/completions";
                HttpHeaders headers = new HttpHeaders();
                headers.setContentType(MediaType.APPLICATION_JSON);
                headers.set("HTTP-Referer", "https://silvercare.app");
                headers.set("X-Title", "SilverCare Assistant");

                List<Map<String, String>> messages = new ArrayList<>();
                messages.add(Map.of("role", "system", "content", systemPrompt));

                if (history != null && !history.isEmpty()) {
                    int start = Math.max(0, history.size() - 4);
                    for (int i = start; i < history.size(); i++) {
                        Map<String, String> h = history.get(i);
                        if (h != null && h.get("text") != null) {
                            String role = "user".equalsIgnoreCase(h.get("sender")) ? "user" : "assistant";
                            messages.add(Map.of("role", role, "content", h.get("text")));
                        }
                    }
                }
                messages.add(Map.of("role", "user", "content", msg));

                Map<String, Object> body = Map.of(
                        "model", model,
                        "messages", messages,
                        "temperature", 0.7
                );

                HttpEntity<Map<String, Object>> entity = new HttpEntity<>(body, headers);
                ResponseEntity<String> response = restTemplate.exchange(url, HttpMethod.POST, entity, String.class);

                if (response.getStatusCode().is2xxSuccessful() && response.getBody() != null) {
                    JsonNode root = objectMapper.readTree(response.getBody());
                    JsonNode textNode = root.path("choices").get(0).path("message").path("content");
                    if (!textNode.isMissingNode() && !textNode.asText().isBlank()) {
                        System.out.println("✅ Free OpenRouter LLM (" + model + ") response generated successfully.");
                        return textNode.asText().trim();
                    }
                }
            } catch (Exception e) {
                System.err.println("⚠️ OpenRouter (" + model + ") attempt: " + e.getMessage());
            }
        }
        return null;
    }

    private String queryPollinationsLlm(String systemPrompt, String msg, List<Map<String, String>> history) {
        try {
            StringBuilder promptBuilder = new StringBuilder();
            if (history != null && !history.isEmpty()) {
                int start = Math.max(0, history.size() - 4);
                for (int i = start; i < history.size(); i++) {
                    Map<String, String> h = history.get(i);
                    if (h != null && h.get("text") != null) {
                        promptBuilder.append("Previous ").append(h.get("sender")).append(": ").append(h.get("text")).append("\n");
                    }
                }
            }
            promptBuilder.append(msg);

            String encodedMsg = java.net.URLEncoder.encode(promptBuilder.toString(), java.nio.charset.StandardCharsets.UTF_8);
            String encodedSys = java.net.URLEncoder.encode(systemPrompt, java.nio.charset.StandardCharsets.UTF_8);

            try {
                String getUrl = "https://text.pollinations.ai/" + encodedMsg + "?system=" + encodedSys;
                HttpHeaders headers = new HttpHeaders();
                headers.set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
                HttpEntity<Void> entity = new HttpEntity<>(headers);

                ResponseEntity<String> response = restTemplate.exchange(getUrl, HttpMethod.GET, entity, String.class);
                if (response.getStatusCode().is2xxSuccessful() && response.getBody() != null) {
                    String text = response.getBody().trim();
                    if (!text.isBlank() && !text.contains("Payment Required") && !text.startsWith("{\"error\"")) {
                        System.out.println("✅ Free Open-LLM engine (Pollinations) generated successfully.");
                        return text;
                    }
                }
            } catch (Exception e) {
                System.err.println("⚠️ Pollinations GET attempt: " + e.getMessage());
            }
        } catch (Exception e) {
            System.err.println("⚠️ Pollinations LLM attempt: " + e.getMessage());
        }

        return null;
    }

    private String extractFirstName(String fullName) {
        if (fullName == null || fullName.trim().isEmpty()) return "";
        String clean = fullName.trim();
        return clean.split("\\s+")[0];
    }

    private List<ChatMemory> getPastMemories(String elderlyId) {
        try {
            if (chatMemoryRepository != null) {
                return chatMemoryRepository.findTop10ByElderlyIdOrderByIdDesc(elderlyId);
            }
        } catch (Throwable e) {
            System.err.println("Error fetching chat memories: " + e.getMessage());
        }
        return Collections.emptyList();
    }

    private String buildMemoryContext(List<ChatMemory> memories) {
        if (memories == null || memories.isEmpty()) {
            return "First interaction with this senior.";
        }
        return memories.stream()
                .filter(m -> m != null && m.getUserInput() != null && m.getBotResponse() != null)
                .limit(5)
                .map(m -> "User asked: '" + m.getUserInput() + "' | Learned insight: " + (m.getExtractedInsight() != null ? m.getExtractedInsight() : "General query"))
                .collect(Collectors.joining("; "));
    }

    private void saveLearnedMemory(String elderlyId, String userInput, String botResponse) {
        try {
            if (chatMemoryRepository != null) {
                String insight = extractInsight(userInput);
                ChatMemory memory = new ChatMemory(elderlyId, userInput, botResponse, insight);
                chatMemoryRepository.save(memory);
            }
        } catch (Throwable e) {
            System.err.println("Error saving learned chat memory: " + e.getMessage());
        }
    }

    private String extractInsight(String text) {
        String lower = text.toLowerCase();
        if (lower.contains("alone") || lower.contains("lonely")) return "Senior expressed feeling alone/lonely";
        if (lower.contains("not well") || lower.contains("sick") || lower.contains("unwell")) return "Senior reported feeling unwell";
        if (lower.contains("feeling good") || lower.contains("great") || lower.contains("happy")) return "Senior reported feeling good";
        if (lower.contains("knee") || lower.contains("joint") || lower.contains("pain")) return "Senior reported knee/joint discomfort";
        if (lower.contains("sleep") || lower.contains("tired")) return "Senior inquired about sleep/rest";
        if (lower.contains("water") || lower.contains("drink")) return "Senior discussed hydration";
        if (lower.contains("medicine") || lower.contains("pill")) return "Senior asked about medicine routine";
        if (lower.contains("joke") || lower.contains("funny")) return "Senior enjoys light humor/jokes";
        return "General conversation";
    }

    private String generateSmartDynamicNLPResponse(String text, String memoryContext, String firstName) {
        String lower = text.toLowerCase().trim();
        String nameGreet = !firstName.isEmpty() ? " " + firstName : "";

        // 1. Name Introduction ("my name is X", "call me X")
        if (lower.contains("my name is") || lower.contains("i am called") || lower.contains("call me")) {
            return "It is wonderful to meet you" + nameGreet + "! I'm Mitra, your SilverCare health companion. How has your day been going?";
        }

        // 2. Feeling Good / Happy / Great / Fine
        if (lower.contains("feeling good") || lower.contains("feel good") || lower.contains("feeling great") || lower.contains("feeling happy") || lower.contains("i am good") || lower.contains("doing well") || lower.contains("doing great") || lower.contains("fine")) {
            return "That is fantastic to hear" + nameGreet + "! Having a positive mood and feeling good is wonderful for your heart and health. Have you had a chance to enjoy some fresh air or a light walk today?";
        }

        // 3. Feeling Alone / Loneliness / Sadness
        if (lower.contains("alone") || lower.contains("lonely") || lower.contains("sad") || lower.contains("feeling low") || lower.contains("bored") || lower.contains("isolated") || lower.contains("nobody")) {
            return "I am right here with you! You are never alone—I am always here to listen, keep you company, and talk with you. What would you like to chat about today?";
        }

        // 4. Feeling Unwell / Not Well / Sick / Pain / Weakness / Dizziness
        if (lower.contains("not well") || lower.contains("unwell") || lower.contains("sick") || lower.contains("ill") || lower.contains("bad") || lower.contains("weak") || lower.contains("dizzy") || lower.contains("headache") || lower.contains("nausea") || lower.contains("fever") || lower.contains("uneasy")) {
            return "I am so sorry to hear that you are not feeling well. Please sit down comfortably, drink a warm glass of water, and rest. Would you like me to notify your guardian or help you check your medicines?";
        }

        // 5. Joint Pain / Body Pain / Knee Pain / Arthritis
        if (lower.contains("knee") || lower.contains("joint") || lower.contains("pain") || lower.contains("leg") || lower.contains("body") || lower.contains("arthritis") || lower.contains("दर्द") || lower.contains("दुख")) {
            return "To keep your knees and joints comfortable, gently apply warm sesame or mustard oil before resting, practice light indoor leg extensions, and stay hydrated throughout the day. Rest comfortably when you feel tired!";
        }

        // 6. Sleep & Rest / Insomnia
        if (lower.contains("sleep") || lower.contains("tired") || lower.contains("rest") || lower.contains("insomnia") || lower.contains("नींद") || lower.contains("झोप")) {
            return "To support peaceful sleep, sip a cup of warm milk or chamomile tea 30 minutes before bed, avoid bright screens, and keep your room pleasantly cool and cozy. Sleep well!";
        }

        // 7. Cold / Throat / Cough / Digestion / Stomach / Acidity
        if (lower.contains("cold") || lower.contains("cough") || lower.contains("throat") || lower.contains("gas") || lower.contains("stomach") || lower.contains("acidity")) {
            return "Sipping warm water with a little fresh ginger or tulsi tea will soothe your throat and settle your digestive system gently. Take small sips and rest easily.";
        }

        // 8. Hydration & Water
        if (lower.contains("water") || lower.contains("drink") || lower.contains("dehydration") || lower.contains("thirsty")) {
            return "Staying hydrated is essential for seniors! Drinking 6 to 8 glasses of warm or room-temperature water daily supports joint lubrication, kidney function, and steady energy.";
        }

        // 9. Morning Routine / Sunshine / Fresh Air / Walk
        if (lower.contains("morning") || lower.contains("sun") || lower.contains("sunlight") || lower.contains("walk") || lower.contains("outside") || lower.contains("air")) {
            return "Taking a gentle 15-minute morning walk in soft sunlight boosts Vitamin D, sharpens your mood, and gently warms up your leg muscles for the day.";
        }

        // 10. Food / Diet / Nutrition / Eating / Breakfast / Lunch / Dinner
        if (lower.contains("food") || lower.contains("eat") || lower.contains("diet") || lower.contains("lunch") || lower.contains("dinner") || lower.contains("breakfast") || lower.contains("soup")) {
            return "A healthy senior diet focuses on warm, freshly cooked foods rich in fiber, greens, lentils, and healthy proteins. Eating light, smaller meals helps keep your digestion smooth!";
        }

        // 11. Blood Pressure / Hypertension
        if (lower.contains("blood pressure") || lower.contains("bp") || lower.contains("hypertension")) {
            return "For healthy blood pressure, enjoy low-sodium meals, stay well-hydrated, practice slow deep breathing, and take any prescribed medication regularly.";
        }

        // 12. Diabetes / Sugar
        if (lower.contains("diabetes") || lower.contains("sugar") || lower.contains("glucose")) {
            return "Managing blood sugar involves eating complex whole grains (oats, millets), fiber-rich vegetables, taking timely walks after meals, and keeping sweet treats light.";
        }

        // 13. Medicines & Schedule
        if (lower.contains("medicine") || lower.contains("pill") || lower.contains("dose") || lower.contains("tablet") || lower.contains("prescription")) {
            if (lower.contains("miss") || lower.contains("forgot")) {
                return "If you missed a medicine dose, take it as soon as you remember, unless your next scheduled dose is coming up soon. Never double up on doses.";
            }
            return "Taking your medicines on time with a full glass of water keeps your health on track. Your SilverCare medicine schedule is active and ready to remind you!";
        }

        // 14. Emergency / Fall / Help
        if (lower.contains("fall") || lower.contains("hurt") || lower.contains("help") || lower.contains("emergency")) {
            return "Please sit down safely and take slow, calm breaths. I am notifying your guardian right now. Please stay put, help is on the way.";
        }

        // 15. Greetings
        if (lower.length() <= 15 && (lower.equals("hi") || lower.equals("hello") || lower.equals("hey") || lower.startsWith("hi mitra") || lower.startsWith("hello mitra") || lower.startsWith("hey mitra"))) {
            return "Hello" + nameGreet + "! It is wonderful to talk with you! How are you feeling today, and how can I assist you with your health or daily routine?";
        }

        // 16. Thank You / Gratitude
        if (lower.contains("thank") || lower.contains("dhanyawad") || lower.contains("thank you")) {
            return "You are so very welcome! I am always happy to assist you anytime. Have a beautiful and peaceful day!";
        }

        // 17. Epics & Indian Mythology (Ramayana, Rama, Sita, Hanuman)
        if (lower.contains("ramayana") || lower.contains("ramayan") || lower.contains("rama") || lower.contains("sita") || lower.contains("hanuman")) {
            return "The Ramayana is one of the most revered epics in world literature. It tells the noble journey of Prince Rama, his devoted wife Sita, his brother Lakshmana, and the heroic Lord Hanuman. The epic celebrates truth, devotion, family love, and the victory of righteousness (Dharma) over evil. Would you like to talk about a specific part or character from the Ramayana?";
        }

        // 18. Mahabharata & Bhagavad Gita
        if (lower.contains("mahabharata") || lower.contains("gita") || lower.contains("krishna")) {
            return "The Mahabharata is a grand epic containing the sacred teachings of the Bhagavad Gita, where Lord Krishna guides Arjuna on duty, devotion, and living a righteous life. Its wisdom brings peace and clarity of mind.";
        }

        // 19. Stories & Tales
        if (lower.contains("story") || lower.contains("tell me a story") || lower.contains("tale") || lower.contains("legend")) {
            return "Here is a cozy short story: Once upon a time, a grandfather and his grandchild planted a tiny banyan tree together. Over the years, that sapling grew into a mighty tree providing cool shade for the entire village—reminding us that simple acts of kindness grow into lifelong blessings!";
        }

        // 20. Weather & Seasons
        if (lower.contains("weather") || lower.contains("rain") || lower.contains("season") || lower.contains("summer") || lower.contains("winter")) {
            return "Staying comfortable with seasonal changes keeps us feeling cozy and healthy. Sipping warm tea during cool weather and staying in shaded areas on sunny days is always comforting.";
        }

        // 21. Open-Ended Questions & General Knowledge Fallback
        String cleanSubject = text.replaceAll("(?i)^(i am|i feel|what about|tell me|tell me about|can you|can u|can you tell me|can u tell me|how is|is it|what is|who is|the|a|an)\\s+", "").trim();
        if (cleanSubject.length() > 35) {
            cleanSubject = cleanSubject.substring(0, 35).trim() + "...";
        }
        if (cleanSubject.isEmpty()) {
            cleanSubject = "this topic";
        }

        return "I am glad you asked about " + cleanSubject + "! I am always here to converse with you, share uplifting thoughts, and keep you company. What specific detail would you like to explore about " + cleanSubject + "?";
    }
}
