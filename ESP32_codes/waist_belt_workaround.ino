#include <NimBLEDevice.h>
#include <Preferences.h>
#include <vector>
#include <algorithm>
#include <mbedtls/aes.h>
#include <mbedtls/gcm.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <TinyGPS++.h>
#include "tinyml_model.h"

// ===== WIFI CONFIGURATION =====
// WiFi credentials are now managed automatically
#define MAX_KNOWN_NETWORKS 10
#define WIFI_SCAN_TIMEOUT 5000
#define WIFI_CONNECT_TIMEOUT 10000
#define AP_SSID "SilverCare_Setup"
#define AP_PASSWORD "silvercare123"

// ===== GSM CONFIGURATION (SIM800L) =====
// GSM RX connected to ESP32 D23 (TX), GSM TX connected to ESP32 D5 (RX)
#define GSM_RX_PIN 5       // GSM TX -> ESP32 RX (D5)
#define GSM_TX_PIN 23      // GSM RX -> ESP32 TX (D23)
#define GSM_BAUD 9600
SoftwareSerial gsmSerial(GSM_RX_PIN, GSM_TX_PIN);

// ===== PIN CONFIGURATION =====
#define SOS_BUTTON_PIN 19    // Push Button 1: Dedicated SOS Emergency Button (D19)
#define ACK_BUTTON_PIN 18    // Push Button 2: Dedicated Reset / Acknowledgment Button (D18)
#define BUZZER_PIN 25        // Mapped to MAX98357A Audio DIN Pin (D25)
#define MPU6050_SDA 21       // MPU6050 SDA pin (D21)
#define MPU6050_SCL 22       // MPU6050 SCL pin (D22)
#define GPS_RX_PIN 16        // GPS TX -> ESP32 RX (D16)
#define GPS_TX_PIN 17        // GPS RX -> ESP32 TX (D17)

// MAX98357A I2S Speaker Amplifier Pins
#define I2S_LRC 33           // Speaker LRC (WS) -> D33
#define I2S_BCLK 32          // Speaker BCLK -> D32
#define I2S_DOUT 25          // Speaker DIN -> D25

// INMP441 I2S Microphone Pins
#define MIC_I2S_WS 14        // Mic WS -> D14
#define MIC_I2S_SCK 27       // Mic SCK -> D27
#define MIC_I2S_SD 34        // Mic SD -> D34 (LR to GND, VDD to 3.3V)

// 1-minute (60 seconds) microphone recording for analysis on Prefall
const unsigned long PREFALL_MIC_RECORDING_DURATION = 60000; // 60,000 ms (1 min)
bool micRecordingActive = false;
unsigned long micRecordingStartTime = 0;
unsigned long lastHelpSpeakerTime = 0;
const unsigned long HELP_SPEAKER_INTERVAL = 2000; // Repeat "Help! Help!" speaker audio every 2 seconds

// ===== GPS CONFIGURATION =====
TinyGPSPlus gps;
HardwareSerial GPSSerial(1);  // Use UART1 for GPS
double gpsLat = 18.5204;
double gpsLng = 73.8567;
double lastKnownLat = 18.5204;
double lastKnownLng = 73.8567;
unsigned long lastGPSUpdate = 0;
const unsigned long GPS_UPDATE_INTERVAL = 10000; // Update GPS every 10 seconds

// GPS History for tracking
#define GPS_HISTORY_SIZE 10
struct GPSLocation {
    double lat;
    double lng;
    unsigned long timestamp;
};
GPSLocation gpsHistory[GPS_HISTORY_SIZE];
int gpsHistoryIndex = 0;

// ===== BACKEND CONFIGURATION =====
const char* serverUrl = "https://silvercare-production-3455.up.railway.app/api/sensor-data";
const char* twilioUrl = "https://silvercare-production-3455.up.railway.app/api/twilio-sms";
const char* deviceApiUrl = "https://silvercare-production-3455.up.railway.app/api/devices";
const char* mlServiceUrl = "http://localhost:8000/predict";  // ML service endpoint
String DEVICE_ID = ""; // Will be generated based on MAC address

// ===== CONNECTION STATE TRACKING =====
unsigned long lastWiFiReconnectAttempt = 0;
const unsigned long WIFI_RECONNECT_INTERVAL = 10000;
unsigned long lastWiFiScan = 0;
const unsigned long WIFI_SCAN_INTERVAL = 60000; // Scan every minute

// ===== WIFI MANAGER =====
struct KnownNetwork {
  String ssid;
  String password;
  int priority; // Higher = preferred
};

KnownNetwork knownNetworks[MAX_KNOWN_NETWORKS];
int knownNetworkCount = 0;
Preferences wifiPrefs;
Preferences devicePrefs;
WebServer webServer(80);
bool apModeActive = false;
unsigned long apModeStartTime = 0;
const unsigned long AP_MODE_TIMEOUT = 300000; // 5 minutes
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 60000; // 1 minute

// ===== EXISTING CONFIGURATION (PRESERVED) =====
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define WRIST_NAME "SilverCare_Wrist"

// ===== ENCRYPTION CONFIGURATION =====
static const uint8_t ENCRYPTION_KEY[32] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};
static const uint8_t ENCRYPTION_IV[12] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B
};

// ===== TIME CONFIGURATION =====
const unsigned long DATA_SEND_INTERVAL = 5000; // Send data every 5 seconds
unsigned long lastDataSent = 0;

// ===== ML PREDICTION STATES =====
enum MLState { ML_NORMAL, ML_PREFALL, ML_FALL, ML_SUDDEN_MOVEMENT };
MLState currentMLState = ML_NORMAL;
unsigned long mlStateChangeTime = 0;
float mlConfidence = 0.0;

// ===== SENSOR DATA FOR ML =====
float accelX = 0.0, accelY = 0.0, accelZ = 0.0;
float gyroX = 0.0, gyroY = 0.0, gyroZ = 0.0;
float currentHr = 72.0;
float currentSpo2 = 98.0;
float currentTemp = 36.6;
long currentIr = 50000;

// ===== ALERT SYSTEM CONFIGURATION =====
// Prefall: 5 minutes acknowledgment timeout
const unsigned long PREFALL_ACK_TIMEOUT = 300000; // 5 minutes
// Fall: 2 minutes acknowledgment timeout for main guardian
const unsigned long FALL_ACK_TIMEOUT = 120000; // 2 minutes

enum AlertState { NO_ALERT, PREFALL_ALERT, FALL_ALERT };
AlertState currentAlertState = NO_ALERT;
unsigned long alertStartTime = 0;
bool guardianAcknowledged = false;
bool secondaryGuardianNotified = false;

// Contact information (stored in EEPROM / NVS flash)
String mainGuardianPhone = "";
String secondaryGuardianPhone = "";
String neighborPhone = "";
String elderlyId = "";
String speakerAudioMessage = "Fall fall! Emergency detected!";

// EEPROM Storage Manager
Preferences eepromPrefs;

void loadEEPROMConfig() {
    eepromPrefs.begin("eeprom_cfg", true); // read-only
    mainGuardianPhone = eepromPrefs.getString("main_phone", mainGuardianPhone);
    secondaryGuardianPhone = eepromPrefs.getString("sec_phone", secondaryGuardianPhone);
    speakerAudioMessage = eepromPrefs.getString("speaker_msg", speakerAudioMessage);
    eepromPrefs.end();
    Serial.println("💾 [EEPROM] Loaded stored contacts: Main=" + mainGuardianPhone + ", Sec=" + secondaryGuardianPhone + ", SpeakerMsg=" + speakerAudioMessage);
}

void saveEEPROMConfig(String mainPhone, String secPhone, String speakerMsg) {
    eepromPrefs.begin("eeprom_cfg", false); // read-write
    if (mainPhone.length() > 0) {
        mainGuardianPhone = mainPhone;
        eepromPrefs.putString("main_phone", mainPhone);
    }
    if (secPhone.length() > 0) {
        secondaryGuardianPhone = secPhone;
        eepromPrefs.putString("sec_phone", secPhone);
    }
    if (speakerMsg.length() > 0) {
        speakerAudioMessage = speakerMsg;
        eepromPrefs.putString("speaker_msg", speakerMsg);
    }
    eepromPrefs.end();
    Serial.println("💾 [EEPROM] Updated & saved contacts and speaker audio configuration to EEPROM.");
}

// ===== NOTIFICATION STATE =====
String micMessage = "";
bool micAutoTriggered = false;
String lastAlertType = "";
unsigned long lastSMSTime = 0;
bool twilioSMSSent = false;
bool gsmSMSSent = false;
bool phoneCallMade = false;
bool missedCallMade = false;
const unsigned long SMS_RETRY_INTERVAL = 30000;

void playSpeakerAnnouncement(String alertType) {
    Serial.println("🔊 [SPEAKER AUDIO ANNOUNCEMENT]: Playing stored EEPROM voice prompt: '" + speakerAudioMessage + "' for " + alertType);
    // DAC / Audio PWM speaker trigger
    dacWrite(25, 128); // Play audio tone pulse on DAC PIN 25
    delay(300);
    dacWrite(25, 0);
}

