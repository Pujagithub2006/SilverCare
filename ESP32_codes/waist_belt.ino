#include <NimBLEDevice.h>
#include <Preferences.h>
#include <vector>
#include <algorithm>
#include <mbedtls/aes.h>
#include <mbedtls/gcm.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>

// ===== EXISTING CONFIGURATION (PRESERVED) =====
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define IDENTITY_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define WRIST_NAME "SilverCare_Wrist"

// ===== ENCRYPTION CONFIGURATION =====
// AES-256 key (32 bytes) - In production, derive from device-specific key
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
const unsigned long CONNECTION_TIMEOUT = 60000;  // 1 MINUTE
const unsigned long DATA_RETRY_TIMEOUT = 60000;  // 1 MINUTE
const unsigned long SCAN_INTERVAL = 30;          // 30 seconds continuous scan
const unsigned long RECONNECT_DELAY = 30000;     // 30 seconds between reconnects

// ===== GSM CONFIGURATION =====
#define GSM_TX 16
#define GSM_RX 17
#define GSM_BAUD 9600
SoftwareSerial gsmSerial(GSM_RX, GSM_TX);

// ===== WiFi & BACKEND SERVER CONFIGURATION =====
String serverURL = "https://api.silvercare-app.com/api/sensor-data";
String twilioURL = "https://api.silvercare-app.com/api/twilio-sms";

// ===== PINS =====
#define TEMP_PIN 4
#define BUZZER_PIN 18
#define BUTTON_PIN 19
#define MIC_BUTTON_PIN 23
#define MAX30102_INT_PIN 34

// ===== HARDWARE IDENTIFICATION =====
String deviceId = "vois_waist";
String beltType = "Waist Belt";
double gpsLat = 18.5204;
double gpsLng = 73.8567;

// ===== OBJECTS =====
Adafruit_MPU6050 mpu;
OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);

// ===== THRESHOLDS =====
#define INSTABILITY_THRESHOLD 1.15
#define SUDDEN_THRESHOLD 1.4
#define FALL_THRESHOLD 1.9

#define TEMP_WORN_THRESHOLD 26.0

#define HR_LOW 50
#define HR_HIGH 135
#define SPO2_LOW 90

// ===== STATES =====
enum SystemState {
  NORMAL,
  PREFALL,
  SUDDEN_MOVEMENT,
  FALL_DETECTED
};

SystemState currentState = NORMAL;
String lastAlertType = "";

// ===== DEVICE STRUCTURE =====
struct PairedDevice {
    String address;
    String name;
    String uniqueID;
    String password;
    int priority;
    bool autoConnect;
    bool isActive;
    unsigned long lastSeen;
    unsigned long lastConnected;
    int connectionCount;
    float avgRSSI;
    
    String serialize() {
        String data = address + "|" + name + "|" + uniqueID + "|" + 
                     String(priority) + "|" + String(autoConnect) + "|" +
                     String(connectionCount) + "|" + password;
        return data;
    }
    
    static PairedDevice deserialize(String data) {
        PairedDevice device;
        int pos1 = data.indexOf('|');
        int pos2 = data.indexOf('|', pos1 + 1);
        int pos3 = data.indexOf('|', pos2 + 1);
        int pos4 = data.indexOf('|', pos3 + 1);
        int pos5 = data.indexOf('|', pos4 + 1);
        int pos6 = data.indexOf('|', pos5 + 1);
        
        device.address = data.substring(0, pos1);
        device.name = data.substring(pos1 + 1, pos2);
        device.uniqueID = data.substring(pos2 + 1, pos3);
        device.priority = data.substring(pos3 + 1, pos4).toInt();
        device.autoConnect = data.substring(pos4 + 1, pos5) == "1";
        device.connectionCount = data.substring(pos5 + 1, pos6).toInt();
        device.password = data.substring(pos6 + 1);
        device.isActive = false;
        device.lastSeen = 0;
        device.lastConnected = 0;
        device.avgRSSI = 0;
        
        return device;
    }
};

// ===== ENCRYPTION MANAGER =====
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
            ret = mbedtls_gcm_update(&ctx, ciphertext, ciphertextLen,
                                   plaintext, ciphertextLen, &outputLen);
        }
        if (ret == 0) {
            ret = mbedtls_gcm_finish(&ctx, plaintext + outputLen, 0, &outputLen, tag, 16);
        }
        
        mbedtls_gcm_free(&ctx);
        
        if (ret != 0) return "";
        
        return String((char*)plaintext, ciphertextLen);
    }
};

