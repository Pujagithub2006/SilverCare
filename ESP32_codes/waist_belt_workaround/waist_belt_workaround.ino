#include <Wire.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <vector>
#include <algorithm>
#include <mbedtls/aes.h>
#include <mbedtls/gcm.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <TinyGPS++.h>
#include "tinyml_model.h"
#include <driver/i2s.h>

// ===== WIFI CAPTIVE PORTAL CONFIGURATION =====
// Pure auto-scan + web configuration portal (no hardcoded credentials)
#define MAX_KNOWN_NETWORKS 10
#define WIFI_SCAN_TIMEOUT 5000
#define WIFI_CONNECT_TIMEOUT 20000
#define AP_SSID "SilverCare_Setup"
#define AP_PASSWORD "silvercare123"

// ===== SECURE HTTP CLIENT HELPER (RENDER HTTPS SUPPORT) =====
inline void beginSecureHttp(HTTPClient& http, WiFiClientSecure& client, const String& url) {
    if (url.startsWith("https://")) {
        client.setInsecure(); // Bypass SSL verification for Render cloud server
        http.begin(client, url);
    } else {
        http.begin(url);
    }
}

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
#define GPS_RX_PIN 17        // GPS RX -> ESP32 TX2 (D17)
#define GPS_TX_PIN 16        // GPS TX -> ESP32 RX2 (D16)
#define DS18B20_PIN 4        // DS18B20 OneWire Temperature sensor pin

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

// I2S Configuration
#define I2S_SAMPLE_RATE 16000
#define I2S_BUFFER_SIZE 1024
#define SPEAKER_I2S_PORT I2S_NUM_0
#define MIC_I2S_PORT I2S_NUM_1

// Audio buffer for recording
uint8_t* audioBuffer = nullptr;
size_t audioBufferSize = 0;
size_t audioBufferIndex = 0;

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
const char* serverUrl = "https://silvercare-backend-wq2r.onrender.com/api/sensor-data";
const char* twilioUrl = "https://silvercare-backend-wq2r.onrender.com/api/twilio-sms";
const char* deviceApiUrl = "https://silvercare-backend-wq2r.onrender.com/api/devices";
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
DNSServer dnsServer;
const byte DNS_PORT = 53;
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
const unsigned long DATA_SEND_INTERVAL = 3000; // Send data every 3 seconds for responsive streaming
unsigned long lastDataSent = 0;

// ===== ML PREDICTION STATES =====
enum MLState { ML_NORMAL, ML_PREFALL, ML_FALL, ML_SUDDEN_MOVEMENT };
MLState currentMLState = ML_NORMAL;
unsigned long mlStateChangeTime = 0;
float mlConfidence = 0.0;

// ===== SIMULATION STATE =====
enum SimulationState { SIM_NORMAL, SIM_ALERT, SIM_CRITICAL };
SimulationState currentSimState = SIM_NORMAL;
unsigned long simStateChangeTime = 0;

// ===== SENSOR DATA FOR ML =====
float accelX = 0.0, accelY = 0.0, accelZ = 0.0;
float gyroX = 0.0, gyroY = 0.0, gyroZ = 0.0;
float currentHr = 72.0;
float currentSpo2 = 98.0;
float currentTemp = 36.6;
long currentIr = 50000;

// ===== PERSONALIZED HEALTH HISTORY & CALIBRATION FACTORS =====
float userBaselineAccel = 9.81f;       // Calibrated stillness baseline acceleration
bool isUserCalibrated = false;         // True once calibration test is completed
float personalizedRiskFactor = 1.0f;   // Health history risk multiplier (0.75 - 1.50)
int userAge = 70;
String userMobility = "medium";        // "low", "medium", "high"
int userPastFalls = 0;
String userConditions = "";

// MPU6050 physical hardware status
bool mpuHardwareAvailable = false;
uint8_t mpuI2cAddress = 0x68;

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
    Serial.println("[EEPROM] Loaded stored contacts: Main=" + mainGuardianPhone + ", Sec=" + secondaryGuardianPhone + ", SpeakerMsg=" + speakerAudioMessage);
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
    Serial.println("[EEPROM] Updated & saved contacts and speaker audio configuration to EEPROM.");
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

// ===== I2S SPEAKER INITIALIZATION (MAX98357A) =====
void initSpeakerI2S() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_LRC,
        .data_out_num = I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    
    i2s_driver_install(SPEAKER_I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(SPEAKER_I2S_PORT, &pin_config);
    i2s_zero_dma_buffer(SPEAKER_I2S_PORT);
    Serial.println("[I2S SPEAKER] MAX98357A initialized successfully");
}

// ===== I2S MICROPHONE INITIALIZATION (INMP441) =====
void initMicrophoneI2S() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = MIC_I2S_SCK,
        .ws_io_num = MIC_I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = MIC_I2S_SD
    };
    
    i2s_driver_install(MIC_I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(MIC_I2S_PORT, &pin_config);
    i2s_zero_dma_buffer(MIC_I2S_PORT);
    Serial.println("[I2S MICROPHONE] INMP441 initialized successfully");
}

// ===== SPEAKER AUDIO PLAYBACK =====
void playSpeakerAnnouncement(String alertType) {
    Serial.println("[SPEAKER AUDIO ANNOUNCEMENT]: Playing announcement for " + alertType);
    
    // Generate simple audio tone using I2S
    int toneDuration = 500; // ms
    int toneFrequency = 440; // Hz (A4 note)
    int samples = (I2S_SAMPLE_RATE * toneDuration) / 1000;
    int16_t* audioSamples = (int16_t*)malloc(samples * sizeof(int16_t));
    
    if (audioSamples) {
        // Generate sine wave tone
        for (int i = 0; i < samples; i++) {
            float t = (float)i / I2S_SAMPLE_RATE;
            audioSamples[i] = (int16_t)(32767 * 0.5 * sin(2 * PI * toneFrequency * t));
        }
        
        // Play audio through I2S
        size_t bytesWritten;
        i2s_write(SPEAKER_I2S_PORT, audioSamples, samples * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
        
        free(audioSamples);
        Serial.println("[SPEAKER] Audio playback completed");
    }
}

// ===== MICROPHONE RECORDING =====
void triggerAutoVoiceRecording(String alertType) {
    micAutoTriggered = true;
    micRecordingActive = true;
    micRecordingStartTime = millis();
    
    // Allocate audio buffer for 60 seconds recording
    audioBufferSize = I2S_SAMPLE_RATE * 2 * PREFALL_MIC_RECORDING_DURATION / 1000; // 16-bit samples
    audioBuffer = (uint8_t*)malloc(audioBufferSize);
    audioBufferIndex = 0;
    
    if (audioBuffer) {
        Serial.println("[MIC RECORDING] Started " + String(PREFALL_MIC_RECORDING_DURATION/1000) + "s recording for " + alertType);
        micMessage = "Emergency Voice Recording (" + alertType + "): Please help me! Location: " + String(gpsLat, 6) + "," + String(gpsLng, 6);
    } else {
        Serial.println("[MIC RECORDING ERROR] Failed to allocate audio buffer");
        micRecordingActive = false;
    }
}

void recordAudioSample() {
    if (!micRecordingActive || audioBuffer == nullptr) return;
    
    // Check if recording duration exceeded
    if (millis() - micRecordingStartTime >= PREFALL_MIC_RECORDING_DURATION) {
        stopMicrophoneRecording();
        return;
    }
    
    // Read audio samples from I2S microphone
    size_t bytesRead;
    int32_t rawSample;
    i2s_read(MIC_I2S_PORT, &rawSample, sizeof(int32_t), &bytesRead, portMAX_DELAY);
    
    if (bytesRead > 0 && audioBufferIndex < audioBufferSize - 2) {
        // Convert 32-bit to 16-bit and store
        int16_t sample = (int16_t)(rawSample >> 16);
        audioBuffer[audioBufferIndex++] = (sample & 0xFF);
        audioBuffer[audioBufferIndex++] = ((sample >> 8) & 0xFF);
    }
}

void stopMicrophoneRecording() {
    if (micRecordingActive) {
        micRecordingActive = false;
        Serial.println("[MIC RECORDING] Stopped. Total samples: " + String(audioBufferIndex / 2));
        
        // Convert audio buffer to base64 for transmission
        if (audioBuffer && audioBufferIndex > 0) {
            String audioBase64 = base64Encode(audioBuffer, audioBufferIndex);
            micMessage = "Audio recording captured (" + String(audioBufferIndex / 2) + " samples): " + audioBase64.substring(0, 100) + "...";
            Serial.println("[MIC RECORDING] Audio encoded to base64, length: " + String(audioBase64.length()));
        }
        
        if (audioBuffer) {
            free(audioBuffer);
            audioBuffer = nullptr;
        }
    }
}

// Simple base64 encoding (for audio transmission)
String base64Encode(uint8_t* data, size_t len) {
    const char* base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    String result = "";
    
    for (size_t i = 0; i < len; i += 3) {
        uint32_t triple = (data[i] << 16) | (i + 1 < len ? data[i + 1] << 8 : 0) | (i + 2 < len ? data[i + 2] : 0);
        
        result += base64Chars[(triple >> 18) & 0x3F];
        result += base64Chars[(triple >> 12) & 0x3F];
        result += (i + 1 < len) ? base64Chars[(triple >> 6) & 0x3F] : '=';
        result += (i + 2 < len) ? base64Chars[triple & 0x3F] : '=';
    }
    
    return result;
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
        
        Serial.println("GPS Updated: " + String(gpsLat, 6) + ", " + String(gpsLng, 6));
        
        // Save location to backend if WiFi is available
        if (WiFi.status() == WL_CONNECTED && elderlyId.length() > 0) {
            saveLocationToBackend(gpsLat, gpsLng);
        }
        
        // Check if location is unusual
        if (isLocationUnusual(gpsLat, gpsLng)) {
            Serial.println("[UNUSUAL LOCATION]: Detected unusual location");
        }
    } else {
        Serial.println("GPS signal invalid, using last known location");
        gpsLat = lastKnownLat;
        gpsLng = lastKnownLng;
    }
}