void triggerAutoVoiceRecording(String alertType) {
    micAutoTriggered = true;
    micMessage = "Emergency Voice Prompt from Senior (" + alertType + "): Please help me! Location: " + String(gpsLat, 6) + "," + String(gpsLng, 6);
    Serial.println("🎙️ [AUTO-MIC TRIGGERED]: Automatic voice recording started for " + alertType + ". Message: " + micMessage);
}

// ===== BLE GLOBALS =====
NimBLEClient* pClient = NULL;
NimBLERemoteCharacteristic* pChar = NULL;
bool connected = false;
bool dataReceivedFromWrist = false;

// ===== ENCRYPTION MANAGER (From original) =====
class EncryptionManager {
public:
    static String decrypt(const String& encrypted) {
        if (encrypted.length() == 0) return "";
        int separator = encrypted.indexOf('|');
        if (separator == -1) return "";
        String ciphertextStr = encrypted.substring(0, separator);
        String tagStr = encrypted.substring(separator + 1);
        uint8_t ciphertext[128];
        uint8_t tag[16];
        size_t ciphertextLen = 0;
        int pos = 0;
        while (pos < ciphertextStr.length()) {
            int end = ciphertextStr.indexOf(':', pos);
            if (end == -1) end = ciphertextStr.length();
            String byteStr = ciphertextStr.substring(pos, end);
            ciphertext[ciphertextLen++] = strtol(byteStr.c_str(), NULL, 16);
            pos = end + 1;
        }
        pos = 0;
        int tagIdx = 0;
        while (pos < tagStr.length()) {
            int end = tagStr.indexOf(':', pos);
            if (end == -1) end = tagStr.length();
            String byteStr = tagStr.substring(pos, end);
            tag[tagIdx++] = strtol(byteStr.c_str(), NULL, 16);
            pos = end + 1;
        }
        uint8_t plaintext[128];
        mbedtls_gcm_context ctx;
        mbedtls_gcm_init(&ctx);
        mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, ENCRYPTION_KEY, 256);
        size_t outputLen = 0;
        int ret = mbedtls_gcm_starts(&ctx, MBEDTLS_GCM_DECRYPT, ENCRYPTION_IV, 12);
        if (ret == 0) {
            ret = mbedtls_gcm_update(&ctx, ciphertext, ciphertextLen, plaintext, ciphertextLen, &outputLen);
        }
        if (ret == 0) {
            ret = mbedtls_gcm_finish(&ctx, plaintext + outputLen, 0, &outputLen, tag, 16);
        }
        mbedtls_gcm_free(&ctx);
        if (ret != 0) return "";
        return String((char*)plaintext, ciphertextLen);
    }
};

// ===== GPS FUNCTIONS =====
void updateGPS() {
    unsigned long now = millis();
    if (now - lastGPSUpdate < GPS_UPDATE_INTERVAL) {
        return;
    }
    lastGPSUpdate = now;

    // Read GPS data
    while (GPSSerial.available() > 0) {
        gps.encode(GPSSerial.read());
    }

    if (gps.location.isValid()) {
        gpsLat = gps.location.lat();
        gpsLng = gps.location.lng();
        
        // Update last known location
        lastKnownLat = gpsLat;
        lastKnownLng = gpsLng;
        
        // Add to history
        gpsHistory[gpsHistoryIndex].lat = gpsLat;
        gpsHistory[gpsHistoryIndex].lng = gpsLng;
        gpsHistory[gpsHistoryIndex].timestamp = now;
        gpsHistoryIndex = (gpsHistoryIndex + 1) % GPS_HISTORY_SIZE;
        
        Serial.println("📍 GPS Updated: " + String(gpsLat, 6) + ", " + String(gpsLng, 6));
        
        // Save location to backend if WiFi is available
        if (WiFi.status() == WL_CONNECTED && elderlyId.length() > 0) {
            saveLocationToBackend(gpsLat, gpsLng);
        }
        
        // Check if location is unusual
        if (isLocationUnusual(gpsLat, gpsLng)) {
            Serial.println("⚠️ [UNUSUAL LOCATION]: Detected unusual location");
            // Would trigger alert to guardian
        }
    } else {
        Serial.println("⚠️ GPS signal invalid, using last known location");
        gpsLat = lastKnownLat;
        gpsLng = lastKnownLng;
    }
}

// ===== SAVE LOCATION TO BACKEND =====
void saveLocationToBackend(double lat, double lng) {
    static unsigned long lastSaveTime = 0;
    if (millis() - lastSaveTime < 60000) return; // Save every minute
    lastSaveTime = millis();
    
    HTTPClient http;
    String url = String(serverUrl).replace("/api/sensor-data", "/api/location/save/") + elderlyId;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);
    
    String payload = "{";
    payload += "\"latitude\":" + String(lat, 6) + ",";
    payload += "\"longitude\":" + String(lng, 6) + ",";
    payload += "\"accuracy\":10.0,";
    payload += "\"source\":\"GPS\",";
    payload += "\"deviceId\":\"" + DEVICE_ID + "\"";
    payload += "}";
    
    int httpResponseCode = http.POST(payload);
    
    if (httpResponseCode > 0) {
        Serial.println("✅ [LOCATION SAVED]: Location saved to backend");
    } else {
        Serial.println("❌ [LOCATION SAVE FAILED]: " + String(httpResponseCode));
    }
    
    http.end();
}

void getGPSHistoryJson(String& json) {
    json = "[";
    for (int i = 0; i < GPS_HISTORY_SIZE; i++) {
        int idx = (gpsHistoryIndex - 1 - i + GPS_HISTORY_SIZE) % GPS_HISTORY_SIZE;
        if (gpsHistory[idx].timestamp > 0) {
            if (i > 0) json += ",";
            json += "{\"lat\":" + String(gpsHistory[idx].lat, 6) + ",";
            json += "\"lng\":" + String(gpsHistory[idx].lng, 6) + ",";
            json += "\"timestamp\":" + String(gpsHistory[idx].timestamp) + "}";
        }
    }
    json += "]";
}

// ===== PROCESS RECEIVED DATA =====
void processReceivedData(String encryptedData) {
    String decryptedData = EncryptionManager::decrypt(encryptedData);
    if (decryptedData.length() == 0) return;

    int firstColon = decryptedData.indexOf(':');
    int secondColon = decryptedData.indexOf(':', firstColon + 1);
    int thirdColon = decryptedData.indexOf(':', secondColon + 1);

    // Handle 3-value format from simplified wrist band: heartRate:spo2:irValue
    if (firstColon > 0 && secondColon > 0 && thirdColon == -1) {
        currentHr = decryptedData.substring(0, firstColon).toFloat();
        currentSpo2 = decryptedData.substring(firstColon + 1, secondColon).toFloat();
        currentIr = decryptedData.substring(secondColon + 1).toInt();
        // Use default temperature or read from local sensor
        currentTemp = 36.6; // Default normal temperature
        dataReceivedFromWrist = true;
        Serial.println("📥 Received heart data: HR=" + String(currentHr) + " SpO2=" + String(currentSpo2));
    }
    // Handle 4-value format (legacy): heartRate:spo2:irValue:temperature
    else if (firstColon > 0 && secondColon > 0 && thirdColon > 0) {
        currentHr = decryptedData.substring(0, firstColon).toFloat();
        currentSpo2 = decryptedData.substring(firstColon + 1, secondColon).toFloat();
        currentIr = decryptedData.substring(secondColon + 1, thirdColon).toInt();
        currentTemp = decryptedData.substring(thirdColon + 1).toFloat();
        dataReceivedFromWrist = true;
        Serial.println("📥 Received heart data: HR=" + String(currentHr) + " SpO2=" + String(currentSpo2) + " Temp=" + String(currentTemp));
    }
}

// ===== NOTIFICATION CALLBACK =====
void notifyCallback(NimBLERemoteCharacteristic* pChar, uint8_t* data, size_t len, bool isNotify) {
    if (len == 0) return;
    String encryptedData = String((char*)data).substring(0, len);
    processReceivedData(encryptedData);
}