// ===== DEVICE DATABASE MANAGER =====
class DeviceDatabase {
private:
    Preferences preferences;
    std::vector<PairedDevice> devices;
    const int MAX_DEVICES = 20;
    
public:
    void init() {
        preferences.begin("device_db", false);
        loadDevices();
        Serial.println("Loaded " + String(devices.size()) + " paired devices");
    }
    
    void loadDevices() {
        devices.clear();
        int count = preferences.getInt("device_count", 0);
        
        for (int i = 0; i < count && i < MAX_DEVICES; i++) {
            String key = "dev_" + String(i);
            String data = preferences.getString(key.c_str(), "");
            if (data.length() > 0) {
                PairedDevice device = PairedDevice::deserialize(data);
                devices.push_back(device);
            }
        }
    }
    
    void saveDevices() {
        preferences.putInt("device_count", devices.size());
        
        for (size_t i = 0; i < devices.size(); i++) {
            String key = "dev_" + String(i);
            preferences.putString(key.c_str(), devices[i].serialize());
        }
        preferences.end();
        preferences.begin("device_db", false);
    }
    
    void addDevice(PairedDevice device) {
        for (auto& d : devices) {
            if (d.address == device.address || d.uniqueID == device.uniqueID) {
                d = device;
                saveDevices();
                Serial.println("Updated device: " + device.name);
                return;
            }
        }
        
        if (devices.size() < MAX_DEVICES) {
            devices.push_back(device);
            saveDevices();
            Serial.println("Added new device: " + device.name);
        }
    }
    
    void removeDevice(String address) {
        auto it = std::remove_if(devices.begin(), devices.end(),
            [address](PairedDevice& d) { return d.address == address; });
        
        if (it != devices.end()) {
            devices.erase(it, devices.end());
            saveDevices();
            Serial.println("Removed device: " + address);
        }
    }
    
    std::vector<PairedDevice> getAllDevices() {
        return devices;
    }
    
    PairedDevice* getDeviceByAddress(String address) {
        for (auto& d : devices) {
            if (d.address == address) {
                return &d;
            }
        }
        return nullptr;
    }
    
    PairedDevice* getDeviceByUID(String uid) {
        for (auto& d : devices) {
            if (d.uniqueID == uid) {
                return &d;
            }
        }
        return nullptr;
    }
    
    std::vector<PairedDevice> getAutoConnectDevices() {
        std::vector<PairedDevice> result;
        for (auto& d : devices) {
            if (d.autoConnect) {
                result.push_back(d);
            }
        }
        return result;
    }
    
    PairedDevice* getHighestPriorityDevice() {
        if (devices.empty()) return nullptr;
        
        PairedDevice* best = &devices[0];
        for (auto& d : devices) {
            if (d.priority > best->priority) {
                best = &d;
            }
        }
        return best;
    }
    
    void updateConnection(String address, float rssi) {
        PairedDevice* device = getDeviceByAddress(address);
        if (device) {
            device->lastSeen = millis();
            device->lastConnected = millis();
            device->connectionCount++;
            device->avgRSSI = (device->avgRSSI * (device->connectionCount - 1) + rssi) / 
                             device->connectionCount;
            saveDevices();
        }
    }
    
    void printAllDevices() {
        Serial.println("========================================");
        Serial.println("PAIRED DEVICES (" + String(devices.size()) + ")");
        Serial.println("========================================");
        for (size_t i = 0; i < devices.size(); i++) {
            auto& d = devices[i];
            Serial.printf("#%d: %s | MAC: %s | Priority: %d | Auto: %s\n",
                i+1, d.name.c_str(), d.address.c_str(), d.priority,
                d.autoConnect ? "YES" : "NO");
        }
        Serial.println("========================================");
    }
};

// ===== BLE GLOBALS =====
NimBLEClient* pClient = NULL;
NimBLERemoteCharacteristic* pChar = NULL;
NimBLERemoteCharacteristic* pIdentityChar = NULL;

// ===== CONNECTION STATE =====
bool connected = false;
bool connecting = false;
bool dataReceived = false;
bool waitingForData = false;
String currentDeviceAddress = "";
String currentDeviceUID = "";
unsigned long connectStartTime = 0;
unsigned long dataRetryStartTime = 0;
unsigned long lastReconnectAttempt = 0;
unsigned long lastDataTime = 0;
int connectAttempts = 0;
int successfulConnects = 0;
int dataReceptionAttempts = 0;
int successfulDataReceptions = 0;

// ===== CORE LOGIC VARIABLES =====
bool beltWorn = false;
unsigned long fallTime = 0;
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 1000;
String micMessage = "";
bool sensorInitialized = false;