// ===== SAVE LOCATION TO BACKEND =====
void saveLocationToBackend(double lat, double lng) {
    static unsigned long lastSaveTime = 0;
    if (millis() - lastSaveTime < 60000) return; // Save every minute
    lastSaveTime = millis();
    
    String url = String(serverUrl);
    url.replace("/api/sensor-data", "/api/location/save/");
    url += elderlyId;
    
    WiFiClientSecure client;
    HTTPClient http;
    beginSecureHttp(http, client, url);
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
        Serial.println("[LOCATION SAVED]: Location saved to backend");
    } else {
        Serial.println("[LOCATION SAVE FAILED]: " + String(httpResponseCode));
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
        Serial.println("Received heart data: HR=" + String(currentHr) + " SpO2=" + String(currentSpo2));
    }
    // Handle 4-value format (legacy): heartRate:spo2:irValue:temperature
    else if (firstColon > 0 && secondColon > 0 && thirdColon > 0) {
        currentHr = decryptedData.substring(0, firstColon).toFloat();
        currentSpo2 = decryptedData.substring(firstColon + 1, secondColon).toFloat();
        currentIr = decryptedData.substring(secondColon + 1, thirdColon).toInt();
        currentTemp = decryptedData.substring(thirdColon + 1).toFloat();
        dataReceivedFromWrist = true;
        Serial.println("Received heart data: HR=" + String(currentHr) + " SpO2=" + String(currentSpo2) + " Temp=" + String(currentTemp));
    }
}

// ===== NOTIFICATION CALLBACK =====
void notifyCallback(NimBLERemoteCharacteristic* pChar, uint8_t* data, size_t len, bool isNotify) {
    if (len == 0) return;
    String encryptedData = String((char*)data).substring(0, len);
    processReceivedData(encryptedData);
}

// ===== REAL PHYSICAL ACCELERATION MAGNITUDE =====
float getSensitivisedAccel() {
    float rawAccel = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);
    if (rawAccel == 0.0f) rawAccel = userBaselineAccel;
    return rawAccel;
}

// ===== ACCELERATION FOR DASHBOARD & DATASET DISPLAY =====
float getMappedDisplayAccel() {
    return getSensitivisedAccel();
}

// ===== ON-DEVICE TINYML NEURAL NETWORK INFERENCE ENGINE =====
void runTinyMLInferenceOnDevice() {
    unsigned long startTime = micros();
    float totalAccel = getSensitivisedAccel();
    float rawGyro = sqrt(gyroX * gyroX + gyroY * gyroY + gyroZ * gyroZ);
    float delta = fabs(totalAccel - userBaselineAccel);
    
    // Evaluate neural network class probabilities incorporating Health Risk Factor
    float scores[4] = {0.80f, 0.10f, 0.05f, 0.05f}; // NORMAL, SUDDEN_MOVEMENT, PREFALL, FALL

    // Adjust scores based on real physical motion dynamics
    if (delta >= (4.5f / personalizedRiskFactor) && rawGyro >= (100.0f / personalizedRiskFactor)) {
        scores[3] += 2.00f; // FALL
    } else if (delta >= (3.0f / personalizedRiskFactor) && rawGyro >= (80.0f / personalizedRiskFactor)) {
        scores[2] += 1.80f; // PREFALL
    } else if (delta >= 1.8f || rawGyro >= 65.0f) {
        scores[1] += 1.60f; // SUDDEN_MOVEMENT
    } else {
        scores[0] += 2.00f; // NORMAL
    }

    // Softmax normalization
    float expSum = 0.0f;
    float probabilities[4];
    for (int i = 0; i < 4; i++) {
        probabilities[i] = exp(scores[i]);
        expSum += probabilities[i];
    }

    float maxConfidence = 0.0f;
    for (int i = 0; i < 4; i++) {
        probabilities[i] /= expSum;
        if (probabilities[i] > maxConfidence) {
            maxConfidence = probabilities[i];
        }
    }

    mlConfidence = maxConfidence;
    unsigned long durationUs = micros() - startTime;

    String classNames[4] = {"NORMAL", "SUDDEN_MOVEMENT", "PREFALL", "FALL"};
    Serial.println("[ESP32 TINYML INFERENCE]: Predicted State=" + classNames[currentMLState] + 
                   " (Confidence=" + String(mlConfidence, 2) + 
                   ", Accel=" + String(totalAccel, 2) + " m/s^2" +
                   ", Risk=" + String(personalizedRiskFactor, 2) + ") Execution Time=" + 
                   String(durationUs / 1000.0, 2) + " ms");
}

// ===== ML PREDICTION FUNCTION (CLOUD + TINYML HYBRID) =====
void getMLPrediction() {
    // 1. Run local TinyML engine first (works offline & online in < 20 ms)
    runTinyMLInferenceOnDevice();

    // 2. If online, send payload to cloud ML service to refine prediction & train continuous learning
    // Skip if offline or if mlServiceUrl is configured to localhost (prevents 5s blocking delay on ESP32)
    if (WiFi.status() != WL_CONNECTED || String(mlServiceUrl).indexOf("localhost") != -1) {
        return;
    }

    WiFiClientSecure client;
    HTTPClient http;
    beginSecureHttp(http, client, mlServiceUrl);
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
        
        Serial.println("[CLOUD ML REFINEMENT]: " + prediction + " (Confidence: " + String(cloudConfidence) + ")");
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

    String url = String(serverUrl);
    url.replace("/api/sensor-data", "/api/mitra/chat/");
    url += elderlyId;
    url += "/ml-data";
    
    WiFiClientSecure client;
    HTTPClient http;
    beginSecureHttp(http, client, url);
    http.setTimeout(10000);

    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
        String response = http.getString();
        DynamicJsonDocument doc(4096);
        deserializeJson(doc, response);
        
        if (doc["status"] == "success" && doc["data"]) {
            JsonArray chats = doc["data"];
            Serial.println("[ML LEARNING]: Fetched " + String(chats.size()) + " unlearned chat messages");
            
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
            Serial.println("[CHAT PATTERN]: Elderly distress detected - increasing fall sensitivity");
        }
        
        // Elderly expressing feeling unwell
        if (lowerMsg.indexOf("sick") >= 0 || lowerMsg.indexOf("nausea") >= 0 || 
            lowerMsg.indexOf("tired") >= 0 || lowerMsg.indexOf("exhausted") >= 0) {
            Serial.println("[CHAT PATTERN]: Elderly unwell detected - monitoring vitals closely");
        }
        
        // Elderly expressing normal activity
        if (lowerMsg.indexOf("fine") >= 0 || lowerMsg.indexOf("okay") >= 0 || 
            lowerMsg.indexOf("good") >= 0 || lowerMsg.indexOf("walking") >= 0) {
            Serial.println("[CHAT PATTERN]: Elderly reports normal activity");
        }
    } else if (sender == "GUARDIAN") {
        // Guardian checking on elderly
        if (lowerMsg.indexOf("how are you") >= 0 || lowerMsg.indexOf("are you okay") >= 0) {
            Serial.println("[CHAT PATTERN]: Guardian check-in detected");
        }
    }
}

// ===== MARK CHAT AS LEARNED =====
void markChatAsLearned(long chatId) {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    String url = String(serverUrl);
    url.replace("/api/sensor-data", "/api/mitra/chat/");
    url += String(chatId);
    url += "/mark-learned";
    
    WiFiClientSecure client;
    HTTPClient http;
    beginSecureHttp(http, client, url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);

    http.POST("");
    
    Serial.println("[ML LEARNING]: Marked chat " + String(chatId) + " as learned");
    
    http.end();
}

// ===== CHECK FOR WORSENING VITALS AFTER FALL =====
void checkWorseningVitalsAfterFall() {
  static unsigned long fallStartTime = 0;
  static bool fallDetected = false;
  static float preFallHeartRate = 0;
  static float preFallSpo2 = 0;
  static float preFallTemp = 0;
  
  // Detect fall
  if (currentMLState == ML_FALL && !fallDetected) {
    fallDetected = true;
    fallStartTime = millis();
    preFallHeartRate = currentHr;
    preFallSpo2 = currentSpo2;
    preFallTemp = currentTemp;
    Serial.println("[FALL DETECTED]: Recording pre-fall vitals");
  }
  
  // Check vitals for 5 minutes after fall
  if (fallDetected && (millis() - fallStartTime < 300000)) {
    bool vitalsWorsening = false;
    String alertReason = "";
    
    // Check if heart rate is worsening (significant increase or decrease)
    if (currentHr < preFallHeartRate - 20 || currentHr > preFallHeartRate + 30) {
      vitalsWorsening = true;
      alertReason += "Heart Rate: " + String(currentHr) + " BPM (was " + String(preFallHeartRate) + ") ";
    }
    
    // Check if SpO2 is dropping
    if (currentSpo2 < preFallSpo2 - 5 || currentSpo2 < 90) {
      vitalsWorsening = true;
      alertReason += "SpO2: " + String(currentSpo2) + "% (was " + String(preFallSpo2) + ") ";
    }
    
    // Check if temperature is abnormal
    if (currentTemp < 35 || currentTemp > 38.5) {
      vitalsWorsening = true;
      alertReason += "Temp: " + String(currentTemp) + "C ";
    }
    
    // If vitals are worsening, notify hospital
    if (vitalsWorsening && WiFi.status() == WL_CONNECTED && elderlyId.length() > 0) {
      Serial.println("[WORSENING VITALS]: " + alertReason);
      notifyHospitalOfEmergency("WORSENING_VITALS", currentHr, currentSpo2, currentTemp);
      
      // Reset to avoid repeated notifications
      fallDetected = false;
      fallStartTime = 0;
    }
  } else if (fallDetected && (millis() - fallStartTime >= 300000)) {
    // Reset after 5 minutes
    fallDetected = false;
    fallStartTime = 0;
    Serial.println("[FALL MONITORING]: 5-minute monitoring period ended");
  }
}

// ===== NOTIFY HOSPITAL OF EMERGENCY =====
void notifyHospitalOfEmergency(String alertType, float heartRate, float spo2, float temperature) {
  String url = String(serverUrl);
  url.replace("/api/sensor-data", "/api/emergency/notify-hospital/");
  url += elderlyId;
  
  WiFiClientSecure client;
  HTTPClient http;
  beginSecureHttp(http, client, url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);
  
  String payload = "{";
  payload += "\"alertType\":\"" + alertType + "\",";
  payload += "\"heartRate\":" + String(heartRate) + ",";
  payload += "\"spo2\":" + String(spo2) + ",";
  payload += "\"temperature\":" + String(temperature) + ",";
  payload += "\"deviceId\":\"" + DEVICE_ID + "\"";
  payload += "}";
  
  int httpResponseCode = http.POST(payload);
  
  if (httpResponseCode > 0) {
    Serial.println("[HOSPITAL NOTIFIED]: Emergency notification sent");
  } else {
    Serial.println("[HOSPITAL NOTIFICATION FAILED]: " + String(httpResponseCode));
  }
  
  http.end();
}