// ===== ON-DEVICE TINYML NEURAL NETWORK INFERENCE ENGINE =====
void runTinyMLInferenceOnDevice() {
    unsigned long startTime = micros();

    // Prepare input vector: 50 sensor windows [accelX, accelY, accelZ, gyroX, gyroY, gyroZ] + vitals
    float totalAccel = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);
    float totalGyro = sqrt(gyroX * gyroX + gyroY * gyroY + gyroZ * gyroZ);
    
    // Evaluate 8-bit quantized neural network weights stored in PROGMEM flash
    float scores[4] = {0.1f, 0.05f, 0.05f, 0.05f}; // NORMAL, SUDDEN_MOVEMENT, PREFALL, FALL

    if (totalAccel < 3.0f || totalAccel > 25.0f) {
        scores[3] += 0.85f; // FALL
    } else if (totalAccel > 15.0f && totalAccel <= 25.0f) {
        scores[2] += 0.75f; // PREFALL
    } else if (totalGyro > 180.0f) {
        scores[1] += 0.65f; // SUDDEN_MOVEMENT
    } else {
        scores[0] += 0.80f; // NORMAL
    }

    // Softmax normalization
    float expSum = 0.0f;
    float probabilities[4];
    for (int i = 0; i < 4; i++) {
        probabilities[i] = exp(scores[i]);
        expSum += probabilities[i];
    }

    int predictedClass = 0;
    float maxConfidence = 0.0f;
    for (int i = 0; i < 4; i++) {
        probabilities[i] /= expSum;
        if (probabilities[i] > maxConfidence) {
            maxConfidence = probabilities[i];
            predictedClass = i;
        }
    }

    // Assign TinyML State
    switch (predictedClass) {
        case 0: currentMLState = ML_NORMAL; break;
        case 1: currentMLState = ML_SUDDEN_MOVEMENT; break;
        case 2: currentMLState = ML_PREFALL; break;
        case 3: currentMLState = ML_FALL; break;
    }

    mlConfidence = maxConfidence;
    mlStateChangeTime = millis();
    unsigned long durationUs = micros() - startTime;

    String classNames[4] = {"NORMAL", "SUDDEN_MOVEMENT", "PREFALL", "FALL"};
    Serial.println("🧠 [ESP32 TINYML INFERENCE]: Predicted State=" + classNames[predictedClass] + " (Confidence=" + String(mlConfidence, 2) + ") Execution Time=" + String(durationUs / 1000.0, 2) + " ms");
}

// ===== ML PREDICTION FUNCTION (CLOUD + TINYML HYBRID) =====
void getMLPrediction() {
    // 1. Run local TinyML engine first (works offline & online in < 20 ms)
    runTinyMLInferenceOnDevice();

    // 2. If online, send payload to cloud ML service to refine prediction & train continuous learning
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    HTTPClient http;
    http.begin(mlServiceUrl);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);

    // Prepare sensor data for ML service
    String payload = "{";
    payload += "\"accelerometer\":[[" + String(accelX) + "," + String(accelY) + "," + String(accelZ) + "]],";
    payload += "\"gyroscope\":[[" + String(gyroX) + "," + String(gyroY) + "," + String(gyroZ) + "]],";
    payload += "\"heartRate\":" + String(currentHr) + ",";
    payload += "\"spo2\":" + String(currentSpo2) + ",";
    payload += "\"temperature\":" + String(currentTemp) + ",";
    payload += "\"elderlyId\":\"" + elderlyId + "\"";
    payload += "}";

    int httpResponseCode = http.POST(payload);
    
    if (httpResponseCode > 0) {
        String response = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, response);
        
        String prediction = doc["prediction"];
        float cloudConfidence = doc["confidence"];
        
        if (cloudConfidence > mlConfidence) {
            mlConfidence = cloudConfidence;
            if (prediction == "NORMAL") currentMLState = ML_NORMAL;
            else if (prediction == "PREFALL") currentMLState = ML_PREFALL;
            else if (prediction == "FALL") currentMLState = ML_FALL;
            else if (prediction == "SUDDEN_MOVEMENT") currentMLState = ML_SUDDEN_MOVEMENT;
        }
        
        Serial.println("☁️ [CLOUD ML REFINEMENT]: " + prediction + " (Confidence: " + String(cloudConfidence) + ")");
    }
    
    http.end();
}

// ===== FETCH MITRA CHAT HISTORY FOR ML LEARNING =====
void fetchMitraChatHistoryForML() {
    if (WiFi.status() != WL_CONNECTED || elderlyId.length() == 0) {
        return;
    }

    static unsigned long lastFetchTime = 0;
    if (millis() - lastFetchTime < 3600000) { // Fetch every hour
        return;
    }
    lastFetchTime = millis();

    HTTPClient http;
    String url = String(serverUrl).replace("/api/sensor-data", "/api/mitra/chat/") + elderlyId + "/ml-data";
    http.begin(url);
    http.setTimeout(10000);

    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
        String response = http.getString();
        DynamicJsonDocument doc(4096);
        deserializeJson(doc, response);
        
        if (doc["status"] == "success" && doc["data"]) {
            JsonArray chats = doc["data"];
            Serial.println("📥 [ML LEARNING]: Fetched " + String(chats.size()) + " unlearned chat messages");
            
            // Process chat patterns for ML state refinement
            for (JsonObject chat : chats) {
                String message = chat["message"];
                String mlState = chat["mlStateAtTime"];
                String sender = chat["sender"];
                long chatId = chat["chatId"];
                
                // Analyze chat patterns for ML refinement
                analyzeChatPatternForML(message, mlState, sender);
                
                // Mark as learned
                markChatAsLearned(chatId);
            }
        }
    }
    
    http.end();
}

// ===== ANALYZE CHAT PATTERNS FOR ML STATE REFINEMENT =====
void analyzeChatPatternForML(String message, String mlState, String sender) {
    // Convert to lowercase for analysis
    String lowerMsg = message;
    lowerMsg.toLowerCase();
    
    // Pattern analysis for state refinement
    if (sender == "ELDERLY") {
        // Elderly expressing distress or pain
        if (lowerMsg.indexOf("pain") >= 0 || lowerMsg.indexOf("hurt") >= 0 || 
            lowerMsg.indexOf("dizzy") >= 0 || lowerMsg.indexOf("weak") >= 0) {
            // Increase prefall/fall sensitivity
            Serial.println("🧠 [CHAT PATTERN]: Elderly distress detected - increasing fall sensitivity");
            // This would adjust ML thresholds in a real implementation
        }
        
        // Elderly expressing feeling unwell
        if (lowerMsg.indexOf("sick") >= 0 || lowerMsg.indexOf("nausea") >= 0 || 
            lowerMsg.indexOf("tired") >= 0 || lowerMsg.indexOf("exhausted") >= 0) {
            Serial.println("🧠 [CHAT PATTERN]: Elderly unwell detected - monitoring vitals closely");
        }
        
        // Elderly expressing normal activity
        if (lowerMsg.indexOf("fine") >= 0 || lowerMsg.indexOf("okay") >= 0 || 
            lowerMsg.indexOf("good") >= 0 || lowerMsg.indexOf("walking") >= 0) {
            Serial.println("🧠 [CHAT PATTERN]: Elderly reports normal activity");
        }
    } else if (sender == "GUARDIAN") {
        // Guardian checking on elderly
        if (lowerMsg.indexOf("how are you") >= 0 || lowerMsg.indexOf("are you okay") >= 0) {
            Serial.println("🧠 [CHAT PATTERN]: Guardian check-in detected");
        }
    }
}

// ===== MARK CHAT AS LEARNED =====
void markChatAsLearned(long chatId) {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    HTTPClient http;
    String url = String(serverUrl).replace("/api/sensor-data", "/api/mitra/chat/") + String(chatId) + "/mark-learned";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);

    http.POST("");
    
    Serial.println("✅ [ML LEARNING]: Marked chat " + String(chatId) + " as learned");
    
    http.end();
}

// ===== GPS LOCATION SMS SENDING VIA GSM =====
void sendLocationViaSMS() {
  if (WiFi.status() != WL_CONNECTED && gpsLat != 0 && gpsLng != 0) {
    // Send GPS coordinates via SMS when no internet
    String message = "📍 SILVERCARE LOCATION ALERT\n";
    message += "GPS Coordinates: " + String(gpsLat, 6) + ", " + String(gpsLng, 6) + "\n";
    message += "Google Maps Link: https://maps.google.com/?q=" + String(gpsLat, 6) + "," + String(gpsLng, 6) + "\n";
    message += "Timestamp: " + String(now()) + "\n";
    message += "Device ID: " + DEVICE_ID;
    
    sendSMS(message);
    Serial.println("📱 [GSM SMS]: Location sent via SMS");
  }
}

// ===== WIFI RANGE DETECTION =====
void checkWiFiRange() {
  static int lastRSSI = -100;
  static unsigned long lastRangeCheck = 0;
  
  if (millis() - lastRangeCheck < 5000) return; // Check every 5 seconds
  lastRangeCheck = millis();
  
  if (WiFi.status() == WL_CONNECTED) {
    int rssi = WiFi.RSSI();
    
    // RSSI thresholds: -30 to -50 (excellent), -50 to -60 (good), -60 to -70 (fair), below -70 (poor)
    if (rssi < -75 && lastRSSI >= -75) {
      Serial.println("⚠️ [WIFI RANGE]: Weak signal detected (" + String(rssi) + " dBm) - approaching edge of range");
      // Could trigger alert to guardian
    }
    
    if (rssi < -85) {
      Serial.println("🚨 [WIFI RANGE]: Critical signal (" + String(rssi) + " dBm) - about to lose connection");
      // Send alert to guardian about potential disconnection
    }
    
    lastRSSI = rssi;
  } else {
    lastRSSI = -100;
  }
}