// Heart rate data received from wrist
float heartRate = 0;
float spo2 = 0;
long irValue = 0;
long redValue = 0;
float bodyTemp = 0;

// ===== DEVICE DATABASE INSTANCE =====
DeviceDatabase deviceDB;

// ===== FORWARD DECLARATIONS =====
void connectToDevice(String address);
void processReceivedData(String encryptedData);
void continuousScan();
void sendDataToServer(SystemState state, float hr, float oxygen, float temp, bool worn, float acc, String micAudio);
String getStateName(SystemState state);
void sendSmartSMSAlert(String alertType, String message);
bool sendTwilioSMS(String alertType, String message);
bool sendGSMAlert(String alertType, String message);

// ===== CLIENT CALLBACKS =====
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) {
        connected = true;
        connecting = false;
        successfulConnects++;
        
        Serial.println("========================================");
        Serial.println("[BLE] Connected to Wrist!");
        Serial.print("Connected to: ");
        Serial.println(pClient->getPeerAddress().toString().c_str());
        Serial.println("========================================");
        
        // Update database
        String address = pClient->getPeerAddress().toString().c_str();
        deviceDB.updateConnection(address, 0);
        
        // Start data reception
        waitingForData = true;
        dataRetryStartTime = millis();
        dataReceived = false;
    }
    
    void onDisconnect(NimBLEClient* pClient) {
        connected = false;
        connecting = false;
        
        Serial.println("[BLE] Disconnected");
        Serial.println("Will try to reconnect...");
        
        if (pClient) {
            NimBLEDevice::deleteClient(pClient);
            pClient = NULL;
        }
        pChar = NULL;
        pIdentityChar = NULL;
        
        dataReceived = false;
        waitingForData = false;
        currentDeviceAddress = "";
        currentDeviceUID = "";
    }
    
    void onAuthenticationComplete(NimBLEClient* pClient) {
        Serial.println("[BLE] Authentication complete");
    }
};

// ===== NOTIFICATION CALLBACK =====
void notifyCallback(NimBLERemoteCharacteristic* pChar, uint8_t* data, size_t len, bool isNotify) {
    if (len == 0) return;
    
    dataReceptionAttempts++;
    
    String encryptedData = String((char*)data).substring(0, len);
    Serial.print("[BLE] Encrypted received: ");
    Serial.println(encryptedData);
    
    processReceivedData(encryptedData);
}

// ===== PROCESS RECEIVED DATA FROM WRIST =====
void processReceivedData(String encryptedData) {
    // DECRYPT DATA
    String decryptedData = EncryptionManager::decrypt(encryptedData);
    
    if (decryptedData.length() == 0) {
        Serial.println("Decryption failed!");
        return;
    }
    
    Serial.print("Decrypted: ");
    Serial.println(decryptedData);
    
    // Parse decrypted data (HR:SPO2:IR:TEMP)
    int firstColon = decryptedData.indexOf(':');
    int secondColon = decryptedData.indexOf(':', firstColon + 1);
    int thirdColon = decryptedData.indexOf(':', secondColon + 1);
    
    if (firstColon > 0 && secondColon > 0 && thirdColon > 0) {
        heartRate = decryptedData.substring(0, firstColon).toFloat();
        spo2 = decryptedData.substring(firstColon + 1, secondColon).toFloat();
        irValue = decryptedData.substring(secondColon + 1, thirdColon).toFloat();
        bodyTemp = decryptedData.substring(thirdColon + 1).toFloat();
        
        successfulDataReceptions++;
        dataReceived = true;
        waitingForData = false;
        lastDataTime = millis();
        
        Serial.println("========================================");
        Serial.println("DATA RECEIVED FROM WRIST");
        Serial.print("HR: ");
        Serial.print(heartRate);
        Serial.print(" | SpO2: ");
        Serial.print(spo2);
        Serial.print(" | IR: ");
        Serial.print(irValue);
        Serial.print(" | Temp: ");
        Serial.println(bodyTemp);
        Serial.println("========================================");
        
        // Reset data retry timer
        dataRetryStartTime = millis();
    } else {
        Serial.println("Invalid data format!");
    }
}

// ===== SCAN CALLBACKS =====
class ScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) {
        String deviceName = advertisedDevice->getName().c_str();
        String deviceAddress = advertisedDevice->getAddress().toString().c_str();
        int rssi = advertisedDevice->getRSSI();
        
        if (deviceName.length() > 0) {
            Serial.print("Found: ");
            Serial.print(deviceName);
            Serial.print(" @ ");
            Serial.print(deviceAddress);
            Serial.print(" (RSSI: ");
            Serial.print(rssi);
            Serial.println(" dBm)");
        }
        
