#include <NimBLEDevice.h>
#include <Preferences.h>
#include <vector>
#include <algorithm>
#include <mbedtls/aes.h>
#include <mbedtls/gcm.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ===== WIFI CONFIGURATION =====
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ===== BACKEND CONFIGURATION =====
const char* serverUrl = "https://silvercare-production-3455.up.railway.app/api/sensor-data";
const char* DEVICE_ID = "waist_belt_demo_01"; // Update to match backend

// ===== EXISTING CONFIGURATION (PRESERVED) =====
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define IDENTITY_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"
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

// ===== SIMULATION STATE =====
enum SystemState { NORMAL, ALERT, CRITICAL };
SystemState currentState = NORMAL;
unsigned long stateChangeTime = 0;

float currentHr = 72.0;
float currentSpo2 = 98.0;
float currentTemp = 36.6;
long currentIr = 50000;

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

// ===== PROCESS RECEIVED DATA =====
void processReceivedData(String encryptedData) {
    String decryptedData = EncryptionManager::decrypt(encryptedData);
    if (decryptedData.length() == 0) return;
    
    int firstColon = decryptedData.indexOf(':');
    int secondColon = decryptedData.indexOf(':', firstColon + 1);
    int thirdColon = decryptedData.indexOf(':', secondColon + 1);
    
    if (firstColon > 0 && secondColon > 0 && thirdColon > 0) {
        currentHr = decryptedData.substring(0, firstColon).toFloat();
        currentSpo2 = decryptedData.substring(firstColon + 1, secondColon).toFloat();
        currentIr = decryptedData.substring(secondColon + 1, thirdColon).toInt();
        currentTemp = decryptedData.substring(thirdColon + 1).toFloat();
        dataReceivedFromWrist = true;
    }
}

// ===== NOTIFICATION CALLBACK =====
void notifyCallback(NimBLERemoteCharacteristic* pChar, uint8_t* data, size_t len, bool isNotify) {
    if (len == 0) return;
    String encryptedData = String((char*)data).substring(0, len);
    processReceivedData(encryptedData);
}

// ===== WIFI SETUP =====
void setupWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\n❌ WiFi Failed");
    }
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

        int stateVal = (currentState == NORMAL) ? 0 : (currentState == ALERT) ? 1 : 2;
        String stateName = (currentState == NORMAL) ? "NORMAL" : (currentState == ALERT) ? "ALERT" : "FALL_DETECTED";

        String payload = "{";
        payload += "\"deviceId\":\"" + String(DEVICE_ID) + "\",";
        payload += "\"beltType\":\"Waist Belt\",";
        payload += "\"state\":" + String(stateVal) + ",";
        payload += "\"stateName\":\"" + stateName + "\",";
        payload += "\"heartRate\":" + String(currentHr) + ",";
        payload += "\"spo2\":" + String(currentSpo2) + ",";
        payload += "\"temperature\":" + String(currentTemp) + ",";
        payload += "\"beltWorn\":true,";
        payload += "\"acceleration\":1.0,";
        payload += "\"latitude\":18.5204,";
        payload += "\"longitude\":73.8567,";
        payload += "\"timestamp\":" + String(millis());
        payload += "}";

        int httpResponseCode = http.POST(payload);
        if (httpResponseCode > 0) {
            Serial.print("✅ HTTP POST successful, code: ");
            Serial.println(httpResponseCode);
        } else {
            Serial.print("❌ HTTP Error code: ");
            Serial.println(httpResponseCode);
        }
        http.end();
    } else {
        Serial.println("❌ WiFi Disconnected, reconnecting...");
        WiFi.reconnect();
    }
}


void setup() {
    Serial.begin(115200);
    delay(3000);
    
    Serial.println("========================================");
    Serial.println("=== SILVERCARE WAIST BELT WORKAROUND ===");
    Serial.println("========================================");
    
    setupWiFi();
    stateChangeTime = millis();
}


void loop() {
    unsigned long now = millis();
    
    // In actual implementation, if dataReceivedFromWrist is false, run simulation
    if (!dataReceivedFromWrist) {
        updateSimulation();
    }
    
    if (now - lastDataSent > DATA_SEND_INTERVAL) {
        sendDataToBackend();
        lastDataSent = now;
    }
    
    // (Optional) Add non-blocking BLE connect / scan logic here to fallback to real data
    delay(10);
}