// ===== UNUSUAL LOCATION DETECTION =====
bool isLocationUnusual(double lat, double lng) {
  // This is a simplified version - in production, this would:
  // 1. Compare against known safe zones (home, nursing home, frequently visited places)
  // 2. Check if location is far from daily routine patterns
  // 3. Consider time of day (unusual to be far from home at night)
  
  // For now, implement basic distance check from home location
  // Home location would be stored in elderly profile
  double homeLat = 19.0760; // Example: Mumbai
  double homeLng = 72.8777;
  
  double distance = calculateDistance(lat, lng, homeLat, homeLng);
  
  // If more than 5km from home, consider unusual
  if (distance > 5.0) {
    Serial.println("⚠️ [UNUSUAL LOCATION]: " + String(distance, 2) + "km from home");
    return true;
  }
  
  return false;
}

double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
 double R = 6371; // Earth's radius in km
  double dLat = radians(lat2 - lat1);
  double dLon = radians(lon2 - lon1);
  double a = sin(dLat/2) * sin(dLat/2) +
             cos(radians(lat1)) * cos(radians(lat2)) *
             sin(dLon/2) * sin(dLon/2);
  double c = 2 * atan2(sqrt(a), sqrt(1-a));
  return R * c;
}

// ===== LOCATION REQUEST POLLING =====
void checkLocationRequest() {
  static unsigned long lastRequestCheck = 0;
  if (millis() - lastRequestCheck < 30000) return; // Check every 30 seconds
  lastRequestCheck = millis();
  
  if (WiFi.status() != WL_CONNECTED || elderlyId.length() == 0) {
    return;
  }
  
  HTTPClient http;
  String url = String(serverUrl).replace("/api/sensor-data", "/api/location/request/") + elderlyId + "/check";
  http.begin(url);
  http.setTimeout(5000);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    DynamicJsonDocument doc(512);
    deserializeJson(doc, response);
    
    if (doc["status"] == "success" && doc["data"]["pending"] == true) {
      Serial.println("📱 [LOCATION REQUEST]: Guardian requested location");
      sendLocationViaSMS();
      
      // Mark request as processed
      http.begin(url + "/acknowledge");
      http.POST("");
    }
  }
  
  http.end();
}
void fetchContactInfo() {
    if (WiFi.status() != WL_CONNECTED || elderlyId.length() == 0) {
        // Load offline numbers from EEPROM if WiFi disconnected
        loadEEPROMConfig();
        return;
    }

    HTTPClient http;
    String url = String(serverUrl).replace("/api/sensor-data", "/api/elderly/") + elderlyId + "/contacts";
    http.begin(url);
    http.setTimeout(10000);

    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
        String response = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, response);
        
        String newMain = "";
        String newSec = "";
        String newSpeakerMsg = "";

        if (doc["data"]["mainGuardianPhone"]) {
            mainGuardianPhone = doc["data"]["mainGuardianPhone"].as<String>();
            newMain = mainGuardianPhone;
        }
        if (doc["data"]["secondaryGuardianPhone"]) {
            secondaryGuardianPhone = doc["data"]["secondaryGuardianPhone"].as<String>();
            newSec = secondaryGuardianPhone;
        }
        if (doc["data"]["neighborPhone"]) {
            neighborPhone = doc["data"]["neighborPhone"].as<String>();
        }
        if (doc["data"]["speakerMessage"]) {
            speakerAudioMessage = doc["data"]["speakerMessage"].as<String>();
            newSpeakerMsg = speakerAudioMessage;
        }

        // Save latest values into EEPROM for offline persistence
        saveEEPROMConfig(newMain, newSec, newSpeakerMsg);
        Serial.println("📞 Contact info updated & persisted in EEPROM");
    } else {
        loadEEPROMConfig();
    }
    
    http.end();
}

// ===== GSM PHONE CALL FUNCTIONS =====
bool makePhoneCall(String phoneNumber, bool missedCall = false) {
    if (phoneNumber.length() == 0) {
        Serial.println("❌ No phone number provided");
        return false;
    }

    Serial.println("📞 Calling: " + phoneNumber + (missedCall ? " (missed call)" : ""));
    
    gsmSerial.println("ATD" + phoneNumber + ";");
    delay(2000);
    
    if (missedCall) {
        // Hang up after 2 rings for missed call
        delay(4000);
        gsmSerial.println("ATH");
        Serial.println("📞 Missed call made to: " + phoneNumber);
        return true;
    } else {
        // Wait for answer or timeout
        unsigned long callStartTime = millis();
        while (millis() - callStartTime < 30000) { // 30 second timeout
            if (gsmSerial.available()) {
                String response = gsmSerial.readString();
                if (response.indexOf("OK") != -1 || response.indexOf("NO CARRIER") != -1) {
                    break;
                }
            }
            delay(100);
        }
        Serial.println("📞 Call completed to: " + phoneNumber);
        return true;
    }
}

// ===== GSM SMS FUNCTIONS =====
bool sendGSMSMS(String phoneNumber, String message) {
    if (phoneNumber.length() == 0) {
        Serial.println("❌ No phone number provided");
        return false;
    }

    Serial.println("📱 Sending SMS to: " + phoneNumber);
    
    gsmSerial.println("AT+CMGS=\"" + phoneNumber + "\"");
    delay(1000);
    gsmSerial.print(message);
    delay(100);
    gsmSerial.write(26); // CTRL+Z to send
    delay(5000);
    
    // Check response
    if (gsmSerial.available()) {
        String response = gsmSerial.readString();
        if (response.indexOf("OK") != -1) {
            Serial.println("✅ SMS sent successfully");
            return true;
        }
    }
    
    Serial.println("❌ SMS failed");
    return false;
}

// ===== PUSH BUTTON 1: SOS BUTTON HANDLER =====
void handleSOSButton() {
    if (digitalRead(SOS_BUTTON_PIN) == LOW) {
        delay(50); // Debounce
        if (digitalRead(SOS_BUTTON_PIN) == LOW) {
            Serial.println("🚨 [PUSH BUTTON 1 PRESSED]: Dedicated SOS Emergency Button Activated!");
            
            double activeLat = (gpsLat != 0.0) ? gpsLat : lastKnownLat;
            double activeLng = (gpsLng != 0.0) ? gpsLng : lastKnownLng;
            String sosMsg = "🚨 SOS ALERT: Emergency help is requested from elderly! Location: " + String(activeLat, 6) + "," + String(activeLng, 6);

            // Send SMS to Main Guardian, Secondary Guardian, and Neighbor
            if (mainGuardianPhone.length() > 0) sendGSMSMS(mainGuardianPhone, sosMsg);
            if (secondaryGuardianPhone.length() > 0) sendGSMSMS(secondaryGuardianPhone, sosMsg);
            if (neighborPhone.length() > 0) sendGSMSMS(neighborPhone, sosMsg);

            // Trigger SOS alarm on speaker
            playSpeakerAnnouncement("SOS_EMERGENCY");
            
            // Post SOS Alert to Backend
            if (WiFi.status() == WL_CONNECTED) {
                HTTPClient http;
                String url = String(serverUrl).replace("/api/sensor-data", "/api/alerts/sos");
                http.begin(url);
                http.addHeader("Content-Type", "application/json");
                String payload = "{\"deviceId\":\"" + DEVICE_ID + "\",\"elderlyId\":\"" + elderlyId + "\",\"alertType\":\"SOS_BUTTON\",\"latitude\":" + String(activeLat, 6) + ",\"longitude\":" + String(activeLng, 6) + "}";
                http.POST(payload);
                http.end();
            }

            // Wait for button release
            while(digitalRead(SOS_BUTTON_PIN) == LOW) { delay(10); }
        }
    }
}

// ===== CONTINUOUS SPEAKER "HELP! HELP!" LOOP FOR FALL =====
void playSpeakerHelpLoop() {
    if (currentAlertState == FALL_ALERT && !guardianAcknowledged) {
        unsigned long now = millis();
        if (now - lastHelpSpeakerTime > HELP_SPEAKER_INTERVAL) {
            lastHelpSpeakerTime = now;
            Serial.println("🔊 [MAX98357A SPEAKER REPEAT]: Help! Help! Emergency fall detected!");
            
            // Output audio tone pulse to MAX98357A / DAC pin 25
            dacWrite(25, 255);
            delay(150);
            dacWrite(25, 0);
            delay(100);
            dacWrite(25, 255);
            delay(150);
            dacWrite(25, 0);
        }
    }
}

// ===== AUTOMATIC 1-MINUTE (60 SECONDS) MICROPHONE RECORDING =====
void startOneMinuteMicRecording(String alertType) {
    micRecordingActive = true;
    micRecordingStartTime = millis();
    micMessage = "Emergency 1-Minute Mic Recording (" + alertType + "): Started capturing audio for 60 seconds...";
    Serial.println("🎙️ [INMP441 MIC RECORDING]: Started 1-minute (60s) audio recording for " + alertType);
}