        if (deviceName == WRIST_NAME && !connected && !connecting) {
            // Check if this is a paired device
            PairedDevice* paired = deviceDB.getDeviceByAddress(deviceAddress);
            
            if (paired) {
                // Found a paired device - connect!
                Serial.println("Found paired device: " + paired->name);
                Serial.print("Address: ");
                Serial.println(deviceAddress);
                
                NimBLEDevice::getScan()->stop();
                connectToDevice(deviceAddress);
            } else {
                // New device found - check if we should auto-add
                Serial.println("Found new device: " + deviceAddress);
                // Optionally auto-add based on policy
                // For security, we'll require manual addition
            }
        }
    }
    
    void onScanEnd(const NimBLEScanResults& results) {
        Serial.println("Scan cycle completed, restarting...");
        // Continuous scanning - will restart in loop
    }
};

// ===== CONNECT TO DEVICE =====
void connectToDevice(String address) {
    if (connecting || connected) {
        Serial.println("Already connecting or connected");
        return;
    }
    
    PairedDevice* device = deviceDB.getDeviceByAddress(address);
    if (!device) {
        Serial.println("Device not in database!");
        return;
    }
    
    connecting = true;
    connectStartTime = millis();
    connectAttempts++;
    currentDeviceAddress = address;
    currentDeviceUID = device->uniqueID;
    
    Serial.println("========================================");
    Serial.println("Connecting to: " + device->name);
    Serial.print("MAC: ");
    Serial.println(address);
    Serial.print("UID: ");
    Serial.println(device->uniqueID);
    Serial.print("Attempt #");
    Serial.println(connectAttempts);
    Serial.println("========================================");
    
    if (pClient) {
        NimBLEDevice::deleteClient(pClient);
        pClient = NULL;
        delay(50);
    }
    
    pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(new ClientCallbacks());
    pClient->setConnectTimeout(5);
    
    NimBLEAddress bleAddress(address.c_str());
    if (pClient->connect(bleAddress)) {
        Serial.println("Connected! Discovering services...");
        delay(200);
        
        NimBLERemoteService* pService = pClient->getService(SERVICE_UUID);
        if (pService) {
            Serial.println("Service found");
            
            // Get identity characteristic
            pIdentityChar = pService->getCharacteristic(IDENTITY_UUID);
            if (pIdentityChar) {
                // Verify device identity
                std::string idStr = pIdentityChar->readValue();
                String deviceUID = String(idStr.c_str());
                
                if (deviceUID == currentDeviceUID) {
                    Serial.println("Device identity verified!");
                } else {
                    Serial.println("Device identity mismatch!");
                    Serial.println("Expected: " + currentDeviceUID);
                    Serial.println("Got: " + deviceUID);
                    pClient->disconnect();
                    connecting = false;
                    return;
                }
            }
            
            // Get data characteristic
            pChar = pService->getCharacteristic(CHARACTERISTIC_UUID);
            if (pChar) {
                Serial.println("Characteristic found");
                
                if (pChar->canNotify()) {
                    pChar->subscribe(true, notifyCallback);
                    Serial.println("Subscribed to notifications");
                    
                    // Try to read initial data
                    std::string value = pChar->readValue();
                    if (value.length() > 0) {
                        notifyCallback(pChar, (uint8_t*)value.c_str(), value.length(), false);
                    }
                    
                    connected = true;
                    connecting = false;
                    deviceDB.updateConnection(address, 0);
                    
                    Serial.println("========================================");
                    Serial.println("SUCCESSFULLY CONNECTED!");
                    Serial.println("========================================");
                    return;
                }
            }
        }
        
        Serial.println("Failed to discover services");
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        pClient = NULL;
        connecting = false;
    } else {
        Serial.println("Connection failed");
        NimBLEDevice::deleteClient(pClient);
        pClient = NULL;
        connecting = false;
    }
}

// ===== CONTINUOUS SCANNING =====
void continuousScan() {
    if (!connected && !connecting) {
        NimBLEScan* pScan = NimBLEDevice::getScan();
        pScan->setScanCallbacks(new ScanCallbacks(), false);
        pScan->setActiveScan(true);
        pScan->setInterval(50);
        pScan->setWindow(25);
        pScan->setDuplicateFilter(false);
        
        pScan->start(SCAN_INTERVAL, false);
    }
}

