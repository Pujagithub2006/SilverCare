#include <Wire.h>
#include <NimBLEDevice.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <mbedtls/aes.h>
#include <mbedtls/gcm.h>

// =========================================================================
//  SILVERCARE - WRIST BAND HEART SENSOR (ESP32-C3 BLE SERVER)
//  ONLY sends heart data via BLE to waist belt
// =========================================================================

// =========================================================================
//  ENCRYPTION CONFIGURATION (Must match waist belt)
// =========================================================================
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
//  BLE CONFIGURATION (Single UUID for simplicity)
// =========================================================================
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define DEVICE_NAME         "SilverCare_Wrist"

// =========================================================================
//  PIN CONFIGURATION
// =========================================================================
#define MAX30102_INT_PIN 34  // Interrupt pin for MAX30102 (if needed)

// =========================================================================
//  HEART RATE DATA
// =========================================================================
float heartRate = 0;
float spo2 = 0;
long irValue = 0;
long redValue = 0;
bool sensorInitialized = false;

// Heart rate algorithm variables
const int RATE_SIZE = 25;
float rates[RATE_SIZE];
int rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute = 0;
int beatAvg = 0;

// =========================================================================
//  BLE SERVER VARIABLES
// =========================================================================
bool deviceConnected = false;
NimBLEServer* pServer = NULL;
NimBLEService* pService = NULL;
NimBLECharacteristic* pCharacteristic = NULL;
NimBLEAdvertising* pAdvertising = NULL;

// =========================================================================
//  TIMING
// =========================================================================
unsigned long lastHeartRateCalc = 0;
const unsigned long HEART_RATE_INTERVAL = 100;  // Calculate every 100ms
unsigned long lastBLESend = 0;
const unsigned long BLE_SEND_INTERVAL = 1000;    // Send via BLE every 1 second

// =========================================================================
//  SENSOR OBJECT
// =========================================================================
MAX30105 maxSensor;

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
    Serial.println("✅ [BLE] Wrist band connected to Waist belt");
    Serial.print("📱 [BLE] Connected to: ");
    Serial.println(connInfo.getAddress().toString().c_str());
    Serial.println("========================================");
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    deviceConnected = false;
    Serial.println("❌ [BLE] Wrist band disconnected from Waist belt");
    Serial.println("🔄 [BLE] Restarting advertising...");
    delay(100);
    if (pAdvertising) {
      pAdvertising->start();
    }
  }
};

// =========================================================================
//  SETUP
// =========================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(8, 9);  // ESP32-C3 default I2C pins

  Serial.println("========================================");
  Serial.println("=== SILVERCARE WRIST BAND HEART SENSOR ===");
  Serial.println("=== (BLE Server - Heart Data Only) ===");
  Serial.println("========================================");

  // ========== Initialize MAX30102 Heart Sensor ==========
  Serial.println("🔴 Initializing MAX30102...");
  if (!maxSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("❌ MAX30102 NOT FOUND");
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
  
  Serial.println("✅ MAX30102 Initialized");
  sensorInitialized = true;

  // ========== BLE Server Initialization ==========
  initBLEServer();
  
  Serial.println("========================================");
  Serial.println("✅ Setup Complete - Advertising as: " + String(DEVICE_NAME));
  Serial.println("🔐 AES-256 Encryption: ENABLED");
  Serial.println("========================================");
}

// =========================================================================
//  BLE SERVER INITIALIZATION
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
  pService = pServer->createService(SERVICE_UUID);
  
  // Create data characteristic (for heart rate data)
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::READ |
    NIMBLE_PROPERTY::WRITE |
    NIMBLE_PROPERTY::NOTIFY
  );
  
  // Start service
  pService->start();
  
  // Setup advertising
  pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  
  // Set advertisement data
  NimBLEAdvertisementData advData;
  advData.setName(DEVICE_NAME);
  advData.setCompleteServices(NimBLEUUID(SERVICE_UUID));
  pAdvertising->setAdvertisementData(advData);
  
  // Set scan response data
  NimBLEAdvertisementData scanData;
  scanData.setName(DEVICE_NAME);
  pAdvertising->setScanResponseData(scanData);
  
  // Start advertising
  pAdvertising->start();
  
  Serial.println("✅ BLE Server started!");
  Serial.print("📱 Service UUID: ");
  Serial.println(SERVICE_UUID);
  Serial.print("📱 Characteristic UUID: ");
  Serial.println(CHARACTERISTIC_UUID);
}

// =========================================================================
//  READ HEART RATE SENSOR
// =========================================================================
void readHeartRateSensor() {
  if (!sensorInitialized) return;
  
  irValue = maxSensor.getIR();
  redValue = maxSensor.getRed();
  
  if (checkForBeat(irValue) == true) {
    // We have a beat
    long delta = millis() - lastBeat;
    lastBeat = millis();
    
    beatsPerMinute = 60 / (delta / 1000.0);
    
    if (beatsPerMinute < 255 && beatsPerMinute > 20) {
      rates[rateSpot++] = (byte)beatsPerMinute;
      rateSpot %= RATE_SIZE;
      
      // Calculate average
      beatAvg = 0;
      for (int x = 0; x < RATE_SIZE; x++) {
        beatAvg += rates[x];
      }
      beatAvg /= RATE_SIZE;
      
      heartRate = beatAvg;
      
      // Simple SpO2 estimation (ratio of red to IR)
      if (redValue > 0 && irValue > 0) {
        float ratio = (float)redValue / (float)irValue;
        spo2 = 110.0 - (ratio * 25.0);  // Simplified estimation
        if (spo2 > 100) spo2 = 100;
        if (spo2 < 70) spo2 = 70;
      }
      
      Serial.print("💓 Heart Rate: ");
      Serial.print(heartRate);
      Serial.print(" BPM | SpO2: ");
      Serial.print(spo2);
      Serial.println("%");
    }
  }
}

// =========================================================================
//  SEND HEART DATA VIA BLE
// =========================================================================
void sendHeartDataViaBLE() {
  if (!deviceConnected || !pCharacteristic) return;
  
  // Format: heartRate:spo2:irValue (compatible with waist belt parsing)
  String dataPayload = String(heartRate) + ":" + String(spo2) + ":" + String(irValue);
  
  // Encrypt the data
  String encryptedData = EncryptionManager::encrypt(dataPayload);
  
  if (encryptedData.length() > 0) {
    pCharacteristic->setValue(encryptedData.c_str());
    pCharacteristic->notify();
    Serial.print("📤 [BLE] Sent encrypted heart data: ");
    Serial.println(dataPayload);
  }
}

// =========================================================================
//  LOOP
// =========================================================================
void loop() {
  unsigned long now = millis();
  
  // Read heart rate sensor every 100ms
  if (now - lastHeartRateCalc >= HEART_RATE_INTERVAL) {
    readHeartRateSensor();
    lastHeartRateCalc = now;
  }
  
  // Send heart data via BLE every 1 second
  if (now - lastBLESend >= BLE_SEND_INTERVAL) {
    sendHeartDataViaBLE();
    lastBLESend = now;
  }
  
  delay(10);
}