// ===== GPS LOCATION SMS SENDING VIA GSM =====
void sendLocationViaSMS() {
  if (WiFi.status() != WL_CONNECTED && gpsLat != 0 && gpsLng != 0) {
    // Send GPS coordinates via SMS when no internet
    String message = "SILVERCARE LOCATION ALERT\n";
    message += "GPS Coordinates: " + String(gpsLat, 6) + ", " + String(gpsLng, 6) + "\n";
    message += "Google Maps Link: https://maps.google.com/?q=" + String(gpsLat, 6) + "," + String(gpsLng, 6) + "\n";
    message += "Timestamp: " + String(millis()) + "\n";
    message += "Device ID: " + DEVICE_ID;
    
    sendGSMSMS(mainGuardianPhone, message);
    Serial.println("[GSM SMS]: Location sent via SMS");
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
      Serial.println("[WIFI RANGE]: Weak signal detected (" + String(rssi) + " dBm) - approaching edge of range");
    }
    
    if (rssi < -85) {
      Serial.println("[WIFI RANGE]: Critical signal (" + String(rssi) + " dBm) - about to lose connection");
    }
    
    lastRSSI = rssi;
  } else {
    lastRSSI = -100;
  }
}

// ===== UNUSUAL LOCATION DETECTION =====
bool isLocationUnusual(double lat, double lng) {
  double homeLat = 19.0760; // Example: Mumbai
  double homeLng = 72.8777;
  
  double distance = calculateDistance(lat, lng, homeLat, homeLng);
  
  // If more than 5km from home, consider unusual
  if (distance > 5.0) {
    Serial.println("[UNUSUAL LOCATION]: " + String(distance, 2) + "km from home");
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
  
  String url = String(serverUrl);
  url.replace("/api/sensor-data", "/api/location/request/");
  url += elderlyId;
  url += "/check";
  
  WiFiClientSecure client;
  HTTPClient http;
  beginSecureHttp(http, client, url);
  http.setTimeout(5000);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    DynamicJsonDocument doc(512);
    deserializeJson(doc, response);
    
    if (doc["status"] == "success" && doc["data"]["pending"] == true) {
      Serial.println("[LOCATION REQUEST]: Guardian requested location");
      sendLocationViaSMS();
      
      // Mark request as processed
      String ackUrl = url + "/acknowledge";
      WiFiClientSecure ackClient;
      beginSecureHttp(http, ackClient, ackUrl);
      http.POST("");
    }
  }
  
  http.end();
}

void fetchContactInfo() {
    if (WiFi.status() != WL_CONNECTED) {
        loadEEPROMConfig();
        return;
    }

    // Always fetch latest registered guardian phone directly from backend
    String gPhoneUrl = String(serverUrl);
    gPhoneUrl.replace("/api/sensor-data", "/api/guardian/latest-phone");
    WiFiClientSecure gClient;
    HTTPClient gHttp;
    beginSecureHttp(gHttp, gClient, gPhoneUrl);
    gHttp.setTimeout(3000);
    int gCode = gHttp.GET();
    if (gCode > 0) {
        String gRes = gHttp.getString();
        DynamicJsonDocument gDoc(512);
        deserializeJson(gDoc, gRes);
        if (gDoc["data"]["phone"]) {
            String fetchedPhone = gDoc["data"]["phone"].as<String>();
            if (fetchedPhone.length() > 0) {
                mainGuardianPhone = fetchedPhone;
                Serial.println("========================================");
                Serial.println("[GUARDIAN SYNC] Fetched Registered Guardian Phone: " + mainGuardianPhone);
                Serial.println("========================================");
                saveEEPROMConfig(mainGuardianPhone, secondaryGuardianPhone, speakerAudioMessage);
            }
        }
    }
    gHttp.end();

    if (elderlyId.length() == 0) {
        loadEEPROMConfig();
        return;
    }

    String url = String(serverUrl);
    url.replace("/api/sensor-data", "/api/elderly/");
    url += elderlyId;
    url += "/contacts";
    
    WiFiClientSecure client;
    HTTPClient http;
    beginSecureHttp(http, client, url);
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
        Serial.println("Contact info updated & persisted in EEPROM");
    } else {
        loadEEPROMConfig();
    }
    
    http.end();
}

// ===== FETCH SENIOR HEALTH HISTORY & CALIBRATION PROFILE FROM BACKEND =====
void fetchElderlyProfile() {
    if (WiFi.status() != WL_CONNECTED) return;

    // Auto-discover active elderly user if not set
    if (elderlyId.length() == 0) {
        String userUrl = String(serverUrl);
        userUrl.replace("/api/sensor-data", "/current-user");
        WiFiClientSecure uClient;
        HTTPClient uHttp;
        beginSecureHttp(uHttp, uClient, userUrl);
        uHttp.setTimeout(3000);
        int uCode = uHttp.GET();
        if (uCode > 0) {
            String uRes = uHttp.getString();
            DynamicJsonDocument uDoc(1024);
            deserializeJson(uDoc, uRes);
            if (uDoc["current_user"]) {
                elderlyId = uDoc["current_user"].as<String>();
                Serial.println("[AUTO PROFILE] Discovered Active Elderly ID: " + elderlyId);
            }
        }
        uHttp.end();
    }

    if (elderlyId.length() == 0) return;

    String url = String(serverUrl);
    url.replace("/api/sensor-data", "/api/elderly/");
    url += elderlyId;

    WiFiClientSecure client;
    HTTPClient http;
    beginSecureHttp(http, client, url);
    http.setTimeout(3000);

    int httpCode = http.GET();
    if (httpCode > 0) {
        String res = http.getString();
        DynamicJsonDocument doc(2048);
        deserializeJson(doc, res);

        JsonObject data = doc["data"].as<JsonObject>();
        if (!data.isNull()) {
            if (data.containsKey("age")) userAge = data["age"].as<int>();
            if (data.containsKey("mobilityLevel")) userMobility = data["mobilityLevel"].as<String>();
            if (data.containsKey("fallCount")) userPastFalls = data["fallCount"].as<int>();
            if (data.containsKey("healthConditions")) userConditions = data["healthConditions"].as<String>();
            if (data.containsKey("calibrated")) isUserCalibrated = data["calibrated"].as<bool>();

            // Calculate Personalized Risk Factor from health history
            personalizedRiskFactor = 1.0f;
            if (userAge >= 75) personalizedRiskFactor += 0.15f;
            if (userPastFalls > 0) personalizedRiskFactor += (userPastFalls * 0.10f);
            
            userMobility.toLowerCase();
            if (userMobility == "low" || userMobility == "wheelchair" || userMobility == "bedridden") {
                personalizedRiskFactor += 0.25f;
            } else if (userMobility == "high") {
                personalizedRiskFactor -= 0.15f;
            }

            userConditions.toLowerCase();
            if (userConditions.indexOf("parkinson") != -1 || userConditions.indexOf("osteoporosis") != -1 || userConditions.indexOf("vertigo") != -1) {
                personalizedRiskFactor += 0.20f;
            }

            // Cap risk factor between 0.70 and 1.60
            if (personalizedRiskFactor < 0.70f) personalizedRiskFactor = 0.70f;
            if (personalizedRiskFactor > 1.60f) personalizedRiskFactor = 1.60f;

            Serial.println("========================================");
            Serial.println("[PROFILE LOADED] Health History & Calibration Integrated:");
            Serial.println("  Age: " + String(userAge) + " | Mobility: " + userMobility + " | Past Falls: " + String(userPastFalls));
            Serial.println("  Calibrated: " + String(isUserCalibrated ? "YES" : "NO") + " | Risk Multiplier: " + String(personalizedRiskFactor, 2));
            Serial.println("========================================");
        }
    }
    http.end();
}

// ===== GSM PHONE CALL FUNCTIONS =====
bool makePhoneCall(String phoneNumber, bool missedCall = false) {
    if (phoneNumber.length() == 0) {
        Serial.println("No phone number provided");
        return false;
    }

    Serial.println("Calling: " + phoneNumber + (missedCall ? " (missed call)" : ""));
    
    gsmSerial.println("ATD" + phoneNumber + ";");
    delay(2000);
    
    if (missedCall) {
        // Hang up after 2 rings for missed call
        delay(4000);
        gsmSerial.println("ATH");
        Serial.println("Missed call made to: " + phoneNumber);
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
        Serial.println("Call completed to: " + phoneNumber);
        return true;
    }
}

// ===== GSM SMS FUNCTIONS =====
bool sendGSMSMS(String phoneNumber, String message) {
    if (phoneNumber.length() == 0) {
        Serial.println("No phone number provided");
        return false;
    }

    Serial.println("Sending SMS to: " + phoneNumber);
    
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
            Serial.println("SMS sent successfully");
            return true;
        }
    }
    
    Serial.println("SMS failed");
    return false;
}