// ===== MANAGE DATA RECEPTION =====
void manageDataReception() {
    if (connected && waitingForData && !dataReceived) {
        unsigned long elapsed = millis() - dataRetryStartTime;
        
        if (elapsed > DATA_RETRY_TIMEOUT) {
            Serial.println("DATA RECEPTION TIMEOUT (1 minute)");
            Serial.println("Re-requesting data...");
            
            if (pChar) {
                pChar->unsubscribe();
                delay(50);
                pChar->subscribe(true, notifyCallback);
                
                std::string value = pChar->readValue();
                if (value.length() > 0) {
                    notifyCallback(pChar, (uint8_t*)value.c_str(), value.length(), false);
                }
                
                dataRetryStartTime = millis();
            } else {
                Serial.println("Characteristic lost! Reconnecting...");
                if (pClient) {
                    pClient->disconnect();
                }
                connected = false;
                waitingForData = false;
            }
        } else {
            if (elapsed % 5000 < 100) {
                Serial.print("Waiting for data... ");
                Serial.print(elapsed / 1000);
                Serial.println("s elapsed");
            }
        }
    }
}

// ===== MANAGE CONNECTION TIMEOUT =====
void manageConnectionTimeout() {
    if (connecting) {
        unsigned long elapsed = millis() - connectStartTime;
        
        if (elapsed > CONNECTION_TIMEOUT) {
            Serial.println("========================================");
            Serial.println("CONNECTION TIMEOUT (1 minute)");
            Serial.print("Attempts: ");
            Serial.println(connectAttempts);
            Serial.println("Retrying...");
            Serial.println("========================================");
            
            if (pClient) {
                pClient->disconnect();
                NimBLEDevice::deleteClient(pClient);
                pClient = NULL;
            }
            connecting = false;
            pChar = NULL;
            pIdentityChar = NULL;
        }
    }
}

// ===== AUTO RECONNECT =====
void autoReconnect() {
    if (!connected && !connecting) {
        unsigned long now = millis();
        
        if (now - lastReconnectAttempt > RECONNECT_DELAY) {
            Serial.println("Attempting to reconnect...");
            
            // Get all auto-connect devices
            std::vector<PairedDevice> autoDevices = deviceDB.getAutoConnectDevices();
            
            if (autoDevices.size() > 0) {
                // Try highest priority first
                for (auto& device : autoDevices) {
                    if (device.address != currentDeviceAddress) {
                        connectToDevice(device.address);
                        if (connecting) break;
                    }
                }
            } else {
                // No auto-connect devices, try any paired device
                std::vector<PairedDevice> allDevices = deviceDB.getAllDevices();
                if (allDevices.size() > 0) {
                    connectToDevice(allDevices[0].address);
                }
            }
            
            lastReconnectAttempt = now;
        }
    }
}

// ===== STATUS REPORTING =====
void printStatus() {
    static unsigned long lastStatusTime = 0;
    
    if (millis() - lastStatusTime > 10000) {
        Serial.println("========================================");
        Serial.println("SYSTEM STATUS");
        Serial.print("Connected: ");
        Serial.println(connected ? "YES" : "NO");
        Serial.print("Connecting: ");
        Serial.println(connecting ? "YES" : "NO");
        Serial.print("Data Received: ");
        Serial.println(dataReceived ? "YES" : "NO");
        Serial.print("Paired Devices: ");
        Serial.println(deviceDB.getAllDevices().size());
        Serial.print("Connect Attempts: ");
        Serial.println(connectAttempts);
        Serial.print("Successful Connects: ");
        Serial.println(successfulConnects);
        Serial.print("Data Attempts: ");
        Serial.println(dataReceptionAttempts);
        Serial.print("Successful Data: ");
        Serial.println(successfulDataReceptions);
        Serial.print("Current State: ");
        Serial.println(getStateName(currentState));
        
        if (connected && pClient) {
            Serial.print("Connected to: ");
            Serial.println(pClient->getPeerAddress().toString().c_str());
        }
        Serial.println("========================================");
        lastStatusTime = millis();
    }
}

// ===== ADD DEMO DEVICES (For testing) =====
void addDemoDevices() {
    Serial.println("Adding demo devices...");
    
    // Senior Citizen 1's wristband
    PairedDevice device1;
    device1.address = "AA:BB:CC:DD:EE:01";
    device1.name = "SilverCare_Wrist_01";
    device1.uniqueID = "UID_001";
    device1.priority = 10;
    device1.autoConnect = true;
    device1.connectionCount = 0;
    device1.password = "SC2024_001";
    deviceDB.addDevice(device1);
    
    // Senior Citizen 2's wristband
    PairedDevice device2;
    device2.address = "AA:BB:CC:DD:EE:02";
    device2.name = "SilverCare_Wrist_02";
    device2.uniqueID = "UID_002";
    device2.priority = 8;
    device2.autoConnect = true;
    device2.connectionCount = 0;
    device2.password = "SC2024_002";
    deviceDB.addDevice(device2);
}