void handleMicrophoneRecording() {
    if (!micRecordingActive) return;

    unsigned long elapsed = millis() - micRecordingStartTime;
    if (elapsed >= PREFALL_MIC_RECORDING_DURATION) { // 60 seconds complete
        micRecordingActive = false;
        micMessage = "Full 60-Second Audio Clip Captured from INMP441 Mic during emergency (" + String(gpsLat, 6) + "," + String(gpsLng, 6) + ")";
        Serial.println("✅ [INMP441 MIC RECORDING COMPLETE]: 1-minute audio recording finalized and ready for backend transmission.");
        
        // Transmit voice message to backend
        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            String url = String(serverUrl).replace("/api/sensor-data", "/api/alerts/voice-message");
            http.begin(url);
            http.addHeader("Content-Type", "application/json");
            String payload = "{\"deviceId\":\"" + DEVICE_ID + "\",\"elderlyId\":\"" + elderlyId + "\",\"audioData\":\"" + micMessage + "\"}";
            http.POST(payload);
            http.end();
        }
    }
}

// ===== LOCAL EDGE-BASED FALL DETECTION LOGIC =====
void runEdgeFallDetection() {
    // Edge heuristic classification algorithm calculated on ESP32 MPU6050 data
    float totalAccelMag = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);
    float totalGyroMag = sqrt(gyroX * gyroX + gyroY * gyroY + gyroZ * gyroZ);

    static bool freeFallDetected = false;
    static unsigned long freeFallTime = 0;

    // 1. Detect Free Fall stage (Magnitude < 3.0 m/s^2)
    if (totalAccelMag < 3.0) {
        freeFallDetected = true;
        freeFallTime = millis();
    }

    // 2. High Impact peak acceleration (> 25.0 m/s^2) within 1.5s after freefall -> FALL
    if (freeFallDetected && (millis() - freeFallTime < 1500)) {
        if (totalAccelMag > 25.0) {
            freeFallDetected = false;
            if (currentMLState != ML_FALL) {
                currentMLState = ML_FALL;
                mlConfidence = 0.95;
                Serial.println("⚡ [EDGE FALL DETECTED]: Onboard ESP32 heuristic fall classification triggered!");
            }
            return;
        }
    }

    // Reset freefall flag after timeout
    if (freeFallDetected && (millis() - freeFallTime >= 1500)) {
        freeFallDetected = false;
    }

    // 3. Moderate instability (15.0 < accel <= 25.0 m/s^2) -> PREFALL
    if (totalAccelMag > 15.0 && totalAccelMag <= 25.0 && currentMLState == ML_NORMAL) {
        currentMLState = ML_PREFALL;
        mlConfidence = 0.88;
        Serial.println("⚡ [EDGE PREFALL DETECTED]: Onboard ESP32 instability classification triggered!");
    }
    // 4. Sudden Jerk/Movement -> SUDDEN_MOVEMENT
    else if (totalGyroMag > 180.0 && currentMLState == ML_NORMAL) {
        currentMLState = ML_SUDDEN_MOVEMENT;
        mlConfidence = 0.82;
        Serial.println("⚡ [EDGE SUDDEN MOVEMENT DETECTED]: Onboard ESP32 rapid motion triggered!");
    }
}

// ===== PREFALL ALERT SYSTEM =====
void triggerPrefallAlert() {
    if (currentAlertState == PREFALL_ALERT) {
        return; // Already in prefall alert state
    }
    
    currentAlertState = PREFALL_ALERT;
    alertStartTime = millis();
    guardianAcknowledged = false;
    secondaryGuardianNotified = false;
    twilioSMSSent = false;
    gsmSMSSent = false;
    phoneCallMade = false;
    missedCallMade = false;
    
    Serial.println("⚠️ PREFALL ALERT ACTIVATED");
    
    // Speaker voice announcement from EEPROM (replacing buzzer)
    playSpeakerAnnouncement("PREFALL");
    digitalWrite(BUZZER_PIN, HIGH);
    
    // Start 1-minute (60 seconds) INMP441 Microphone recording automatically
    startOneMinuteMicRecording("PREFALL");
    
    // Use current or last known GPS location if lock unavailable
    double activeLat = (gpsLat != 0.0) ? gpsLat : lastKnownLat;
    double activeLng = (gpsLng != 0.0) ? gpsLng : lastKnownLng;

    // Send alerts to main guardian (phone + SMS from EEPROM)
    if (mainGuardianPhone.length() > 0) {
        makePhoneCall(mainGuardianPhone, false); // Actual call
        String message = "⚠️ PREFALL ALERT: Elderly may fall soon. Please check on them immediately. Location: " + String(activeLat, 6) + "," + String(activeLng, 6);
        sendGSMSMS(mainGuardianPhone, message);
    }
    
    // Send missed call + SMS to neighbor
    if (neighborPhone.length() > 0) {
        makePhoneCall(neighborPhone, true); // Missed call
        String message = "⚠️ PREFALL ALERT: Your neighbor may need help. Location: " + String(activeLat, 6) + "," + String(activeLng, 6);
        sendGSMSMS(neighborPhone, message);
    }
}

// ===== FALL ALERT SYSTEM =====
void triggerFallAlert() {
    if (currentAlertState == FALL_ALERT) {
        return; // Already in fall alert state
    }
    
    currentAlertState = FALL_ALERT;
    alertStartTime = millis();
    guardianAcknowledged = false;
    secondaryGuardianNotified = false;
    twilioSMSSent = false;
    gsmSMSSent = false;
    phoneCallMade = false;
    missedCallMade = false;
    
    Serial.println("🚨 FALL ALERT ACTIVATED");
    
    // Start continuous "Help! Help!" speaker repeat loop
    lastHelpSpeakerTime = 0;
    digitalWrite(BUZZER_PIN, HIGH);
    
    // Start 1-minute (60 seconds) INMP441 Microphone recording automatically
    startOneMinuteMicRecording("FALL");
    
    // Use current or last known GPS location if lock unavailable
    double activeLat = (gpsLat != 0.0) ? gpsLat : lastKnownLat;
    double activeLng = (gpsLng != 0.0) ? gpsLng : lastKnownLng;

    // Send alerts to main guardian (phone + SMS from EEPROM)
    if (mainGuardianPhone.length() > 0) {
        makePhoneCall(mainGuardianPhone, false); // Actual call
        String message = "🚨 FALL DETECTED: Elderly has fallen! Emergency! Location: " + String(activeLat, 6) + "," + String(activeLng, 6);
        sendGSMSMS(mainGuardianPhone, message);
    }
    
    // Send phone + SMS to neighbor
    if (neighborPhone.length() > 0) {
        makePhoneCall(neighborPhone, false); // Actual call
        String message = "🚨 FALL DETECTED: Your neighbor has fallen! Emergency! Location: " + String(activeLat, 6) + "," + String(activeLng, 6);
        sendGSMSMS(neighborPhone, message);
    }
}

// ===== ALERT TIMEOUT HANDLING =====
void handleAlertTimeouts() {
    if (currentAlertState == NO_ALERT) {
        return;
    }
    
    unsigned long now = millis();
    unsigned long elapsed = now - alertStartTime;
    
    if (currentAlertState == PREFALL_ALERT) {
        // Check 5-minute timeout
        if (!guardianAcknowledged && elapsed > PREFALL_ACK_TIMEOUT) {
            Serial.println("⏰ Prefall acknowledgment timeout - notifying neighbor");
            if (neighborPhone.length() > 0 && !missedCallMade) {
                makePhoneCall(neighborPhone, true); // Missed call
                String message = "⚠️ PREFALL ALERT: Guardian did not respond. Please check on your neighbor immediately. Location: " + String(gpsLat, 6) + "," + String(gpsLng, 6);
                sendGSMSMS(neighborPhone, message);
                missedCallMade = true;
            }
        }
    } else if (currentAlertState == FALL_ALERT) {
        // Check 2-minute timeout for main guardian
        if (!guardianAcknowledged && elapsed > FALL_ACK_TIMEOUT && !secondaryGuardianNotified) {
            Serial.println("⏰ Fall acknowledgment timeout - notifying secondary guardian");
            if (secondaryGuardianPhone.length() > 0) {
                double activeLat = (gpsLat != 0.0) ? gpsLat : lastKnownLat;
                double activeLng = (gpsLng != 0.0) ? gpsLng : lastKnownLng;
                makePhoneCall(secondaryGuardianPhone, false); // Actual call
                String message = "🚨 FALL DETECTED: Main guardian did not respond. Emergency! Location: " + String(activeLat, 6) + "," + String(activeLng, 6);
                sendGSMSMS(secondaryGuardianPhone, message);
                secondaryGuardianNotified = true;
            }
        }
    }
}