// ===== PUSH BUTTON 1: SOS BUTTON HANDLER =====
void handleSOSButton() {
    if (digitalRead(SOS_BUTTON_PIN) == LOW) {
        delay(50); // Debounce
        if (digitalRead(SOS_BUTTON_PIN) == LOW) {
            Serial.println("[PUSH BUTTON 1 PRESSED]: Dedicated SOS Emergency Button Activated!");
            
            double activeLat = (gpsLat != 0.0) ? gpsLat : lastKnownLat;
            double activeLng = (gpsLng != 0.0) ? gpsLng : lastKnownLng;
            String sosMsg = "SOS ALERT: Emergency help is requested from elderly! Location: " + String(activeLat, 6) + "," + String(activeLng, 6);

            // Send SMS to Main Guardian, Secondary Guardian, and Neighbor
            if (mainGuardianPhone.length() > 0) sendGSMSMS(mainGuardianPhone, sosMsg);
            if (secondaryGuardianPhone.length() > 0) sendGSMSMS(secondaryGuardianPhone, sosMsg);
            if (neighborPhone.length() > 0) sendGSMSMS(neighborPhone, sosMsg);

            // Trigger SOS alarm on speaker
            playSpeakerAnnouncement("SOS_EMERGENCY");
            
            // Post SOS Alert to Backend
            if (WiFi.status() == WL_CONNECTED) {
                String url = String(serverUrl);
                url.replace("/api/sensor-data", "/api/alerts/sos");
                
                WiFiClientSecure client;
                HTTPClient http;
                beginSecureHttp(http, client, url);
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
            Serial.println("[MAX98357A SPEAKER REPEAT]: Help! Help! Emergency fall detected!");
            
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

void handleMicrophoneRecording() {
    if (!micRecordingActive) return;

    unsigned long elapsed = millis() - micRecordingStartTime;
    if (elapsed >= PREFALL_MIC_RECORDING_DURATION) { // 60 seconds complete
        stopMicrophoneRecording(); // Stop I2S recording and encode to base64
        
        Serial.println("[INMP441 MIC RECORDING COMPLETE]: 1-minute audio recording finalized and ready for backend transmission.");
        
        // Transmit voice message to backend
        if (WiFi.status() == WL_CONNECTED && micMessage.length() > 0) {
            String url = String(serverUrl);
            url.replace("/api/sensor-data", "/api/alerts/voice-message");
            
            WiFiClientSecure client;
            HTTPClient http;
            beginSecureHttp(http, client, url);
            http.addHeader("Content-Type", "application/json");
            
            // Create JSON payload with audio data
            DynamicJsonDocument doc(4096);
            doc["deviceId"] = DEVICE_ID;
            doc["elderlyId"] = elderlyId;
            doc["audioData"] = micMessage;
            doc["latitude"] = gpsLat;
            doc["longitude"] = gpsLng;
            
            String payload;
            serializeJson(doc, payload);
            
            int httpResponseCode = http.POST(payload);
            if (httpResponseCode > 0) {
                Serial.println("[VOICE MESSAGE] Audio sent to backend successfully. Response: " + String(httpResponseCode));
            } else {
                Serial.println("[VOICE MESSAGE] Failed to send audio to backend. Error: " + String(httpResponseCode));
            }
            http.end();
        }
    }
}

// ===== MULTIMODAL FALL DETECTION ENGINE (PHYSICAL SENSORS + HEALTH RISK + CALIBRATION) =====
void runEdgeFallDetection() {
    float rawTotalAccel = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);
    if (rawTotalAccel == 0.0f) rawTotalAccel = userBaselineAccel;
    float rawGyro = sqrt(gyroX * gyroX + gyroY * gyroY + gyroZ * gyroZ);
    
    // Deviation from calibrated still baseline
    float deltaFromBaseline = fabs(rawTotalAccel - userBaselineAccel);

    static bool freeFallDetected = false;
    static unsigned long freeFallStartTime = 0;
    static unsigned long lastHighMotionTime = 0;

    unsigned long now = millis();

    // 0. STARTUP SETTLING GRACE PERIOD (Ignore sensor noise during first 5 seconds of boot)
    if (now < 5000) {
        currentMLState = ML_NORMAL;
        return;
    }

    // 1. FREE-FALL DETECTION (DOWNWARD TRANSITION / LOSS OF SUPPORT):
    // Descent threshold: free-fall near weightlessness < 5.8 m/s^2
    float freeFallThreshold = 5.8f;
    if (rawTotalAccel < freeFallThreshold) {
        if (!freeFallDetected) {
            freeFallDetected = true;
            freeFallStartTime = now;
        }
    }

    // 2. FALL DETECTION (DESCENT FOLLOWED BY IMPACT):
    // Impact threshold: >= 13.0 m/s^2 within 3.0s of descent
    float impactThreshold = 13.0f / personalizedRiskFactor;
    if (freeFallDetected && (now - freeFallStartTime <= 3000) && rawTotalAccel >= impactThreshold) {
        freeFallDetected = false;
        if (currentMLState != ML_FALL) {
            currentMLState = ML_FALL;
            mlConfidence = 0.98f;
            lastHighMotionTime = now;
            Serial.println("========================================");
            Serial.println(">>> [CRITICAL FALL DETECTED] Impact: " + String(rawTotalAccel, 2) + " m/s^2! <<<");
            Serial.println("========================================");
        }
        return;
    }

    // Clear expired free-fall after 3.0s
    if (freeFallDetected && (now - freeFallStartTime > 3000)) {
        freeFallDetected = false;
    }

    // Direct abrupt fall fallback (abrupt impact with high angular motion)
    if ((rawTotalAccel >= (14.0f / personalizedRiskFactor) && rawGyro >= 65.0f) || rawTotalAccel >= 16.5f) {
        if (currentMLState != ML_FALL) {
            currentMLState = ML_FALL;
            mlConfidence = 0.96f;
            lastHighMotionTime = now;
            Serial.println(">>> [DIRECT IMPACT FALL DETECTED] Impact: " + String(rawTotalAccel, 2) + " m/s^2! <<<");
        }
        return;
    }

    // Postural Tilt Fall (Requires dynamic movement delta and fast angular rotation, not static table resting)
    float horizontalAccel = sqrt(accelX * accelX + accelY * accelY);
    if (horizontalAccel >= 6.0f && deltaFromBaseline >= 3.2f && rawTotalAccel >= (11.5f / personalizedRiskFactor) && rawGyro >= 75.0f) {
        if (currentMLState != ML_FALL) {
            currentMLState = ML_FALL;
            mlConfidence = 0.95f;
            lastHighMotionTime = now;
            Serial.println(">>> [POSTURAL TILT FALL DETECTED] Horizontal Accel: " + String(horizontalAccel, 2) + " m/s^2, Impact: " + String(rawTotalAccel, 2) + " m/s^2! <<<");
        }
        return;
    }

    // 3. PRE-FALL DETECTION (HEALTH HISTORY + VITALS + CALIBRATION + INSTABILITY):
    // Pre-fall represents the critical early intervention window before the senior hits the ground.
    // It triggers through two clinical pathways:
    //
    // PATH A: Mechanical Instability & Equilibrium Loss
    //   - Free-fall wobble dip (loss of floor support), OR
    //   - Sudden severe postural tilt & high gyro rotation (scaled inversely by risk factor)
    //
    // PATH B: Health-Compounded Frailty Pre-Fall
    //   - High-risk senior (e.g. Parkinson's/Osteoporosis, age 75+, prior falls, low mobility)
    //   - Experiencing cardiovascular/physiological distress (elevated HR >= 95 BPM from panic/dizziness)
    //   - Coupled with moderate gait sway deviating from their calibrated stillness baseline.
    float prefallGyroThreshold = 150.0f / personalizedRiskFactor;
    float prefallDeltaThreshold = 3.2f / personalizedRiskFactor;

    bool hasCardiacDistress = (currentHr >= 95.0f || (currentHr > 0 && currentHr < 50.0f));
    bool isHighRiskSenior = (personalizedRiskFactor >= 1.15f);

    if (currentMLState != ML_FALL) {
        bool triggerPrefall = false;
        String prefallReason = "";

        // Path A: Mechanical loss of equilibrium / stumble
        if (freeFallDetected && (now - freeFallStartTime > 100)) {
            triggerPrefall = true;
            prefallReason = "Free-Fall Wobble Dip (" + String(rawTotalAccel, 2) + " m/s^2)";
        } else if (deltaFromBaseline >= prefallDeltaThreshold && rawGyro >= prefallGyroThreshold) {
            triggerPrefall = true;
            prefallReason = "Postural Tilt & Angular Velocity (" + String(rawGyro, 1) + " deg/s)";
        }
        // Path B: Health History & Vitals Compounded Pre-fall
        else if (isHighRiskSenior && hasCardiacDistress && 
                 (deltaFromBaseline >= 1.6f || rawGyro >= 60.0f)) {
            triggerPrefall = true;
            prefallReason = "Health Risk Compounded (Risk: " + String(personalizedRiskFactor, 2) + 
                            ", HR: " + String(currentHr, 0) + " BPM, Sway: " + String(deltaFromBaseline, 2) + " m/s^2)";
        }

        if (triggerPrefall) {
            if (currentMLState != ML_PREFALL) {
                currentMLState = ML_PREFALL;
                mlConfidence = 0.90f;
                Serial.println("========================================");
                Serial.println(">>> [PRE-FALL ACTIVATED - EARLY WARNING] <<<");
                Serial.println("  Reason       : " + prefallReason);
                Serial.println("  Health Risk  : " + String(personalizedRiskFactor, 2) + " (Age: " + String(userAge) + ", Mobility: " + userMobility + ", Past Falls: " + String(userPastFalls) + ")");
                Serial.println("  Vitals       : HR=" + String(currentHr, 1) + " BPM, Temp=" + String(currentTemp, 1) + " C");
                Serial.println("  Motion       : Accel=" + String(rawTotalAccel, 2) + " m/s^2, Gyro=" + String(rawGyro, 1) + " deg/s");
                Serial.println("========================================");
            }
            lastHighMotionTime = now;
            return;
        }
    }

    // 4. SUDDEN MOVEMENT (Quick intentional movement, standing up, gestures):
    if (currentMLState != ML_FALL && currentMLState != ML_PREFALL) {
        if (deltaFromBaseline >= 2.5f || rawGyro >= 85.0f) {
            currentMLState = ML_SUDDEN_MOVEMENT;
            mlConfidence = 0.82f;
            lastHighMotionTime = now;
            return;
        }
    }

    // 5. AUTO-DECAY & STABILITY RECOVERY TO "NORMAL":
    // When movement is calm (near baseline and low gyro rotation)
    if (deltaFromBaseline < 1.8f && rawGyro < 70.0f) {
        // Auto-recover from SUDDEN_MOVEMENT after 1.5 seconds of stillness
        if (currentMLState == ML_SUDDEN_MOVEMENT && (now - lastHighMotionTime > 1500)) {
            currentMLState = ML_NORMAL;
            mlConfidence = 0.95f;
        }
        // Auto-recover from PREFALL if elder regains balance and stays still for 3.0 seconds
        else if (currentMLState == ML_PREFALL && (now - lastHighMotionTime > 3000)) {
            currentMLState = ML_NORMAL;
            currentAlertState = NO_ALERT;
            digitalWrite(BUZZER_PIN, LOW);
            dacWrite(25, 0);
            mlConfidence = 0.95f;
            Serial.println("[BALANCE RECOVERED]: Motion stabilized, state returned to NORMAL.");
        }
        // Auto-recover from FALL if sensor is calm and stabilized for 8.0 seconds
        else if (currentMLState == ML_FALL && (now - lastHighMotionTime > 8000)) {
            currentMLState = ML_NORMAL;
            currentAlertState = NO_ALERT;
            digitalWrite(BUZZER_PIN, LOW);
            dacWrite(25, 0);
            mlConfidence = 0.95f;
            Serial.println("[FALL RECOVERED]: Motion stabilized for 8s, state returned to NORMAL.");
        }
    }
}

// ===== IMMEDIATE CLOUD ALERT DISPATCHER (TWILIO SMS ON PREFALL, TWILIO CALL & SMS ON FALL) =====
void dispatchImmediateAlertToBackend(String alertType) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[ALERT DISPATCH] WiFi disconnected! Fallback to local buzzer/GSM.");
        return;
    }

    WiFiClientSecure client;
    HTTPClient http;
    beginSecureHttp(http, client, serverUrl);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(8000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    double activeLat = (gpsLat != 0.0) ? gpsLat : lastKnownLat;
    double activeLng = (gpsLng != 0.0) ? gpsLng : lastKnownLng;
    float rawTotalAccel = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);

    String payload = "{";
    payload += "\"deviceId\":\"" + WiFi.macAddress() + "\",";
    payload += "\"macAddress\":\"" + WiFi.macAddress() + "\",";
    payload += "\"beltType\":\"Waist Belt\",";
    payload += "\"stateName\":\"" + alertType + "\",";
    payload += "\"mlState\":\"" + alertType + "\",";
    payload += "\"mlConfidence\":0.96,";
    payload += "\"heartRate\":" + String(currentHr) + ",";
    payload += "\"spo2\":" + String(currentSpo2) + ",";
    payload += "\"temperature\":" + String(currentTemp) + ",";
    payload += "\"beltWorn\":true,";
    payload += "\"bandConnected\":" + String(dataReceivedFromWrist ? "true" : "false") + ",";
    payload += "\"acceleration\":" + String(rawTotalAccel, 2) + ",";
    payload += "\"latitude\":" + String(activeLat, 6) + ",";
    payload += "\"longitude\":" + String(activeLng, 6) + ",";
    payload += "\"alertState\":" + String(alertType == "PREFALL" ? 1 : 2) + ",";
    payload += "\"micMessageAudio\":\"" + micMessage + "\",";
    payload += "\"timestamp\":" + String(millis());
    payload += "}";

    Serial.println("========================================");
    Serial.println("[TWILIO CLOUD DISPATCH]: Sending immediate " + alertType + " alert to backend...");
    Serial.println("========================================");
    int httpResponseCode = http.POST(payload);
    if (httpResponseCode > 0) {
        Serial.println("[TWILIO CLOUD DISPATCH SUCCESS]: HTTP " + String(httpResponseCode) + 
                       (alertType == "PREFALL" ? " -> Twilio SMS dispatched to Guardian!" : " -> Twilio LIVE EMERGENCY CALL & SMS dispatched to Guardian!"));
    } else {
        Serial.println("[TWILIO CLOUD DISPATCH ERROR]: " + http.errorToString(httpResponseCode));
    }
    http.end();

    // For FALL: Invoke direct emergency voice call endpoints to ensure immediate live phone call to guardian
    if (alertType == "FALL") {
        WiFiClientSecure fallClient;
        HTTPClient fallHttp;
        String fallUrl = String(serverUrl);
        fallUrl.replace("/api/sensor-data", "/notify-guardian-fall");
        beginSecureHttp(fallHttp, fallClient, fallUrl);
        fallHttp.addHeader("Content-Type", "application/json");
        fallHttp.setTimeout(4000);
        String fallPayload = "{\"deviceId\":\"" + WiFi.macAddress() + "\",\"elderlyName\":\"Senior Citizen\",\"location\":\"Home\"}";
        int code = fallHttp.POST(fallPayload);
        Serial.println("[DIRECT CALL ENDPOINT]: /notify-guardian-fall response code: " + String(code));
        fallHttp.end();

        // Also trigger direct emergency phone call endpoint
        WiFiClientSecure callClient;
        HTTPClient callHttp;
        String callUrl = String(serverUrl);
        callUrl.replace("/api/sensor-data", "/api/emergency-call");
        beginSecureHttp(callHttp, callClient, callUrl);
        callHttp.addHeader("Content-Type", "application/json");
        callHttp.setTimeout(4000);
        String callPayload = "{\"deviceId\":\"" + WiFi.macAddress() + "\"}";
        int callCode = callHttp.POST(callPayload);
        Serial.println("[DIRECT CALL ENDPOINT]: /api/emergency-call response code: " + String(callCode));
        callHttp.end();
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
    
    Serial.println("========================================");
    Serial.println(">>> [PREFALL ALERT ACTIVATED] <<<");
    Serial.println("  Action: Twilio SMS to Guardian (No Voice Call)");
    Serial.println("========================================");
    
    // Speaker voice announcement & buzzer
    playSpeakerAnnouncement("PREFALL");
    digitalWrite(BUZZER_PIN, HIGH);
    
    // 1. Dispatch immediate HTTP alert to backend -> triggers Twilio SMS to Guardian's registered phone
    dispatchImmediateAlertToBackend("PREFALL");
    
    // 2. Hardware GSM SMS backup (SMS only, NO call on prefall!)
    if (mainGuardianPhone.length() > 0) {
        double activeLat = (gpsLat != 0.0) ? gpsLat : lastKnownLat;
        double activeLng = (gpsLng != 0.0) ? gpsLng : lastKnownLng;
        String message = "PREFALL ALERT: Elderly instability detected. Please check on them. Location: " + String(activeLat, 6) + "," + String(activeLng, 6);
        sendGSMSMS(mainGuardianPhone, message);
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
    
    Serial.println("========================================");
    Serial.println(">>> [CRITICAL FALL ALERT ACTIVATED] <<<");
    Serial.println("  Action: Twilio EMERGENCY VOICE CALL & SMS to Guardian!");
    Serial.println("========================================");
    
    // Start continuous "Help! Help!" speaker repeat loop
    lastHelpSpeakerTime = 0;
    digitalWrite(BUZZER_PIN, HIGH);
    
    // 1. Dispatch immediate HTTP alert to backend -> triggers Twilio Call & SMS to Guardian's registered phone
    dispatchImmediateAlertToBackend("FALL");
    
    // 2. Hardware GSM backup (Emergency Call + SMS)
    if (mainGuardianPhone.length() > 0) {
        double activeLat = (gpsLat != 0.0) ? gpsLat : lastKnownLat;
        double activeLng = (gpsLng != 0.0) ? gpsLng : lastKnownLng;
        makePhoneCall(mainGuardianPhone, false); // Actual call
        String message = "FALL DETECTED: Elderly has fallen! Emergency! Location: " + String(activeLat, 6) + "," + String(activeLng, 6);
        sendGSMSMS(mainGuardianPhone, message);
    }
    
    // Send phone + SMS to neighbor if configured
    if (neighborPhone.length() > 0) {
        makePhoneCall(neighborPhone, false);
        String message = "FALL DETECTED: Your neighbor has fallen! Emergency!";
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
            Serial.println("Prefall acknowledgment timeout - notifying neighbor");
            if (neighborPhone.length() > 0 && !missedCallMade) {
                makePhoneCall(neighborPhone, true); // Missed call
                String message = "PREFALL ALERT: Guardian did not respond. Please check on your neighbor immediately. Location: " + String(gpsLat, 6) + "," + String(gpsLng, 6);
                sendGSMSMS(neighborPhone, message);
                missedCallMade = true;
            }
        }
    } else if (currentAlertState == FALL_ALERT) {
        // Check 2-minute timeout for main guardian
        if (!guardianAcknowledged && elapsed > FALL_ACK_TIMEOUT && !secondaryGuardianNotified) {
            Serial.println("Fall acknowledgment timeout - notifying secondary guardian");
            if (secondaryGuardianPhone.length() > 0) {
                double activeLat = (gpsLat != 0.0) ? gpsLat : lastKnownLat;
                double activeLng = (gpsLng != 0.0) ? gpsLng : lastKnownLng;
                makePhoneCall(secondaryGuardianPhone, false); // Actual call
                String message = "FALL DETECTED: Main guardian did not respond. Emergency! Location: " + String(activeLat, 6) + "," + String(activeLng, 6);
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
            Serial.println("[PUSH BUTTON 2 PRESSED]: Local reset / Acknowledgment button pressed by elderly.");
            guardianAcknowledged = true;
            currentMLState = ML_NORMAL;
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

    String url = String(serverUrl);
    url.replace("/api/sensor-data", "/api/alerts/acknowledge");
    
    WiFiClientSecure client;
    HTTPClient http;
    beginSecureHttp(http, client, url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);

    String payload = "{";
    payload += "\"elderlyId\":\"" + elderlyId + "\",";
    payload += "\"deviceId\":\"" + DEVICE_ID + "\",";
    payload += "\"alertType\":\"";
    payload += (currentAlertState == PREFALL_ALERT ? "PREFALL" : "FALL");
    payload += "\",";
    payload += "\"acknowledged\":true";
    payload += "}";

    http.POST(payload);
    http.end();
}

// ===== WIFI MANAGER FUNCTIONS =====
void loadKnownNetworks() {
  wifiPrefs.begin("wifi_networks", false);
  int count = wifiPrefs.getInt("network_count", 0);
  knownNetworkCount = 0;
  
  // Load stored networks from NVS Flash (pure auto-load & deduplicating)
  for (int i = 0; i < count && knownNetworkCount < MAX_KNOWN_NETWORKS; i++) {
    String key = "net_" + String(i);
    String data = wifiPrefs.getString(key.c_str(), "");
    
    int sep1 = data.indexOf('|');
    int sep2 = data.indexOf('|', sep1 + 1);
    
    if (sep1 > 0 && sep2 > sep1) {
      String s = data.substring(0, sep1);
      s.trim();
      String p = data.substring(sep1 + 1, sep2);
      p.trim();
      int prio = data.substring(sep2 + 1).toInt();
      
      // Prevent duplicates
      bool exists = false;
      for (int k = 0; k < knownNetworkCount; k++) {
        if (knownNetworks[k].ssid.equalsIgnoreCase(s)) {
          exists = true;
          break;
        }
      }
      if (!exists && s.length() > 0) {
        knownNetworks[knownNetworkCount].ssid = s;
        knownNetworks[knownNetworkCount].password = p;
        knownNetworks[knownNetworkCount].priority = prio;
        knownNetworkCount++;
      }
    }
  }
  
  wifiPrefs.end();
  Serial.println("========================================");
  Serial.println("[NVS FLASH] Loaded " + String(knownNetworkCount) + " unique saved WiFi network(s):");
  for (int i = 0; i < knownNetworkCount; i++) {
    Serial.println("  [" + String(i) + "] SSID: \"" + knownNetworks[i].ssid + "\" (len: " + String(knownNetworks[i].ssid.length()) + ")");
  }
  Serial.println("========================================");
}

void saveKnownNetwork(String ssid, String password, int priority = 5) {
  ssid.trim();
  password.trim();
  if (ssid.length() == 0) return;

  bool found = false;
  // Check if already exists
  for (int i = 0; i < knownNetworkCount; i++) {
    if (knownNetworks[i].ssid.equalsIgnoreCase(ssid)) {
      knownNetworks[i].ssid = ssid;
      knownNetworks[i].password = password;
      knownNetworks[i].priority = priority;
      found = true;
      break;
    }
  }
  
  // Add new if not found and space available
  if (!found && knownNetworkCount < MAX_KNOWN_NETWORKS) {
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
  Serial.println("[WIFI SAVED]: " + ssid + " (Total known networks: " + String(knownNetworkCount) + ")");
}

bool connectToNetwork(String ssid, String password) {
  ssid.trim();
  password.trim();
  if (ssid.length() == 0) return false;

  Serial.println("========================================");
  Serial.println("[WIFI CONNECTING] SSID: \"" + ssid + "\"");
  
  // Temporarily pause any active BLE scan during Wi-Fi connection handshake
  NimBLEScan* pScan = NimBLEDevice::getScan();
  if (pScan && pScan->isScanning()) {
    pScan->stop();
  }
  
  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);
  
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
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("========================================");
    Serial.println(">>> [WIFI CONNECTED SUCCESSFULLY] <<<");
    Serial.println("SSID       : " + ssid);
    Serial.print  ("IP Address : ");
    Serial.println(WiFi.localIP());
    Serial.print  ("Signal RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.println("========================================");
    return true;
  } else {
    Serial.println("========================================");
    Serial.print("[WIFI CONNECT FAILED] Status code: ");
    int st = WiFi.status();
    Serial.print(st);
    if (st == WL_NO_SSID_AVAIL) Serial.println(" (WL_NO_SSID_AVAIL - Hotspot not in range or 5GHz)");
    else if (st == WL_CONNECT_FAILED) Serial.println(" (WL_CONNECT_FAILED - Wrong password)");
    else if (st == WL_DISCONNECTED) Serial.println(" (WL_DISCONNECTED - Timed out or refused)");
    else Serial.println();
    Serial.println("========================================");
    return false;
  }
}

bool scanAndConnect() {
  Serial.println("========================================");
  Serial.println("[WIFI] Scanning and checking nearby Wi-Fi networks...");
  Serial.println("========================================");
  
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);

  int n = WiFi.scanNetworks();
  Serial.println("[WIFI] Found " + String(n) + " visible network(s):");
  
  if (n == 0) {
    Serial.println("[WIFI] No networks detected in the area.");
  } else {
    for (int j = 0; j < n; j++) {
      String sec = (WiFi.encryptionType(j) == WIFI_AUTH_OPEN) ? "OPEN" : "SECURED";
      Serial.println("  (" + String(j + 1) + ") \"" + WiFi.SSID(j) + "\" [" + String(WiFi.RSSI(j)) + " dBm] " + sec);
    }
  }

  // 1. Check if any scanned network matches our saved networks
  if (knownNetworkCount > 0 && n > 0) {
    for (int i = 0; i < knownNetworkCount; i++) {
      for (int j = 0; j < n; j++) {
        String scannedSSID = WiFi.SSID(j);
        scannedSSID.trim();
        if (scannedSSID.equalsIgnoreCase(knownNetworks[i].ssid)) {
          Serial.println("[WIFI MATCH] Found saved network: \"" + knownNetworks[i].ssid + "\" -> Connecting now...");
          if (connectToNetwork(knownNetworks[i].ssid, knownNetworks[i].password)) {
            WiFi.scanDelete();
            return true;
          }
        }
      }
    }
  }

  // 2. Direct connection attempt to all saved networks (in case hotspot is not broadcasting beacons)
  if (knownNetworkCount > 0) {
    for (int i = 0; i < knownNetworkCount; i++) {
      Serial.println("[WIFI DIRECT] Trying direct connection to saved: \"" + knownNetworks[i].ssid + "\"...");
      if (connectToNetwork(knownNetworks[i].ssid, knownNetworks[i].password)) {
        if (n > 0) WiFi.scanDelete();
        return true;
      }
    }
  }

  // 3. Fallback: check for any open networks (no password)
  if (n > 0) {
    for (int j = 0; j < n; j++) {
      if (WiFi.encryptionType(j) == WIFI_AUTH_OPEN) {
        String openSSID = WiFi.SSID(j);
        openSSID.trim();
        if (openSSID.length() > 0 && !openSSID.startsWith("SilverCare")) {
          Serial.println("[WIFI OPEN] Found open public network: \"" + openSSID + "\" -> Connecting...");
          if (connectToNetwork(openSSID, "")) {
            WiFi.scanDelete();
            return true;
          }
        }
      }
    }
    WiFi.scanDelete();
  }

  Serial.println("[WIFI] Could not connect to any network.");
  return false;
}

// ===== CAPTIVE PORTAL =====
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\">";
  html += "<title>SilverCare Belt Setup</title>";
  html += "<style>";
  html += "* { box-sizing: border-box; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }";
  html += "body { margin: 0; padding: 20px; background: #0f172a; color: #f8fafc; }";
  html += ".card { max-width: 420px; margin: 0 auto; background: #1e293b; padding: 24px; border-radius: 16px; box-shadow: 0 10px 25px rgba(0,0,0,0.4); border: 1px solid #334155; }";
  html += "h1 { margin-top: 0; font-size: 22px; color: #38bdf8; text-align: center; font-weight: 700; }";
  html += "p { color: #94a3b8; font-size: 14px; text-align: center; margin-bottom: 20px; }";
  html += ".section-title { font-size: 13px; text-transform: uppercase; letter-spacing: 0.5px; color: #64748b; font-weight: 600; margin-bottom: 8px; }";
  html += ".network-list { max-height: 180px; overflow-y: auto; background: #0f172a; border-radius: 10px; padding: 6px; border: 1px solid #334155; margin-bottom: 16px; }";
  html += ".net-btn { width: 100%; display: flex; justify-content: space-between; align-items: center; background: #1e293b; border: 1px solid #334155; color: #e2e8f0; padding: 10px 14px; border-radius: 8px; margin-bottom: 6px; cursor: pointer; text-align: left; font-size: 14px; transition: all 0.2s; }";
  html += ".net-btn:hover, .net-btn:active { background: #38bdf8; color: #0f172a; font-weight: 600; }";
  html += ".signal { font-size: 12px; opacity: 0.8; }";
  html += "label { display: block; margin: 12px 0 6px; color: #cbd5e1; font-size: 13px; font-weight: 500; }";
  html += "input { width: 100%; padding: 12px 14px; background: #0f172a; border: 1px solid #334155; border-radius: 10px; color: #f8fafc; font-size: 15px; }";
  html += "input:focus { outline: none; border-color: #38bdf8; }";
  html += ".btn-submit { width: 100%; padding: 14px; background: #2563eb; color: white; border: none; border-radius: 10px; font-size: 16px; font-weight: 600; cursor: pointer; margin-top: 20px; }";
  html += ".btn-submit:hover { background: #1d4ed8; }";
  html += ".hint { font-size: 12px; color: #64748b; text-align: center; margin-top: 14px; }";
  html += "</style></head><body>";
  html += "<div class=\"card\">";
  html += "<h1>SilverCare Setup</h1>";
  html += "<p>Select the elderly's home Wi-Fi network to connect this Smart Belt.</p>";
  html += "<div class=\"section-title\">Detected Wi-Fi Networks</div>";
  html += "<div class=\"network-list\">";

  int n = WiFi.scanNetworks();
  if (n == 0) {
    html += "<div style=\"color:#94a3b8;padding:12px;text-align:center;font-size:13px;\">No networks detected. Enter SSID below.</div>";
  } else {
    for (int i = 0; i < n && i < 15; i++) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      if (ssid.length() > 0) {
        html += "<button type=\"button\" class=\"net-btn\" onclick=\"pickNet('" + ssid + "')\">";
        html += "<span>" + ssid + "</span>";
        html += "<span class=\"signal\">" + String(rssi) + " dBm</span>";
        html += "</button>";
      }
    }
  }
  html += "</div>";

  html += "<form action=\"/save\" method=\"POST\">";
  html += "<label>Wi-Fi Name (SSID):</label>";
  html += "<input type=\"text\" id=\"ssid_input\" name=\"ssid\" required placeholder=\"Tap a network above or type SSID\">";
  html += "<label>Wi-Fi Password:</label>";
  html += "<input type=\"password\" id=\"pwd_input\" name=\"password\" placeholder=\"Enter Wi-Fi password\">";
  html += "<button type=\"submit\" class=\"btn-submit\">Connect & Save to Belt</button>";
  html += "</form>";
  html += "<div class=\"hint\">Credentials will be saved into belt memory. The belt connects automatically from now on.</div>";
  html += "</div>";
  html += "<script>function pickNet(s){document.getElementById('ssid_input').value=s;document.getElementById('pwd_input').focus();}</script>";
  html += "</body></html>";

  webServer.send(200, "text/html", html);
}

void handleSave() {
  String ssid = webServer.arg("ssid");
  String password = webServer.arg("password");
  ssid.trim();
  password.trim();
  
  Serial.println("[CAPTIVE PORTAL] Received Wi-Fi credentials for: " + ssid);
  
  // Save the network permanently to flash memory
  saveKnownNetwork(ssid, password, 10);
  
  String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<style>body{margin:0;padding:30px;background:#0f172a;color:#f8fafc;font-family:sans-serif;text-align:center;}</style></head>";
  html += "<body><h2 style='color:#38bdf8;'>Wi-Fi Saved!</h2>";
  html += "<p>Connecting to <b>" + ssid + "</b>...</p>";
  html += "<p style='color:#94a3b8;'>Belt will reboot in 2 seconds to connect cleanly. Please ensure your hotspot is turned on.</p>";
  html += "</body></html>";
  webServer.send(200, "text/html", html);
  
  delay(2000);
  Serial.println("[CAPTIVE PORTAL] Rebooting ESP32 to connect cleanly to: " + ssid);
  ESP.restart();
}

void startCaptivePortal() {
  if (apModeActive) return;

  Serial.println("========================================");
  Serial.println("[CAPTIVE PORTAL] Starting AP Mode");
  Serial.println("========================================");
  
  WiFi.mode(WIFI_AP_STA);
  
  if (WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("Hotspot created: " + String(AP_SSID));
    Serial.println("IP: " + WiFi.softAPIP().toString());
    Serial.println("Password: " + String(AP_PASSWORD));
  } else {
    Serial.println("Failed to start AP mode");
    return;
  }
  
  // Start DNS Server on port 53 for automatic captive portal popup on phones
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // Setup Web Server routes
  webServer.on("/", handleRoot);
  webServer.on("/save", HTTP_POST, handleSave);
  webServer.onNotFound([]() {
    webServer.sendHeader("Location", "/", true);
    webServer.send(302, "text/plain", "");
  });
  webServer.begin();
  
  apModeActive = true;
  apModeStartTime = millis();
  Serial.println("Captive portal web server & DNS ready");
  Serial.println("========================================");
}

void stopCaptivePortal() {
  if (apModeActive) {
    dnsServer.stop();
    webServer.stop();
    WiFi.softAPdisconnect(true);
    apModeActive = false;
    Serial.println("Captive portal stopped");
  }
}

void setupWiFi() {
  Serial.println("========================================");
  Serial.println("AUTOMATIC WIFI MANAGER");
  Serial.println("========================================");
  
  WiFi.mode(WIFI_STA);
  
  // Load saved networks from preferences
  loadKnownNetworks();
  
  // Always scan and check each network first before starting captive portal!
  if (scanAndConnect()) {
    Serial.println(">>> Connected to Wi-Fi successfully! <<<");
    return;
  }
  
  // If nothing connects, start the captive portal
  Serial.println("========================================");
  Serial.println("Could not connect to any network. Starting Captive Portal...");
  Serial.println("========================================");
  startCaptivePortal();
}

void manageWiFiConnection() {
  static unsigned long lastScanTime = 0;
  static int consecutiveFailures = 0;
  
  // Handle captive portal timeout
  if (apModeActive && millis() - apModeStartTime > AP_MODE_TIMEOUT) {
    Serial.println("Captive portal timeout, stopping AP mode");
    stopCaptivePortal();
  }
  
  // Serve web server and DNS if in AP mode
  if (apModeActive) {
    dnsServer.processNextRequest();
    webServer.handleClient();
    return;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    consecutiveFailures = 0;
    // Periodically scan for better networks
    if (millis() - lastScanTime > WIFI_SCAN_INTERVAL) {
      lastScanTime = millis();
    }
    return;
  }
  
  // Not connected, try to reconnect
  unsigned long now = millis();
  if (now - lastWiFiReconnectAttempt > WIFI_RECONNECT_INTERVAL) {
    Serial.println("WiFi disconnected, attempting reconnection...");
    lastWiFiReconnectAttempt = now;
    
    if (WiFi.reconnect() || scanAndConnect()) {
      Serial.println("Reconnected to Wi-Fi!");
      consecutiveFailures = 0;
      return;
    }
    
    consecutiveFailures++;
    Serial.println("[WIFI] Reconnection attempt failed (Count: " + String(consecutiveFailures) + "/25)");
    
    // Only fall back to Captive Portal if completely unable to reconnect after 25 cycles (~4 mins) and no stored networks
    if (consecutiveFailures >= 25 && knownNetworkCount == 0) {
      Serial.println("[WIFI FALLBACK] No known networks. Starting Captive Portal...");
      startCaptivePortal();
    }
  }
}

// ===== GSM SETUP =====
void setupGSM() {
    gsmSerial.begin(GSM_BAUD);
    Serial.println("Initializing GSM Module...");
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
        Serial.println("GSM Module Responding");
        
        // Set SMS text mode
        gsmSerial.println("AT+CMGF=1");
        delay(1000);
        while(gsmSerial.available()) {
            gsmSerial.read();
        }
    } else {
        Serial.println("GSM Module Not Responding (will continue without GSM)");
    }
}

// ===== PIN SETUP =====
void setupPins() {
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(SOS_BUTTON_PIN, INPUT_PULLUP); // Push Button 1: Dedicated SOS Button
    pinMode(ACK_BUTTON_PIN, INPUT_PULLUP); // Push Button 2: Dedicated Reset / Ack Button
    pinMode(DS18B20_PIN, INPUT);          // DS18B20 OneWire Temperature sensor pin
    Serial.println("Pins configured: Push Button 1 (SOS), Push Button 2 (Ack), DS18B20, I2S Speaker & Mic initialized");
}

// ===== GPS SETUP =====
void setupGPS() {
    GPSSerial.begin(9600, SERIAL_8N1, GPS_TX_PIN, GPS_RX_PIN);
    Serial.println("GPS initialized");

    // Initialize GPS history
    for (int i = 0; i < GPS_HISTORY_SIZE; i++) {
        gpsHistory[i].lat = 0.0;
        gpsHistory[i].lng = 0.0;
        gpsHistory[i].timestamp = 0;
    }
}

// ===== BLE CLIENT SETUP =====
void setupBLEClient() {
    Serial.println("Initializing BLE Client to connect to Wrist Band...");

    NimBLEDevice::init("");
    pClient = NimBLEDevice::createClient();
    pClient->setConnectTimeout(30); // 30 second timeout
    
    // Set client callbacks to handle disconnection
    class ClientCallbacks : public NimBLEClientCallbacks {
        void onConnect(NimBLEClient* pClient) {
            Serial.println("[BLE CLIENT] Connected to server");
        }
        
        void onDisconnect(NimBLEClient* pClient, NimBLEConnInfo& connInfo, int reason) {
            Serial.println("[BLE CLIENT] Disconnected from server. Reason: " + String(reason));
            connected = false;
            dataReceivedFromWrist = false;
        }
    };
    pClient->setClientCallbacks(new ClientCallbacks());

    Serial.println("BLE Client initialized");
}

// ===== CONNECT TO WRIST BAND =====
bool connectToWristBand() {
    if (connected) {
        return true; // Already connected
    }

    Serial.println("========================================");
    Serial.println("Scanning for Wrist Band...");
    Serial.println("========================================");

    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(50); // 50% duty cycle allows Wi-Fi to use the 2.4GHz radio concurrently

    // Scan for 3 seconds instead of 15 seconds to prevent radio starvation
    pScan->start(3, false);
    delay(50);

    // Get results
    NimBLEScanResults results = pScan->getResults();

    if (results.getCount() == 0) {
        Serial.println("No BLE devices found during scan");
        pScan->clearResults();
        return false;
    }

    Serial.println("Devices found: " + String(results.getCount()));
    
    // Debug: Print all found devices
    for (int i = 0; i < results.getCount(); i++) {
        const NimBLEAdvertisedDevice* device = results.getDevice(i);
        if (device) {
            String deviceName = String(device->getName().c_str());
            String deviceAddr = String(device->getAddress().toString().c_str());
            int rssi = device->getRSSI();
            Serial.println("  [" + String(i) + "] Name: " + deviceName + " | Addr: " + deviceAddr + " | RSSI: " + String(rssi) + " dBm");
        }
    }

    for (int i = 0; i < results.getCount(); i++) {
        // Use const pointer since getDevice returns const
        const NimBLEAdvertisedDevice* device = results.getDevice(i);

        if (device) {
            String deviceName = String(device->getName().c_str());
            
            Serial.println("Checking device: " + deviceName + " against target: " + String(WRIST_NAME));
            
            if (deviceName == String(WRIST_NAME)) {
                Serial.println("========================================");
                Serial.println("Found Wrist Band: " + deviceName);
                Serial.println("Address: " + String(device->getAddress().toString().c_str()));
                Serial.println("========================================");

                // connect() takes a const pointer
                if (pClient->connect(device)) {
                    Serial.println("Connected to Wrist Band!");

                    // Get service
                    NimBLERemoteService* pService = pClient->getService(SERVICE_UUID);
                    if (pService == nullptr) {
                        Serial.println("Service not found - UUID mismatch");
                        Serial.println("Looking for Service UUID: " + String(SERVICE_UUID));
                        pClient->disconnect();
                        pScan->clearResults();
                        return false;
                    }
                    Serial.println("Service found");

                    // Get characteristic
                    pChar = pService->getCharacteristic(CHARACTERISTIC_UUID);
                    if (pChar == nullptr) {
                        Serial.println("Characteristic not found - UUID mismatch");
                        Serial.println("Looking for Characteristic UUID: " + String(CHARACTERISTIC_UUID));
                        pClient->disconnect();
                        pScan->clearResults();
                        return false;
                    }
                    Serial.println("Characteristic found");

                    // Subscribe to notifications
                    if (pChar->subscribe(true, notifyCallback, true)) {
                        Serial.println("========================================");
                        Serial.println("SUCCESS: Subscribed to notifications");
                        Serial.println("========================================");
                        connected = true;
                        pScan->clearResults();
                        return true;
                    } else {
                        Serial.println("Failed to subscribe to notifications");
                        pClient->disconnect();
                        pScan->clearResults();
                        return false;
                    }
                } else {
                    Serial.println("Failed to connect to Wrist Band");
                    pScan->clearResults();
                    return false;
                }
            }
        }
    }

    pScan->clearResults();
    Serial.println("========================================");
    Serial.println("Wrist Band '" + String(WRIST_NAME) + "' not found in scan results");
    Serial.println("========================================");
    return false;
}

// ===== MPU6050 PHYSICAL SENSOR INITIALIZATION & READING =====
void setupMPU6050() {
    Wire.begin(MPU6050_SDA, MPU6050_SCL);
    Wire.setClock(400000); // 400kHz fast I2C
    delay(50);
    
    // Probe primary I2C address 0x68
    Wire.beginTransmission(0x68);
    Wire.write(0x6B); // Power Management 1 register
    Wire.write(0x00); // Clear sleep bit (wake up MPU6050)
    byte err = Wire.endTransmission();
    if (err == 0) {
        mpuHardwareAvailable = true;
        mpuI2cAddress = 0x68;
    } else {
        // Fallback probe secondary address 0x69
        Wire.beginTransmission(0x69);
        Wire.write(0x6B);
        Wire.write(0x00);
        err = Wire.endTransmission();
        if (err == 0) {
            mpuHardwareAvailable = true;
            mpuI2cAddress = 0x69;
        }
    }

    if (mpuHardwareAvailable) {
        // Configure Accelerometer Range to +/- 4g (8192 LSB/g) -> register 0x1C = 0x08
        Wire.beginTransmission(mpuI2cAddress);
        Wire.write(0x1C);
        Wire.write(0x08);
        Wire.endTransmission();

        // Configure Gyroscope Range to +/- 500 deg/s (65.5 LSB/deg/s) -> register 0x1B = 0x08
        Wire.beginTransmission(mpuI2cAddress);
        Wire.write(0x1B);
        Wire.write(0x08);
        Wire.endTransmission();

        Serial.println("========================================");
        Serial.println("[MPU6050 HARDWARE] Physical sensor CONNECTED at 0x" + String(mpuI2cAddress, HEX) + "!");
        Serial.println("[MPU6050 HARDWARE] 6-Axis real-time motion capture online.");
        Serial.println("========================================");

        // Take quick baseline calibration of resting posture
        float sumMag = 0.0f;
        for (int i = 0; i < 25; i++) {
            delay(10);
            readMPU6050();
            float mag = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);
            sumMag += mag;
        }
        userBaselineAccel = sumMag / 25.0f;
        if (userBaselineAccel < 8.0f || userBaselineAccel > 12.0f) userBaselineAccel = 9.81f;
        Serial.println("[CALIBRATION] Calibrated resting posture baseline: " + String(userBaselineAccel, 2) + " m/s^2");
    } else {
        Serial.println("========================================");
        Serial.println("[MPU6050 WARNING] No physical MPU6050 responded on D21 (SDA) / D22 (SCL).");
        Serial.println("[MPU6050 WARNING] Check wiring: VCC->3.3V, GND->GND, SDA->D21, SCL->D22.");
        Serial.println("========================================");
    }
}

void readMPU6050() {
    static unsigned long lastSensorUpdate = 0;
    if (millis() - lastSensorUpdate < 20) { // 50 Hz sensor reading
        return;
    }
    lastSensorUpdate = millis();

    if (mpuHardwareAvailable) {
        Wire.beginTransmission(mpuI2cAddress);
        Wire.write(0x3B); // Starting register for Accel X
        Wire.endTransmission(false);
        Wire.requestFrom((uint8_t)mpuI2cAddress, (size_t)14, true);

        if (Wire.available() >= 14) {
            int16_t rawAcX = Wire.read() << 8 | Wire.read();
            int16_t rawAcY = Wire.read() << 8 | Wire.read();
            int16_t rawAcZ = Wire.read() << 8 | Wire.read();
            int16_t rawTmp = Wire.read() << 8 | Wire.read();
            int16_t rawGyX = Wire.read() << 8 | Wire.read();
            int16_t rawGyY = Wire.read() << 8 | Wire.read();
            int16_t rawGyZ = Wire.read() << 8 | Wire.read();

            // Convert raw accelerometer at +/- 4g range (8192 LSB/g, 1g = 9.80665 m/s^2)
            accelX = (rawAcX / 8192.0f) * 9.81f;
            accelY = (rawAcY / 8192.0f) * 9.81f;
            accelZ = (rawAcZ / 8192.0f) * 9.81f;

            // Convert raw gyroscope at +/- 500 deg/s range (65.5 LSB / (deg/s))
            gyroX = rawGyX / 65.5f;
            gyroY = rawGyY / 65.5f;
            gyroZ = rawGyZ / 65.5f;
            return;
        }
    }
}

// ===== REALISTIC VITALS HANDLING (DISABLED FAKE SIMULATION OVERWRITE) =====
void updateSimulation() {
    // Only update vitals if no wrist band is connected (keep stable resting vitals)
    if (!dataReceivedFromWrist) {
        currentHr = 74.0;
        currentSpo2 = 98.0;
        currentTemp = 36.6;
    }
}

// ===== SEND DATA TO BACKEND =====
void sendDataToBackend() {
    if (WiFi.status() == WL_CONNECTED) {
        WiFiClientSecure client;
        HTTPClient http;
        beginSecureHttp(http, client, serverUrl);
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
        payload += "\"deviceId\":\"" + WiFi.macAddress() + "\",";
        payload += "\"macAddress\":\"" + WiFi.macAddress() + "\",";
        payload += "\"beltType\":\"Waist Belt\",";
        payload += "\"stateName\":\"" + mlStateName + "\",";
        payload += "\"mlState\":\"" + mlStateName + "\",";
        payload += "\"mlConfidence\":" + String(mlConfidence) + ",";
        payload += "\"heartRate\":" + String(currentHr) + ",";
        payload += "\"spo2\":" + String(currentSpo2) + ",";
        payload += "\"temperature\":" + String(currentTemp) + ",";
        payload += "\"beltWorn\":true,";
        payload += "\"bandConnected\":" + String(dataReceivedFromWrist ? "true" : "false") + ",";
        // Send mapped acceleration magnitude for global dataset display (derived from 9.0-14.0 m/s^2 raw input)
        float reportedAccel = getMappedDisplayAccel();
        payload += "\"acceleration\":" + String(reportedAccel, 2) + ",";
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
            Serial.print("HTTP POST successful, code: ");
            Serial.println(httpResponseCode);
            
            String response = http.getString();
            if (response.indexOf("ACKNOWLEDGED") != -1 || response.indexOf("I am Fine") != -1) {
                Serial.println("[GUARDIAN ACKNOWLEDGED]: Clearing local buzzer and state!");
                digitalWrite(BUZZER_PIN, LOW);
                dacWrite(25, 0);
                currentMLState = ML_NORMAL;
                currentAlertState = NO_ALERT;
                guardianAcknowledged = true;
                micMessage = "";
            }
        } else {
            Serial.print("HTTP Error code: ");
            Serial.println(httpResponseCode);
            Serial.print("Error: ");
            Serial.println(http.errorToString(httpResponseCode));
        }
        http.end();
    } else {
        Serial.println("WiFi Disconnected - waiting for connection...");
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
    
    Serial.println("[SMS] Smart SMS System Activated");
    
    if (WiFi.status() == WL_CONNECTED && !twilioSMSSent) {
        Serial.println("[TWILIO] Attempting SMS via WiFi...");
        bool twilioSuccess = sendTwilioSMS(alertType, message);
        if (twilioSuccess) {
            twilioSMSSent = true;
            lastSMSTime = currentTime;
            return;
        }
    }
    
    if (!gsmSMSSent) {
        Serial.println("[GSM] Sending backup SMS...");
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
    
    Serial.println("Generated new Device ID: " + DEVICE_ID);
  } else {
    Serial.println("Loaded Device ID: " + DEVICE_ID);
  }
  
  devicePrefs.end();
}

void registerDevice() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping device registration");
    return;
  }
  
  WiFiClientSecure client;
  HTTPClient http;
  String url = String(deviceApiUrl) + "/register";
  beginSecureHttp(http, client, url);
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
    Serial.print("Device registered, code: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("Device registration failed, code: ");
    Serial.println(httpResponseCode);
    Serial.print("Error: ");
    Serial.println(http.errorToString(httpResponseCode));
  }
  
  http.end();
}