// ===== COMMAND PROCESSOR =====
void processSerialCommand() {
    if (!Serial.available()) return;
    
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "list") {
        deviceDB.printAllDevices();
    }
    else if (command == "connect") {
        autoReconnect();
    }
    else if (command.startsWith("connect ")) {
        String address = command.substring(8);
        connectToDevice(address);
    }
    else if (command == "disconnect") {
        if (pClient) {
            pClient->disconnect();
        }
        connected = false;
        connecting = false;
        Serial.println("Disconnected");
    }
    else if (command.startsWith("add ")) {
        // Format: add MAC:AA:BB:CC:DD:EE,Name:Device,UID:1234,Priority:5
        String data = command.substring(4);
        PairedDevice device;
        
        int macStart = data.indexOf("MAC:") + 4;
        int macEnd = data.indexOf(',', macStart);
        device.address = data.substring(macStart, macEnd);
        
        int nameStart = data.indexOf("Name:") + 5;
        int nameEnd = data.indexOf(',', nameStart);
        device.name = data.substring(nameStart, nameEnd);
        
        int uidStart = data.indexOf("UID:") + 4;
        int uidEnd = data.indexOf(',', uidStart);
        device.uniqueID = data.substring(uidStart, uidEnd);
        
        int priorityStart = data.indexOf("Priority:") + 9;
        if (priorityStart > 9) {
            int priorityEnd = data.indexOf(',', priorityStart);
            if (priorityEnd == -1) priorityEnd = data.length();
            device.priority = data.substring(priorityStart, priorityEnd).toInt();
        } else {
            device.priority = 5;
        }
        
        device.autoConnect = true;
        device.connectionCount = 0;
        device.password = "SC2024_" + device.uniqueID;
        
        deviceDB.addDevice(device);
        Serial.println("Device added successfully!");
    }
    else if (command.startsWith("remove ")) {
        String address = command.substring(7);
        deviceDB.removeDevice(address);
        if (currentDeviceAddress == address) {
            connected = false;
            connecting = false;
            if (pClient) {
                pClient->disconnect();
            }
        }
    }
    else if (command == "help") {
        Serial.println("========================================");
        Serial.println("COMMANDS");
        Serial.println("========================================");
        Serial.println("list                    - List all paired devices");
        Serial.println("connect                 - Auto-connect to best device");
        Serial.println("connect <MAC>           - Connect to specific device");
        Serial.println("disconnect              - Disconnect current device");
        Serial.println("add MAC:XX,Name:YY,...  - Add new device");
        Serial.println("remove <MAC>            - Remove device");
        Serial.println("help                    - Show this help");
        Serial.println("========================================");
    }
}

// ===== CORE LOGIC FUNCTIONS (MOVED FROM WRIST) =====

void initSensors() {
    // Initialize MPU6050
    if (!mpu.begin()) {
        Serial.println("MPU6050 NOT FOUND");
        while (1);
    }
    Serial.println("MPU6050 Initialized");

    // Initialize DS18B20
    tempSensor.begin();
    Serial.println("DS18B20 Initialized");
    
    sensorInitialized = true;
}

void initGSM() {
    gsmSerial.begin(GSM_BAUD);
    Serial.println("Initializing GSM Module...");
    delay(1000);
    
    gsmSerial.println("AT");
    delay(1000);
    if (gsmSerial.available()) {
        Serial.println("GSM Module Responding");
    } else {
        Serial.println("GSM Module Not Responding");
    }
    
    gsmSerial.println("AT+CMGF=1");
    delay(1000);
}

void initWiFi() {
    Serial.println("Initializing WiFiManager...");
    WiFiManager wifiManager;
    
    // Set timeout for captive portal (3 minutes)
    wifiManager.setConfigPortalTimeout(180);
    
    if (!wifiManager.autoConnect("SilverCare_Waist_AP")) {
        Serial.println("Failed to connect to WiFi and hit timeout");
        Serial.println("Local Operation Active");
    } else {
        Serial.println("WiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    }
}

void sendDataToServer(SystemState state, float hr, float oxygen, float temp, bool worn, float acc, String micAudio) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected - skipping send");
        return;
    }

    HTTPClient http;
    http.begin(serverURL);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<384> doc;
    doc["deviceId"] = deviceId;
    doc["beltType"] = beltType;
    doc["state"] = (int)state;
    doc["stateName"] = getStateName(state);
    doc["heartRate"] = hr;
    doc["spo2"] = oxygen;
    doc["temperature"] = temp;
    doc["beltWorn"] = worn;
    doc["acceleration"] = acc;
    doc["latitude"] = gpsLat;
    doc["longitude"] = gpsLng;
    doc["micMessageAudio"] = micAudio;
    doc["timestamp"] = millis();

    String payload;
    serializeJson(doc, payload);

    Serial.print("[WAIST BELT TELEMETRY SENT]: ");
    Serial.println(payload);

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.print("Server Response Code: ");
        Serial.println(httpResponseCode);

        if (response.indexOf("ACKNOWLEDGED") != -1 || response.indexOf("I am Fine") != -1) {
            Serial.println("[GUARDIAN ACKNOWLEDGED]: Clearing local buzzer and state!");
            digitalWrite(BUZZER_PIN, LOW);
            currentState = NORMAL;
        }
    } else {
        Serial.print("HTTP POST Error: ");
        Serial.println(httpResponseCode);
    }

    http.end();
}

