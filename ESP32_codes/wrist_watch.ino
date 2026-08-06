#include <Wire.h>
#include <MAX30105.h>
#include <heartRate.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include <mbedtls/aes.h>
#include <mbedtls/gcm.h>

// =========================================================================
//  SILVERCARE - SENIOR SAFETY WRIST BAND (BLE SERVER)
//  ONLY HEART RATE DATA COLLECTION
// =========================================================================

// =========================================================================
//  ENCRYPTION CONFIGURATION
// =========================================================================
// AES-256 key (32 bytes) - Same key as waist belt for compatibility
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

// =========================================================================
//  BLE CONFIGURATION
// =========================================================================
#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BLE_IDENTITY_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define DEVICE_NAME             "SilverCare_Wrist"
#define PAIRING_PASSWORD        "SC2024_001"

// =========================================================================
//  PINS
// =========================================================================
#define MAX30102_INT_PIN 34

// =========================================================================
//  OBJECTS
// =========================================================================
MAX30105 maxSensor;
Preferences preferences;

// =========================================================================
//  BLE SERVER VARIABLES
// =========================================================================
bool deviceConnected = false;
NimBLEServer* pServer = NULL;
NimBLEService* pService = NULL;
NimBLECharacteristic* pCharacteristic = NULL;
NimBLECharacteristic* pIdentityCharacteristic = NULL;
NimBLEAdvertising* pAdvertising = NULL;

// =========================================================================
//  HEART RATE DATA
// =========================================================================
float heartRate = 0;
float spo2 = 0;
long irValue = 0;
long redValue = 0;
float bodyTemp = 0;  // Local temperature from MAX30102
bool sensorInitialized = false;

// =========================================================================
//  HEART RATE ALGORITHM VARIABLES
// =========================================================================
const int RATE_SIZE = 25;
float rates[RATE_SIZE];
int rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute = 0;
int beatAvg = 0;

// =========================================================================
//  UNIQUE DEVICE IDENTITY
// =========================================================================
String uniqueDeviceID = "";

// =========================================================================
//  ENCRYPTION MANAGER CLASS
// =========================================================================
class EncryptionManager {
public:
    static String encrypt(const String& plaintext) {
        if (plaintext.length() == 0) return "";
        
        size_t plaintextLen = plaintext.length();
        size_t ciphertextLen = plaintextLen + 16;
        
        uint8_t* ciphertext = new uint8_t[ciphertextLen];
        uint8_t* tag = new uint8_t[16];
        
        mbedtls_gcm_context ctx;
        mbedtls_gcm_init(&ctx);
        mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, ENCRYPTION_KEY, 256);
        
        size_t outputLen = 0;
        int ret = mbedtls_gcm_starts(&ctx, MBEDTLS_GCM_ENCRYPT, ENCRYPTION_IV, 12);
        if (ret == 0) {
            ret = mbedtls_gcm_update(&ctx, 
                                   (const uint8_t*)plaintext.c_str(), plaintextLen,
                                   ciphertext, plaintextLen, &outputLen);
        }
        if (ret == 0) {
            ret = mbedtls_gcm_finish(&ctx, ciphertext + outputLen, 0, &outputLen, tag, 16);
        }
        
        mbedtls_gcm_free(&ctx);
        
        if (ret != 0) {
            delete[] ciphertext;
            delete[] tag;
            return "";
        }
        
        String result = "";
        for (size_t i = 0; i < plaintextLen; i++) {
            result += String(ciphertext[i], HEX);
            if (i < plaintextLen - 1) result += ":";
        }
        result += "|";
        for (int i = 0; i < 16; i++) {
            result += String(tag[i], HEX);
            if (i < 15) result += ":";
        }
        
        delete[] ciphertext;
        delete[] tag;
        return result;
    }
};

// =========================================================================
//  BLE SERVER CALLBACKS
// =========================================================================
class MyServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
        deviceConnected = true;
        Serial.println("========================================");
        Serial.println("[BLE] Wrist band connected to Waist band");
        Serial.print("[BLE] Connected to: ");
        Serial.println(connInfo.getAddress().toString().c_str());
        Serial.println("========================================");
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
        deviceConnected = false;
        Serial.println("[BLE] Wrist band disconnected from Waist band");
        Serial.println("[BLE] Restarting advertising...");
        delay(100);
        if (pAdvertising) {
            pAdvertising->start();
        }
    }
};

class MyCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
        Serial.println("[BLE] Characteristic read request");
    }
    
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
        String value = pCharacteristic->getValue().c_str();
        Serial.print("[BLE] Write request: ");
        Serial.println(value);
        
        // Handle pairing commands if needed
        if (value.startsWith("PAIR:")) {
            String response = "PAIR_ACK:" + uniqueDeviceID;
            pCharacteristic->setValue(response.c_str());
            pCharacteristic->notify();
            Serial.println("Pairing acknowledged");
        }
    }
    
    void onNotify(NimBLECharacteristic* pCharacteristic) {
        Serial.println("[BLE] Notification sent");
    }
};

// =========================================================================
//  SETUP
// =========================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Wire.begin(21, 22);
    
    Serial.println("========================================");
    Serial.println("=== SILVERCARE SENIOR SAFETY WRIST BAND ===");
    Serial.println("=== (Heart Rate Data Only) ===");
    Serial.println("========================================");
    
    // ========== GENERATE UNIQUE DEVICE ID ==========
    generateUniqueDeviceID();
    
    // ========== MAX30102 INITIALIZATION ==========
    Serial.println("Initializing MAX30102...");
    if (!maxSensor.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("MAX30102 NOT FOUND");
        while (1);
    }
    
    byte ledBrightness = 0x7F;
    byte sampleAverage = 4;
    byte ledMode = 2;
    int sampleRate = 400;
    int pulseWidth = 411;
    int adcRange = 4096;
    
    maxSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
    maxSensor.setPulseAmplitudeRed(0x7F);
    maxSensor.setPulseAmplitudeIR(0x7F);
    
    Serial.println("MAX30102 Initialized");
    sensorInitialized = true;
    
    // ========== BLE SERVER INITIALIZATION ==========
    initBLEServer();
    
    Serial.println("========================================");
    Serial.println("BLE Server initialized - Advertising as: " + String(DEVICE_NAME));
    Serial.println("Unique Device ID: " + uniqueDeviceID);
    Serial.println("AES-256 Encryption: ENABLED");
    Serial.println("========================================");
}

// =========================================================================
//  GENERATE UNIQUE DEVICE ID
// =========================================================================
void generateUniqueDeviceID() {
    preferences.begin("wrist_id", false);
    
    // Check if ID already exists
    uniqueDeviceID = preferences.getString("unique_id", "");
    
    if (uniqueDeviceID.length() == 0) {
        // Generate new unique ID
        String mac = NimBLEDevice::getAddress().toString().c_str();
        uint32_t random = esp_random();
        uniqueDeviceID = "SCW_" + String(millis(), HEX) + "_" + String(random, HEX);
        
        preferences.putString("unique_id", uniqueDeviceID);
        preferences.end();
        preferences.begin("wrist_id", false);
        
        Serial.println("Generated new Unique Device ID: " + uniqueDeviceID);
    } else {
        Serial.println("Loaded Unique Device ID: " + uniqueDeviceID);
    }
    
    preferences.end();
}

// =========================================================================
//  BLE SERVER FUNCTIONS
// =========================================================================
void initBLEServer() {
    // Initialize NimBLE
    NimBLEDevice::init(DEVICE_NAME);
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    
    // Create server
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    
    // Create service
    pService = pServer->createService(BLE_SERVICE_UUID);
    
    // Create data characteristic (for heart rate data)
    pCharacteristic = pService->createCharacteristic(
        BLE_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ |
        NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::NOTIFY
    );
    pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
    
    // Identity Characteristic
    pIdentityCharacteristic = pService->createCharacteristic(
        BLE_IDENTITY_UUID,
        NIMBLE_PROPERTY::READ
    );
    
    // Set the unique device ID as the identity value
    pIdentityCharacteristic->setValue(uniqueDeviceID.c_str());
    Serial.println("Identity characteristic set with UID: " + uniqueDeviceID);
    
    // Start service
    pService->start();
    
    // Setup advertising
    pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    
    // Set advertisement data
    NimBLEAdvertisementData advData;
    advData.setName(DEVICE_NAME);
    advData.setCompleteServices(NimBLEUUID(BLE_SERVICE_UUID));
    pAdvertising->setAdvertisementData(advData);
    
    // Set scan response data
    NimBLEAdvertisementData scanData;
    scanData.setName(DEVICE_NAME);
    pAdvertising->setScanResponseData(scanData);
    
    // Start advertising
    pAdvertising->start();
    
    Serial.println("BLE Server started!");
    Serial.print("Service UUID: ");
    Serial.println(BLE_SERVICE_UUID);
    Serial.print("Data Characteristic: ");
    Serial.println(BLE_CHARACTERISTIC_UUID);
    Serial.print("Identity Characteristic: ");
    Serial.println(BLE_IDENTITY_UUID);
    Serial.print("Device Name: ");
    Serial.println(DEVICE_NAME);
}