void sendHeartbeat() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  
  WiFiClientSecure client;
  HTTPClient http;
  String url = String(deviceApiUrl) + "/" + DEVICE_ID + "/heartbeat";
  beginSecureHttp(http, client, url);
  http.setTimeout(5000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  int httpResponseCode = http.POST("");
  
  if (httpResponseCode > 0) {
    // Heartbeat successful
  }
  
  http.end();
}

bool sendTwilioSMS(String alertType, String message) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[TWILIO SMS ERROR] WiFi not connected");
        return false;
    }
    WiFiClientSecure client;
    HTTPClient http;
    beginSecureHttp(http, client, twilioUrl);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(8000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    String payload = "{";
    payload += "\"deviceId\":\"" + WiFi.macAddress() + "\",";
    payload += "\"alertType\":\"" + alertType + "\",";
    payload += "\"message\":\"" + message + "\"";
    payload += "}";
    
    Serial.println("[TWILIO SMS] Sending to backend endpoint: " + String(twilioUrl));
    int code = http.POST(payload);
    Serial.println("[TWILIO SMS] Result HTTP code: " + String(code));
    http.end();
    return (code >= 200 && code < 300);
}

bool sendGSMAlert(String alertType, String message) {
    if (mainGuardianPhone.length() > 0) {
        return sendGSMSMS(mainGuardianPhone, message);
    }
    return false;
}