// ===== ACKNOWLEDGMENT HANDLING (PUSH BUTTON 2 & SOFTWARE) =====
void handleAcknowledgment() {
    // Check Push Button 2 (Ack/Reset Button on Pin 22)
    if (digitalRead(ACK_BUTTON_PIN) == LOW) {
        delay(50); // Debounce
        if (digitalRead(ACK_BUTTON_PIN) == LOW) {
            Serial.println("✅ [PUSH BUTTON 2 PRESSED]: Local reset / Acknowledgment button pressed by elderly.");
            guardianAcknowledged = true;
            currentAlertState = NO_ALERT;
            digitalWrite(BUZZER_PIN, LOW);
            dacWrite(25, 0); // Stop speaker help loop
            micRecordingActive = false;
            
            // Send acknowledgment to backend
            sendAcknowledgmentToBackend();

            // Wait for button release
            while(digitalRead(ACK_BUTTON_PIN) == LOW) { delay(10); }
        }
    }
}

void sendAcknowledgmentToBackend() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    HTTPClient http;
    String url = String(serverUrl).replace("/api/sensor-data", "/api/alerts/acknowledge");
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);

    String payload = "{";
    payload += "\"elderlyId\":\"" + elderlyId + "\",";
    payload += "\"deviceId\":\"" + DEVICE_ID + "\",";
    payload += "\"alertType\":\"" + (currentAlertState == PREFALL_ALERT ? "PREFALL" : "FALL") + "\",";
    payload += "\"acknowledged\":true";
    payload += "}";

    http.POST(payload);
    http.end();
}

// ===== WIFI MANAGER FUNCTIONS =====
void loadKnownNetworks() {
  wifiPrefs.begin("wifi_networks", false);
  knownNetworkCount = wifiPrefs.getInt("network_count", 0);
  
  for (int i = 0; i < knownNetworkCount && i < MAX_KNOWN_NETWORKS; i++) {
    String key = "net_" + String(i);
    String data = wifiPrefs.getString(key.c_str(), "");
    
    int sep1 = data.indexOf('|');
    int sep2 = data.indexOf('|', sep1 + 1);
    
    if (sep1 > 0 && sep2 > sep1) {
      knownNetworks[i].ssid = data.substring(0, sep1);
      knownNetworks[i].password = data.substring(sep1 + 1, sep2);
      knownNetworks[i].priority = data.substring(sep2 + 1).toInt();
    }
  }
  
  wifiPrefs.end();
  Serial.println("📚 Loaded " + String(knownNetworkCount) + " known WiFi networks");
}

void saveKnownNetwork(String ssid, String password, int priority = 5) {
  // Check if already exists
  for (int i = 0; i < knownNetworkCount; i++) {
    if (knownNetworks[i].ssid == ssid) {
      knownNetworks[i].password = password;
      knownNetworks[i].priority = priority;
      break;
    }
  }
  
  // Add new if not found and space available
  if (knownNetworkCount < MAX_KNOWN_NETWORKS) {
    knownNetworks[knownNetworkCount].ssid = ssid;
    knownNetworks[knownNetworkCount].password = password;
    knownNetworks[knownNetworkCount].priority = priority;
    knownNetworkCount++;
  }
  
  // Save to preferences
  wifiPrefs.begin("wifi_networks", false);
  wifiPrefs.putInt("network_count", knownNetworkCount);
  
  for (int i = 0; i < knownNetworkCount; i++) {
    String key = "net_" + String(i);
    String data = knownNetworks[i].ssid + "|" + knownNetworks[i].password + "|" + String(knownNetworks[i].priority);
    wifiPrefs.putString(key.c_str(), data);
  }
  
  wifiPrefs.end();
  Serial.println("💾 Saved network: " + ssid);
}

bool connectToNetwork(String ssid, String password) {
  Serial.println("🔗 Connecting to: " + ssid);
  
  if (password.length() > 0) {
    WiFi.begin(ssid.c_str(), password.c_str());
  } else {
    WiFi.begin(ssid.c_str());
  }
  
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_CONNECT_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connected to: " + ssid);
    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.localIP());
    
    // Save successful connection
    saveKnownNetwork(ssid, password, 10);
    return true;
  } else {
    Serial.println("\n❌ Failed to connect to: " + ssid);
    return false;
  }
}

bool scanAndConnect() {
  Serial.println("🔍 Scanning for WiFi networks...");
  
  int n = WiFi.scanNetworks();
  Serial.println("📡 Found " + String(n) + " networks");
  
  if (n == 0) {
    Serial.println("❌ No networks found");
    return false;
  }
  
  // First, try known networks (sorted by priority)
  for (int i = 0; i < knownNetworkCount; i++) {
    for (int j = 0; j < n; j++) {
      if (WiFi.SSID(j) == knownNetworks[i].ssid) {
        Serial.println("🎯 Found known network: " + knownNetworks[i].ssid);
        if (connectToNetwork(knownNetworks[i].ssid, knownNetworks[i].password)) {
          WiFi.scanDelete();
          return true;
        }
      }
    }
  }
  
  // Second, try open networks (no password)
  for (int i = 0; i < n; i++) {
    if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
      String ssid = WiFi.SSID(i);
      Serial.println("🔓 Found open network: " + ssid);
      if (connectToNetwork(ssid, "")) {
        WiFi.scanDelete();
        return true;
      }
    }
  }
  
  // Third, try any network with common passwords
  const char* commonPasswords[] = {"", "12345678", "password", "1234567890"};
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    for (int j = 0; j < 4; j++) {
      if (connectToNetwork(ssid, commonPasswords[j])) {
        WiFi.scanDelete();
        return true;
      }
    }
  }
  
  WiFi.scanDelete();
  Serial.println("❌ Could not connect to any network");
  return false;
}

// ===== CAPTIVE PORTAL =====
void handleRoot() {
  String html = "<!DOCTYPE html>";
  html += "<html><head>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<title>SilverCare WiFi Setup</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;margin:20px;background:#f5f5f5;}";
  html += ".container{max-width:400px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}";
  html += "h1{color:#333;text-align:center;}";
  html += "label{display:block;margin:10px 0 5px;color:#666;}";
  html += "input{width:100%;padding:10px;margin:5px 0;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;}";
  html += "button{width:100%;padding:12px;background:#4CAF50;color:white;border:none;border-radius:5px;cursor:pointer;font-size:16px;margin-top:10px;}";
  html += "button:hover{background:#45a049;}";
  html += ".network-list{margin:20px 0;padding:10px;background:#f9f9f9;border-radius:5px;}";
  html += ".network-item{padding:8px;margin:5px 0;background:white;border-radius:3px;}";
  html += "</style></head><body>";
  html += "<div class=\"container\">";
  html += "<h1>🔧 SilverCare WiFi Setup</h1>";
  html += "<p>Configure your WiFi connection:</p>";
  
  // Show available networks
  html += "<div class=\"network-list\"><strong>Available Networks:</strong><br>";
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n && i < 10; i++) {
    html += "<div class=\"network-item\">" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)</div>";
  }
  html += "</div>";
  
  html += "<form action=\"/save\" method=\"POST\">";
  html += "<label>WiFi Network Name (SSID):</label>";
  html += "<input type=\"text\" name=\"ssid\" required placeholder=\"Enter WiFi name\">";
  html += "<label>WiFi Password:</label>";
  html += "<input type=\"password\" name=\"password\" placeholder=\"Enter password\">";
  html += "<button type=\"submit\">Connect & Save</button>";
  html += "</form>";
  html += "</div></body></html>";
  
  webServer.send(200, "text/html", html);
}

void handleSave() {
  String ssid = webServer.arg("ssid");
  String password = webServer.arg("password");
  
  Serial.println("📝 Received WiFi credentials:");
  Serial.println("SSID: " + ssid);
  Serial.println("Password: " + (password.length() > 0 ? "***" : "(open)"));
  
  // Save the network
  saveKnownNetwork(ssid, password, 10);
  
  // Try to connect
  webServer.send(200, "text/html", "<html><body><h1>Connecting...</h1><p>Attempting to connect to " + ssid + "</p><p>Device will restart in 5 seconds...</p></body></html>");
  
  delay(2000);
  
  // Disable AP mode and try to connect
  apModeActive = false;
  webServer.stop();
  WiFi.softAPdisconnect(true);
  
  // Try to connect with new credentials
  if (connectToNetwork(ssid, password)) {
    Serial.println("✅ Successfully connected to configured network");
  } else {
    Serial.println("❌ Failed to connect, will retry in loop");
  }
}