// =========================================================================
//  SEND HEART DATA WITH ENCRYPTION
// =========================================================================
void sendHeartData() {
    if (!deviceConnected) return;
    if (pCharacteristic == NULL) return;
    
    // Format: "HR:SPO2:IR:TEMP"
    String plainData = String((int)heartRate) + ":" + 
                       String((int)spo2) + ":" + 
                       String(irValue) + ":" + 
                       String(bodyTemp, 1);
    
    // ENCRYPT DATA BEFORE SENDING
    String encryptedData = EncryptionManager::encrypt(plainData);
    
    if (encryptedData.length() > 0) {
        pCharacteristic->setValue(encryptedData.c_str());
        pCharacteristic->notify();
        
        Serial.print("[BLE] Sent (Encrypted): ");
        Serial.println(encryptedData);
        Serial.print("[BLE] Plain: ");
        Serial.println(plainData);
    } else {
        Serial.println("[BLE] Encryption failed!");
    }
}

// =========================================================================
//  HEART RATE CALCULATION FUNCTIONS
// =========================================================================
void calculateHeartRate() {
    if (!sensorInitialized) return;
    
    irValue = maxSensor.getIR();
    redValue = maxSensor.getRed();
    
    long delta = abs(irValue - redValue);
    
    if (irValue > 10000 && redValue > 10000) {
        if (delta > 5000) {
            long currentTime = millis();
            if (currentTime - lastBeat > 100) {
                long timeBetweenBeats = currentTime - lastBeat;
                if (timeBetweenBeats > 300 && timeBetweenBeats < 2000) {
                    beatsPerMinute = 60000.0 / timeBetweenBeats;
                    
                    rates[rateSpot++] = beatsPerMinute;
                    rateSpot %= RATE_SIZE;
                    
                    float sum = 0;
                    int count = 0;
                    for (int i = 0; i < RATE_SIZE; i++) {
                        if (rates[i] > 0) {
                            sum += rates[i];
                            count++;
                        }
                    }
                    if (count > 0) {
                        heartRate = sum / count;
                    }
                }
                lastBeat = currentTime;
            }
        }
    }
    
    if (irValue > 0 && redValue > 0) {
        float ratio = (float)redValue / (float)irValue;
        if (ratio > 0.5 && ratio < 1.5) {
            spo2 = 100 - (ratio - 0.5) * 20;
            if (spo2 > 100) spo2 = 100;
            if (spo2 < 70) spo2 = 70;
        }
    }
    
    // Body temperature estimation from MAX30102
    bodyTemp = 36.5 + (irValue / 50000.0); // Simple estimation
    if (bodyTemp > 42) bodyTemp = 42;
    if (bodyTemp < 32) bodyTemp = 32;
}

// =========================================================================
//  MAIN LOOP
// =========================================================================
void loop() {
    // ---------- HEART RATE CALCULATION ----------
    static unsigned long lastHeartRateCalc = 0;
    const unsigned long HEART_RATE_INTERVAL = 100;
    
    if (millis() - lastHeartRateCalc > HEART_RATE_INTERVAL) {
        calculateHeartRate();
        lastHeartRateCalc = millis();
    }

    // ---------- SEND HEART DATA VIA BLE ----------
    static unsigned long lastBLESend = 0;
    if (millis() - lastBLESend > 500) {
        if (deviceConnected && heartRate > 0) {
            sendHeartData();
        }
        lastBLESend = millis();
    }

    delay(10);
}