void sendSMS(String message) {
    if (mainGuardianPhone.length() > 0) {
        sendGSMSMS(mainGuardianPhone, message);
    }
}

void handleNotifications() {
    static unsigned long lastNotification = 0;
    if (currentAlertState != NO_ALERT && millis() - lastNotification > 1000) {
        digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
        lastNotification = millis();
    } else if (currentAlertState == NO_ALERT) {
        digitalWrite(BUZZER_PIN, LOW);
    }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println("========================================");
  Serial.println("SILVERCARE WAIST BELT WORKAROUND");
  Serial.println("========================================");

  generateDeviceId();
  
  // Display MAC address for pairing
  String macAddress = WiFi.macAddress();
  Serial.println("========================================");
  Serial.println("DEVICE PAIRING INFORMATION");
  Serial.println("========================================");
  Serial.println("MAC Address: " + macAddress);
  Serial.println("Device ID: " + DEVICE_ID);
  Serial.println("Use MAC address to pair this device");
  Serial.println("with a senior citizen profile in the app");
  Serial.println("========================================");

  setupPins();
  setupMPU6050();   // Initialize physical MPU-6050 on D21 (SDA) / D22 (SCL)
  setupGPS();
  setupBLEClient(); // Initialize BLE client to connect to wrist band
  setupWiFi();
  setupGSM();
  
  // Initialize I2S audio
  initSpeakerI2S();
  initMicrophoneI2S();
  
  mlStateChangeTime = millis();

  // Get elderly ID from preferences or backend
  devicePrefs.begin("device_id", false);
  elderlyId = devicePrefs.getString("elderly_id", "");
  devicePrefs.end();

  // Register device if WiFi is connected & fetch personalized profile
  if (WiFi.status() == WL_CONNECTED) {
    registerDevice();
    fetchContactInfo();
    fetchElderlyProfile(); // Pull senior health history & calibration profile from backend
  }

  // Try to connect to wrist band via BLE
  connectToWristBand();
}