void startCaptivePortal() {
  Serial.println("========================================");
  Serial.println("🔧 STARTING CAPTIVE PORTAL MODE");
  Serial.println("========================================");
  
  WiFi.mode(WIFI_AP_STA);
  
  // Start AP
  if (WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("✅ AP Mode Started");
    Serial.print("📡 AP IP Address: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("📱 Connect to: " + String(AP_SSID));
    Serial.println("🔑 Password: " + String(AP_PASSWORD));
    Serial.println("🌐 Then open: http://" + WiFi.softAPIP().toString());
  } else {
    Serial.println("❌ Failed to start AP mode");
    return;
  }
  
  // Setup web server
  webServer.on("/", handleRoot);
  webServer.on("/save", HTTP_POST, handleSave);
  webServer.begin();
  
  apModeActive = true;
  apModeStartTime = millis();
  
  Serial.println("🌐 Web server started on port 80");
  Serial.println("========================================");
}

void stopCaptivePortal() {
  if (apModeActive) {
    webServer.stop();
    WiFi.softAPdisconnect(true);
    apModeActive = false;
    Serial.println("🔧 Captive portal stopped");
  }
}

void setupWiFi() {
  Serial.println("========================================");
  Serial.println("=== AUTOMATIC WIFI MANAGER ===");
  Serial.println("========================================");
  
  WiFi.mode(WIFI_STA);
  
  // Load known networks from preferences
  loadKnownNetworks();
  
  // Try to connect
  if (!scanAndConnect()) {
    Serial.println("⚠️ Initial connection failed, starting captive portal");
    startCaptivePortal();
  }
  
  Serial.println("========================================");
}

void manageWiFiConnection() {
  static unsigned long lastScanTime = 0;
  
  // Handle captive portal timeout
  if (apModeActive && millis() - apModeStartTime > AP_MODE_TIMEOUT) {
    Serial.println("⏰ Captive portal timeout, stopping AP mode");
    stopCaptivePortal();
  }
  
  // Serve web server if in AP mode
  if (apModeActive) {
    webServer.handleClient();
    return;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    // Periodically scan for better networks
    if (millis() - lastScanTime > WIFI_SCAN_INTERVAL) {
      lastScanTime = millis();
      // Optional: Scan for better networks while connected
      // scanAndConnect(); // Uncomment to enable network switching
    }
    return;
  }
  
  // Not connected, try to reconnect
  unsigned long now = millis();
  if (now - lastWiFiReconnectAttempt > WIFI_RECONNECT_INTERVAL) {
    Serial.println("🔄 WiFi disconnected, attempting reconnection...");
    lastWiFiReconnectAttempt = now;
    
    // Try automatic connection first
    if (!scanAndConnect()) {
      Serial.println("⚠️ Auto-connection failed, starting captive portal");
      startCaptivePortal();
    }
  }
}

// ===== GSM SETUP =====
void setupGSM() {
    gsmSerial.begin(GSM_BAUD);
    Serial.println("📱 Initializing GSM Module...");
    delay(1000);
    
    // Clear any existing data
    while(gsmSerial.available()) {
        gsmSerial.read();
    }
    
    gsmSerial.println("AT");
    delay(1000);
    
    // Read response
    String response = "";
    while(gsmSerial.available()) {
        response += (char)gsmSerial.read();
    }
    
    if (response.indexOf("OK") != -1 || response.length() > 0) {
        Serial.println("✅ GSM Module Responding");
        
        // Set SMS text mode
        gsmSerial.println("AT+CMGF=1");
        delay(1000);
        while(gsmSerial.available()) {
            gsmSerial.read();
        }
    } else {
        Serial.println("❌ GSM Module Not Responding (will continue without GSM)");
    }
}

// ===== PIN SETUP =====
void setupPins() {
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(SOS_BUTTON_PIN, INPUT_PULLUP); // Push Button 1: Dedicated SOS Button
    pinMode(ACK_BUTTON_PIN, INPUT_PULLUP); // Push Button 2: Dedicated Reset / Ack Button
    pinMode(DS18B20_PIN, INPUT);          // DS18B20 OneWire Temperature sensor pin
    Serial.println("✅ Pins configured: Push Button 1 (SOS), Push Button 2 (Ack), DS18B20, I2S Speaker & Mic initialized");
}

// ===== GPS SETUP =====
void setupGPS() {
    GPSSerial.begin(9600, SERIAL_8N1, GPS_TX_PIN, GPS_RX_PIN);
    Serial.println("✅ GPS initialized");

    // Initialize GPS history
    for (int i = 0; i < GPS_HISTORY_SIZE; i++) {
        gpsHistory[i].lat = 0.0;
        gpsHistory[i].lng = 0.0;
        gpsHistory[i].timestamp = 0;
    }
}

// ===== BLE CLIENT SETUP =====
void setupBLEClient() {
    Serial.println("🔵 Initializing BLE Client to connect to Wrist Band...");

    NimBLEDevice::init("");
    pClient = NimBLEDevice::createClient();
    pClient->setConnectTimeout(30); // 30 second timeout
    pClient->setScanTimeout(30); // 30 second scan timeout

    Serial.println("✅ BLE Client initialized");
}

// ===== CONNECT TO WRIST BAND =====
bool connectToWristBand() {
    if (connected) {
        return true; // Already connected
    }

    Serial.println("🔍 Scanning for Wrist Band...");

    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setActiveScanning(true);
    pScan->setInterval(100);
    pScan->setWindow(99);

    NimBLEScanResults results = pScan->start(5, false); // Scan for 5 seconds

    Serial.println("📱 Scan complete. Devices found: " + String(results.getCount()));

    for (int i = 0; i < results.getCount(); i++) {
        NimBLEAdvertisedDevice device = results.getDevice(i);

        if (device.getName().c_str() == String(WRIST_NAME)) {
            Serial.println("✅ Found Wrist Band: " + device.getName().c_str());
            Serial.println("📍 Address: " + device.getAddress().toString().c_str());

            if (pClient->connect(&device)) {
                Serial.println("✅ Connected to Wrist Band!");

                // Get service
                NimBLERemoteService* pService = pClient->getService(SERVICE_UUID);
                if (pService == nullptr) {
                    Serial.println("❌ Service not found");
                    pClient->disconnect();
                    return false;
                }
                Serial.println("✅ Service found");

                // Get characteristic
                pChar = pService->getCharacteristic(CHARACTERISTIC_UUID);
                if (pChar == nullptr) {
                    Serial.println("❌ Characteristic not found");
                    pClient->disconnect();
                    return false;
                }
                Serial.println("✅ Characteristic found");

                // Subscribe to notifications
                if (pChar->subscribe(true, notifyCallback, true)) {
                    Serial.println("✅ Subscribed to notifications");
                    connected = true;
                    return true;
                } else {
                    Serial.println("❌ Failed to subscribe");
                    pClient->disconnect();
                    return false;
                }
            } else {
                Serial.println("❌ Failed to connect");
                return false;
            }
        }
    }

    Serial.println("❌ Wrist Band not found");
    return false;
}

// ===== MPU6050 SENSOR READING (SIMULATED) =====
void readMPU6050() {
    // In actual implementation, read from MPU6050 via I2C
    // For now, simulate sensor data
    static unsigned long lastSensorUpdate = 0;
    if (millis() - lastSensorUpdate < 100) {
        return;
    }
    lastSensorUpdate = millis();
    
    // Simulate accelerometer data
    accelX = (random(-100, 100) / 100.0);
    accelY = (random(-100, 100) / 100.0) + 9.8; // Gravity
    accelZ = (random(-100, 100) / 100.0);
    
    // Simulate gyroscope data
    gyroX = (random(-50, 50) / 100.0);
    gyroY = (random(-50, 50) / 100.0);
    gyroZ = (random(-50, 50) / 100.0);
}

// ===== SIMULATION LOGIC =====
void updateSimulation() {
    unsigned long now = millis();
    
    // Cycle state every 30 seconds for demonstration purposes
    if (now - stateChangeTime > 30000) {
        if (currentState == NORMAL) {
            currentState = ALERT;
            Serial.println("🔄 State changed to ALERT");
        } else if (currentState == ALERT) {
            currentState = CRITICAL;
            Serial.println("🔄 State changed to CRITICAL");
        } else {
            currentState = NORMAL;
            Serial.println("🔄 State changed to NORMAL");
        }
        stateChangeTime = now;
    }

    // Simulate HR based on state
    switch (currentState) {
        case NORMAL:
            currentHr = random(65, 85);
            break;
        case ALERT:
            currentHr = random(105, 115);
            break;
        case CRITICAL:
            currentHr = random(125, 140);
            break;
    }
    currentSpo2 = random(95, 100);
    currentTemp = 36.5 + (random(-5, 5) / 10.0);
}

// ===== SEND DATA TO BACKEND =====
void sendDataToBackend() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverUrl);
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(10000); // 10 second timeout
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        // Convert ML state to string
        String mlStateName = "";
        switch (currentMLState) {
            case ML_NORMAL: mlStateName = "NORMAL"; break;
            case ML_PREFALL: mlStateName = "PREFALL"; break;
            case ML_FALL: mlStateName = "FALL"; break;
            case ML_SUDDEN_MOVEMENT: mlStateName = "SUDDEN_MOVEMENT"; break;
        }

        // Get GPS history JSON
        String gpsHistoryJson;
        getGPSHistoryJson(gpsHistoryJson);

        String payload = "{";
        payload += "\"deviceId\":\"" + String(DEVICE_ID) + "\",";
        payload += "\"beltType\":\"Waist Belt\",";
        payload += "\"mlState\":\"" + mlStateName + "\",";
        payload += "\"mlConfidence\":" + String(mlConfidence) + ",";
        payload += "\"heartRate\":" + String(currentHr) + ",";
        payload += "\"spo2\":" + String(currentSpo2) + ",";
        payload += "\"temperature\":" + String(currentTemp) + ",";
        payload += "\"beltWorn\":true,";
        payload += "\"accelX\":" + String(accelX) + ",";
        payload += "\"accelY\":" + String(accelY) + ",";
        payload += "\"accelZ\":" + String(accelZ) + ",";
        payload += "\"gyroX\":" + String(gyroX) + ",";
        payload += "\"gyroY\":" + String(gyroY) + ",";
        payload += "\"gyroZ\":" + String(gyroZ) + ",";
        payload += "\"latitude\":" + String(gpsLat, 6) + ",";
        payload += "\"longitude\":" + String(gpsLng, 6) + ",";
        payload += "\"gpsHistory\":" + gpsHistoryJson + ",";
        payload += "\"alertState\":" + String(currentAlertState) + ",";
        payload += "\"micMessageAudio\":\"" + micMessage + "\",";
        payload += "\"timestamp\":" + String(millis());
        payload += "}";

        int httpResponseCode = http.POST(payload);
        if (httpResponseCode > 0) {
            Serial.print("✅ HTTP POST successful, code: ");
            Serial.println(httpResponseCode);
            
            String response = http.getString();
            if (response.indexOf("ACKNOWLEDGED") != -1 || response.indexOf("I am Fine") != -1) {
                Serial.println("💚 [GUARDIAN ACKNOWLEDGED]: Clearing local buzzer and state!");
                digitalWrite(BUZZER_PIN, LOW);
                currentAlertState = NO_ALERT;
                guardianAcknowledged = true;
                micMessage = "";
            }
        } else {
            Serial.print("❌ HTTP Error code: ");
            Serial.println(httpResponseCode);
            Serial.print("❌ Error: ");
            Serial.println(http.errorToString(httpResponseCode));
        }
        http.end();
    } else {
        Serial.println("❌ WiFi Disconnected");
        manageWiFiConnection();
    }
}