String getStateName(SystemState state) {
    switch(state) {
        case NORMAL: return "NORMAL";
        case PREFALL: return "PREFALL";
        case SUDDEN_MOVEMENT: return "SUDDEN_MOVEMENT";
        case FALL_DETECTED: return "FALL_DETECTED";
        default: return "UNKNOWN";
    }
}

void sendSmartSMSAlert(String alertType, String message) {
    static unsigned long lastSMSTime = 0;
    static bool twilioSMSSent = false;
    static bool gsmSMSSent = false;
    const unsigned long SMS_RETRY_INTERVAL = 30000;
    
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

bool sendTwilioSMS(String alertType, String message) {
    StaticJsonDocument<256> doc;
    doc["alert_type"] = alertType;
    doc["message"] = message;
    doc["device_id"] = deviceId;
    doc["belt_type"] = beltType;
    doc["timestamp"] = millis();
    
    String payload;
    serializeJson(doc, payload);
    
    HTTPClient http;
    http.begin(twilioURL);
    http.addHeader("Content-Type", "application/json");
    
    int httpResponseCode = http.POST(payload);
    http.end();
    return (httpResponseCode == 200);
}

bool sendGSMAlert(String alertType, String message) {
    String guardianPhone = "+919322757538";
    
    gsmSerial.println("AT+CMGS=\"" + guardianPhone + "\"");
    delay(1000);
    
    String smsMessage = "SILVERCARE ALERT - " + alertType + "\n" + 
                        message + "\n" + 
                        "Device: " + deviceId + " (" + beltType + ")";
    gsmSerial.println(smsMessage);
    delay(1000);
    
    gsmSerial.write(26);
    delay(3000);
    
    while(gsmSerial.available()) {
        String response = gsmSerial.readString();
        if (response.indexOf("OK") != -1) return true;
    }
    return false;
}

// ================================================================
// ===== SETUP =====
// ================================================================
void setup() {
    Serial.begin(115200);
    delay(3000);
    
    Serial.println();
    Serial.println("========================================");
    Serial.println("=== SILVERCARE WAIST BELT BLE CLIENT ===");
    Serial.println("=== (Enhanced with Multi-Device Support) ===");
    Serial.println("========================================");
    
    // Initialize pins
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(MIC_BUTTON_PIN, INPUT_PULLUP);
    
    // Initialize GSM
    initGSM();
    
    // Initialize WiFi
    initWiFi();
    
    // Initialize sensors (MPU6050, DS18B20)
    initSensors();
    
    // Initialize BLE
    NimBLEDevice::setSecurityAuth(false, false, false);
    NimBLEDevice::init("");
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    
    Serial.print("Waist MAC: ");
    Serial.println(NimBLEDevice::getAddress().toString().c_str());
    
    // Initialize device database
    deviceDB.init();
    
    // Add demo devices if none exist
    if (deviceDB.getAllDevices().size() == 0) {
        addDemoDevices();
    }
    
    deviceDB.printAllDevices();
    
    Serial.println("========================================");
    Serial.println("AES-256 Encryption: ENABLED");
    Serial.println("Connection Timeout: 1 MINUTE");
    Serial.println("Data Reception Timeout: 1 MINUTE");
    Serial.println("Continuous Scanning: ENABLED");
    Serial.println("Multi-Device Support: ENABLED");
    Serial.println("Device Verification: ENABLED");
    Serial.println("Fall Detection: ENABLED");
    Serial.println("GPS/GSM: ENABLED");
    Serial.println("========================================");
    Serial.println("Type 'help' for commands");
    Serial.println("Starting continuous scan...");
    Serial.println("========================================");
    
    // Start scanning immediately
    lastReconnectAttempt = millis() - RECONNECT_DELAY; // Allow immediate connect
}

// ================================================================
// ===== LOOP =====
// ================================================================
void loop() {
    // ===== 1. PROCESS SERIAL COMMANDS =====
    processSerialCommand();
    
    // ===== 2. CONTINUOUS SCANNING (NO GAPS) =====
    if (!connected && !connecting) {
        continuousScan();
        // After scan ends, loop will restart it immediately
        // This ensures continuous scanning with no gaps
    }
    
    // ===== 3. MANAGE CONNECTION TIMEOUT (1 MINUTE) =====
    manageConnectionTimeout();
    
    // ===== 4. MANAGE DATA RECEPTION (1 MINUTE WITH RETRIES) =====
    manageDataReception();
    
    // ===== 5. AUTO RECONNECT =====
    autoReconnect();
    
    // ===== 6. STATUS REPORTING =====
    printStatus();
    
    // ===== 7. CORE LOGIC (MOVED FROM WRIST) =====
    
    // MPU6050 READING
    sensors_event_t acc, gyro, temp;
    mpu.getEvent(&acc, &gyro, &temp);

    float ax = acc.acceleration.x;
    float ay = acc.acceleration.y;
    float az = acc.acceleration.z;
    float accMag = sqrt(ax * ax + ay * ay + az * az);
    float accMagG = accMag / 9.8;

    // TEMPERATURE READING (Local DS18B20)
    tempSensor.requestTemperatures();
    float bodyTempLocal = tempSensor.getTempCByIndex(0);

    // BELT WORN CHECK (Using IR value from wrist)
    #define IR_WORN_THRESHOLD 4500
    beltWorn = (irValue > IR_WORN_THRESHOLD && bodyTempLocal > TEMP_WORN_THRESHOLD);
    bool vitalsAbnormal = (heartRate < HR_LOW || heartRate > HR_HIGH || spo2 < SPO2_LOW);

    // MICROPHONE TRIGGER CHECK
    if (digitalRead(MIC_BUTTON_PIN) == LOW) {
        micMessage = "Senior citizen pressed Waist Mic button: 'Help needed!'";
        Serial.println("[MIC TRIGGERED]: " + micMessage);
    } else if (micMessage.length() > 0 && currentState == NORMAL) {
        micMessage = "";
    }

    // STATE MACHINE
    if (currentState == FALL_DETECTED) {
        goto STATE_OUTPUT;
    }

    if (accMagG > INSTABILITY_THRESHOLD && accMagG <= SUDDEN_THRESHOLD && beltWorn && vitalsAbnormal) {
        currentState = PREFALL;
        micMessage = "Pre-fall instability detected on Waist Belt. Asking: 'Are you okay?'";
    } else if (accMagG > SUDDEN_THRESHOLD && accMagG <= FALL_THRESHOLD) {
        currentState = SUDDEN_MOVEMENT;
    } else if (accMagG > FALL_THRESHOLD && beltWorn) {
        currentState = FALL_DETECTED;
        fallTime = millis();
        micMessage = "EMERGENCY: Fall impact detected on Waist Belt! Urgent help requested.";
    } else {
        currentState = NORMAL;
    }

    STATE_OUTPUT:
    switch (currentState) {
        case NORMAL:
            digitalWrite(BUZZER_PIN, LOW);
            break;

        case PREFALL:
            digitalWrite(BUZZER_PIN, LOW);
            sendSmartSMSAlert("PRE-FALL", "Pre-fall detected on Waist Belt! Please check senior ward.");
            break;

        case SUDDEN_MOVEMENT:
            digitalWrite(BUZZER_PIN, LOW);
            break;

        case FALL_DETECTED:
            digitalWrite(BUZZER_PIN, HIGH);
            sendSmartSMSAlert("FALL", "EMERGENCY: Fall detected on Waist Belt! Immediate assistance required!");
            break;
    }

    // USER MANUAL OK OVERRIDE
    if (digitalRead(BUTTON_PIN) == LOW) {
        Serial.println("USER RESPONSE: I'M OK");
        digitalWrite(BUZZER_PIN, LOW);
        currentState = NORMAL;
        micMessage = "Senior pressed 'I AM OK' button on waist belt.";
    }

    // SEND TELEMETRY TO SPRING BOOT SERVER
    unsigned long currentTime = millis();
    if (currentTime - lastSendTime >= SEND_INTERVAL) {
        sendDataToServer(currentState, heartRate, spo2, bodyTempLocal, beltWorn, accMagG, micMessage);
        lastSendTime = currentTime;
    }
    
    delay(10); // Small delay to prevent watchdog reset
}