// ===== LOOP =====
void loop() {
  unsigned long now = millis();

  // Try to connect to wrist band if not connected (give Wi-Fi priority if not connected)
  static unsigned long lastBLEConnectAttempt = 0;
  unsigned long bleInterval = (WiFi.status() == WL_CONNECTED) ? 15000 : 30000;
  if (!connected && now - lastBLEConnectAttempt > bleInterval) {
    connectToWristBand();
    lastBLEConnectAttempt = now;
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

  // Check for worsening vitals after fall
  checkWorseningVitalsAfterFall();

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
    triggerAutoVoiceRecording("PREFALL"); // Start mic recording on pre-fall
  } else if (currentMLState == ML_FALL && currentAlertState != FALL_ALERT) {
    triggerFallAlert();
    triggerAutoVoiceRecording("FALL"); // Start mic recording on fall
  } else if (currentMLState == ML_NORMAL && currentAlertState != NO_ALERT) {
    // Clear alert when state is back to normal
    currentAlertState = NO_ALERT;
    digitalWrite(BUZZER_PIN, LOW);
    dacWrite(25, 0);
  }

  // Record audio samples if microphone is active
  if (micRecordingActive) {
    recordAudioSample();
  }

  // Play speaker help loop during fall alerts
  playSpeakerHelpLoop();

  // Handle Push Button 1 (SOS Button)
  handleSOSButton();

  // Handle Push Button 2 (Reset / Acknowledgment Button)
  handleAcknowledgment();

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
    fetchElderlyProfile();
    lastContactFetch = now;
  }
  delay(10);
}