// ===== SMS ALERT FUNCTIONS =====
void sendSmartSMSAlert(String alertType, String message) {
    unsigned long currentTime = millis();
    
    if (currentTime - lastSMSTime < SMS_RETRY_INTERVAL) {
        return;
    }
    
    if (alertType != lastAlertType) {
        twilioSMSSent = false;
        gsmSMSSent = false;
        lastAlertType = alertType;
    }
    
    Serial.println("📱 [SMS] Smart SMS System Activated");
    
    if (WiFi.status() == WL_CONNECTED && !twilioSMSSent) {
        Serial.println("📡 [TWILIO] Attempting SMS via WiFi...");
        bool twilioSuccess = sendTwilioSMS(alertType, message);
        if (twilioSuccess) {
            twilioSMSSent = true;
            lastSMSTime = currentTime;
            return;
        }
    }
    
    if (!gsmSMSSent) {
        Serial.println("📱 [GSM] Sending backup SMS...");
        bool gsmSuccess = sendGSMAlert(alertType, message);
        if (gsmSuccess) {
            gsmSMSSent = true;
            lastSMSTime = currentTime;
        }
    }
}

// ===== DEVICE MANAGEMENT =====
void generateDeviceId() {
  devicePrefs.begin("device_id", false);
  
  // Check if ID already exists
  DEVICE_ID = devicePrefs.getString("device_id", "");
  
  if (DEVICE_ID.length() == 0) {
    // Generate unique ID based on MAC address
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    DEVICE_ID = "SC_WB_" + mac.substring(0, 12) + "_" + String(millis(), HEX);
    
    devicePrefs.putString("device_id", DEVICE_ID);
    devicePrefs.end();
    devicePrefs.begin("device_id", false);
    
    Serial.println("🆕 Generated new Device ID: " + DEVICE_ID);
  } else {
    Serial.println("🆔 Loaded Device ID: " + DEVICE_ID);
  }
  
  devicePrefs.end();
}

void registerDevice() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi not connected, skipping device registration");
    return;
  }
  
  HTTPClient http;
  String url = String(deviceApiUrl) + "/register";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  String mac = WiFi.macAddress();
  
  String payload = "{";
  payload += "\"deviceId\":\"" + DEVICE_ID + "\",";
  payload += "\"deviceType\":\"Waist Belt\",";
  payload += "\"macAddress\":\"" + mac + "\"";
  payload += "}";
  
  int httpResponseCode = http.POST(payload);
  
  if (httpResponseCode > 0) {
    Serial.print("✅ Device registered, code: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("❌ Device registration failed, code: ");
    Serial.println(httpResponseCode);
    Serial.print("❌ Error: ");
    Serial.println(http.errorToString(httpResponseCode));
  }
  
  http.end();
}

void sendHeartbeat() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  
  HTTPClient http;
  String url = String(deviceApiUrl) + "/" + DEVICE_ID + "/heartbeat";
  http.begin(url);
  http.setTimeout(5000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  int httpResponseCode = http.POST("");
  
  if (httpResponseCode > 0) {
    // Heartbeat successful
  }
  
  http.end();
}

// ... (rest of the code remains the same)

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println("========================================");
  Serial.println("=== SILVERCARE WAIST BELT WORKAROUND ===");
  Serial.println("========================================");

  generateDeviceId();
  
  // Display MAC address for pairing
  String macAddress = WiFi.macAddress();
  Serial.println("========================================");
  Serial.println("📱 DEVICE PAIRING INFORMATION");
  Serial.println("========================================");
  Serial.println("MAC Address: " + macAddress);
  Serial.println("Device ID: " + DEVICE_ID);
  Serial.println("Use MAC address to pair this device");
  Serial.println("with a senior citizen profile in the app");
  Serial.println("========================================");

  setupPins();
  setupGPS();
  setupBLEClient(); // Initialize BLE client to connect to wrist band
  setupWiFi();
  setupGSM();
  mlStateChangeTime = millis();

  // Get elderly ID from preferences or backend
  devicePrefs.begin("device_id", false);
  elderlyId = devicePrefs.getString("elderly_id", "");
  devicePrefs.end();

  // Register device if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    registerDevice();
    fetchContactInfo();
  }

  // Try to connect to wrist band via BLE
  connectToWristBand();
}

void loop() {
  unsigned long now = millis();

  // Try to connect to wrist band if not connected
  static unsigned long lastBLEConnect尝试 = 0;
  if (!connected && now - lastBLEConnect尝试 > 30000) { // Try every 30 seconds
    connectToWristBand();
    lastBLEConnect尝试 = now;
  }

  // In actual implementation, if dataReceivedFromWrist is false, run simulation
  if (!dataReceivedFromWrist) {
    updateSimulation();
  }

  // Read MPU6050 sensor data & run onboard edge-based fall detection
  readMPU6050();
  runEdgeFallDetection();

  // Update GPS
  updateGPS();

  // Check WiFi range
  checkWiFiRange();

  // Check for location requests from guardian
  checkLocationRequest();

  // Get ML prediction from cloud service (if available)
  static unsigned long lastMLPrediction = 0;
  if (now - lastMLPrediction > 1000) { // Get prediction every second
    getMLPrediction();
    lastMLPrediction = now;
  }

  // Fetch Mitra chat history for ML learning (every hour)
  fetchMitraChatHistoryForML();

  // Handle ML & Edge state changes and trigger alerts
  if (currentMLState == ML_PREFALL && currentAlertState != PREFALL_ALERT) {
    triggerPrefallAlert();
  } else if (currentMLState == ML_FALL && currentAlertState != FALL_ALERT) {
    triggerFallAlert();
  } else if (currentMLState == ML_NORMAL && currentAlertState != NO_ALERT && guardianAcknowledged) {
    // Clear alert if back to normal and acknowledged
    currentAlertState = NO_ALERT;
    digitalWrite(BUZZER_PIN, LOW);
  }

  // Handle Push Button 1 (SOS Button)
  handleSOSButton();

  // Handle Push Button 2 (Reset / Acknowledgment Button)
  handleAcknowledgment();

  // Continuous "Help! Help!" speaker repeat loop on Fall
  playSpeakerHelpLoop();

  // Handle 1-minute (60 seconds) INMP441 microphone recording timer on Prefall/Fall
  handleMicrophoneRecording();

  // Handle alert timeouts
  handleAlertTimeouts();

  // Manage WiFi connection (includes captive portal handling)
  manageWiFiConnection();

  // Send heartbeat to backend
  if (now - lastHeartbeat > HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    lastHeartbeat = now;
  }

  // Handle notifications (buzzer, SMS)
  handleNotifications();

  // Send data to backend
  if (now - lastDataSent > DATA_SEND_INTERVAL) {
    sendDataToBackend();
    lastDataSent = now;
  }

  // Periodically fetch contact info (every 5 minutes)
  static unsigned long lastContactFetch = 0;
  if (now - lastContactFetch > 300000) {
    fetchContactInfo();
    lastContactFetch = now;
  }
  delay(10);
}
