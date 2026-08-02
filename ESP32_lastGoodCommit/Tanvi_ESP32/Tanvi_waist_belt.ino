// ============================================================
//  CRITICAL FIXES FOR ARDUINO IDE - MUST BE FIRST
// ============================================================
#define ESP_TASK_BT_BT_STACK_SIZE 10240
#define ESP_TASK_BT_GATTS_STACK_SIZE 10240
#define ESP_TASK_BT_GATTC_STACK_SIZE 10240
#define ESP_TASK_BT_BLE_STACK_SIZE 10240

// ============================================================
//  INCLUDES
// ============================================================
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Preferences.h>
#include <vector>
#include <algorithm>
#include <mbedtls/aes.h>
#include <mbedtls/gcm.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>
#include <esp_system.h>

// ============================================================
//  INCREASE BUFFER SIZES
// ============================================================
#define SERIAL_RX_BUFFER_SIZE 4096
#define SERIAL_TX_BUFFER_SIZE 4096
#define BLE_MTU_SIZE 247
#define SCAN_BUFFER_SIZE 1024
#define ENCRYPTION_BUFFER_SIZE 1024

// ============================================================
//  CONFIGURATION
// ============================================================
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define IDENTITY_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define WRIST_NAME "SilverCare_Wrist"  // Device name to scan for

// ============================================================
//  ENCRYPTION CONFIGURATION
// ============================================================
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

// ============================================================
//  TIME CONFIGURATION
// ============================================================
const unsigned long CONNECTION_TIMEOUT = 60000;
const unsigned long DATA_RETRY_TIMEOUT = 60000;
const unsigned long SCAN_INTERVAL = 5;
const unsigned long RECONNECT_DELAY = 5000;  // Reduced from 30000 for faster reconnection
const unsigned long WATCHDOG_TIMEOUT = 5000;

// ============================================================
//  DEVICE STRUCTURE
// ============================================================
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
        return address + "|" + name + "|" + uniqueID + "|" +
               String(priority) + "|" + String(autoConnect) + "|" +
               String(connectionCount) + "|" + password + "|" +
               String(lastSeen) + "|" + String(lastConnected);
    }

    static PairedDevice deserialize(String data) {
        PairedDevice device;
        try {
            int pos1 = data.indexOf('|');
            int pos2 = data.indexOf('|', pos1 + 1);
            int pos3 = data.indexOf('|', pos2 + 1);
            int pos4 = data.indexOf('|', pos3 + 1);
            int pos5 = data.indexOf('|', pos4 + 1);
            int pos6 = data.indexOf('|', pos5 + 1);
            int pos7 = data.indexOf('|', pos6 + 1);
            int pos8 = data.indexOf('|', pos7 + 1);

            if (pos1 > 0 && pos2 > 0 && pos3 > 0 && pos4 > 0 && pos5 > 0 && pos6 > 0) {
                device.address = data.substring(0, pos1);
                device.name = data.substring(pos1 + 1, pos2);
                device.uniqueID = data.substring(pos2 + 1, pos3);
                device.priority = data.substring(pos3 + 1, pos4).toInt();
                device.autoConnect = data.substring(pos4 + 1, pos5) == "1";
                device.connectionCount = data.substring(pos5 + 1, pos6).toInt();
                device.password = data.substring(pos6 + 1, pos7);
                device.lastSeen = data.substring(pos7 + 1, pos8).toInt();
                device.lastConnected = data.substring(pos8 + 1).toInt();
            }
        } catch (...) {
            device.address = "";
            device.name = "";
            device.uniqueID = "";
            device.priority = 0;
            device.autoConnect = false;
            device.connectionCount = 0;
            device.password = "";
            device.lastSeen = 0;
            device.lastConnected = 0;
        }
        device.isActive = false;
        device.avgRSSI = 0;

        return device;
    }
};

// ============================================================
//  ENCRYPTION MANAGER - SIMPLE FIXED VERSION (NO ENTROPY)
// ============================================================
class EncryptionManager {
private:
    static bool initialized;
    static mbedtls_gcm_context gcm_ctx;

public:
    static bool init() {
        if (!initialized) {
            mbedtls_gcm_init(&gcm_ctx);
            int ret = mbedtls_gcm_setkey(&gcm_ctx, MBEDTLS_CIPHER_ID_AES, ENCRYPTION_KEY, 256);
            initialized = (ret == 0);
            Serial.println(initialized ? "✅ Encryption initialized" : "❌ Encryption init failed");
        }
        return initialized;
    }

    static String encrypt(const String& plaintext) {
        if (!initialized) {
            if (!init()) return "";
        }
        if (plaintext.length() == 0 || plaintext.length() > 128) return "";

        mbedtls_gcm_free(&gcm_ctx);
        mbedtls_gcm_init(&gcm_ctx);
        mbedtls_gcm_setkey(&gcm_ctx, MBEDTLS_CIPHER_ID_AES, ENCRYPTION_KEY, 256);

        size_t plaintextLen = plaintext.length();
        uint8_t ciphertext[256];
        uint8_t tag[16];
        size_t outputLen = 0;

        int ret = mbedtls_gcm_crypt_and_tag(&gcm_ctx, MBEDTLS_GCM_ENCRYPT, plaintextLen,
                                         ENCRYPTION_IV, 12, NULL, 0,
                                         (const uint8_t*)plaintext.c_str(),
                                         ciphertext, 16, tag);

        if (ret != 0) {
            Serial.println("❌ Encryption failed");
            return "";
        }

        String result = "";
        for (size_t i = 0; i < plaintextLen; i++) {
            if (ciphertext[i] < 16) result += "0";
            result += String(ciphertext[i], HEX);
            if (i < plaintextLen - 1) result += ":";
        }
        result += "|";
        for (int i = 0; i < 16; i++) {
            if (tag[i] < 16) result += "0";
            result += String(tag[i], HEX);
            if (i < 15) result += ":";
        }

        return result;
    }

    static String decrypt(const String& encrypted) {
        if (!initialized) {
            if (!init()) return "";
        }
        if (encrypted.length() == 0) return "";

        mbedtls_gcm_free(&gcm_ctx);
        mbedtls_gcm_init(&gcm_ctx);
        mbedtls_gcm_setkey(&gcm_ctx, MBEDTLS_CIPHER_ID_AES, ENCRYPTION_KEY, 256);

        int separator = encrypted.indexOf('|');
        if (separator == -1) return "";

        String ciphertextStr = encrypted.substring(0, separator);
        String tagStr = encrypted.substring(separator + 1);

        if (ciphertextStr.length() == 0 || tagStr.length() == 0) return "";

        uint8_t ciphertext[128];
        uint8_t tag[16];
        size_t ciphertextLen = 0;

        int pos = 0;
        while (pos < ciphertextStr.length() && ciphertextLen < 128) {
            int end = ciphertextStr.indexOf(':', pos);
            if (end == -1) end = ciphertextStr.length();
            if (end > pos) {
                String byteStr = ciphertextStr.substring(pos, end);
                ciphertext[ciphertextLen++] = strtol(byteStr.c_str(), NULL, 16);
            }
            pos = end + 1;
        }

        pos = 0;
        int tagIdx = 0;
        while (pos < tagStr.length() && tagIdx < 16) {
            int end = tagStr.indexOf(':', pos);
            if (end == -1) end = tagStr.length();
            if (end > pos) {
                String byteStr = tagStr.substring(pos, end);
                tag[tagIdx++] = strtol(byteStr.c_str(), NULL, 16);
            }
            pos = end + 1;
        }

        uint8_t plaintext[128];
        size_t outputLen = 0;

        int ret = mbedtls_gcm_auth_decrypt(&gcm_ctx, ciphertextLen,
                                       ENCRYPTION_IV, 12, NULL, 0,
                                       tag, 16,
                                       ciphertext, plaintext);

        if (ret != 0) {
            Serial.println("❌ Decryption failed");
            return "";
        }

        return String((char*)plaintext, ciphertextLen);
    }
};

bool EncryptionManager::initialized = false;
mbedtls_gcm_context EncryptionManager::gcm_ctx;

// ============================================================
//  DEVICE DATABASE MANAGER
// ============================================================
class DeviceDatabase {
private:
    Preferences preferences;
    std::vector<PairedDevice> devices;
    const int MAX_DEVICES = 20;
    bool initialized = false;

public:
    void init() {
        if (initialized) return;

        bool opened = preferences.begin("device_db", false);

        if (!opened) {
            Serial.println("❌ Failed to initialize preferences");
            preferences.end();
            preferences.begin("device_db", true);
            preferences.clear();
            preferences.end();
            opened = preferences.begin("device_db", false);
            Serial.println(opened ? "🔄 Preferences reset" : "❌ Preferences still unavailable");
        }

        try {
            loadDevices();
        } catch (...) {
            Serial.println("❌ Failed to load devices, starting empty");
            devices.clear();
        }

        initialized = true;
        Serial.println("📚 Loaded " + String(devices.size()) + " paired devices");
    }

    void loadDevices() {
        devices.clear();
        int count = preferences.getInt("device_count", 0);
        if (count > MAX_DEVICES) count = MAX_DEVICES;

        for (int i = 0; i < count; i++) {
            String key = "dev_" + String(i);
            String data = preferences.getString(key.c_str(), "");
            if (data.length() > 0) {
                PairedDevice device = PairedDevice::deserialize(data);
                if (device.address.length() > 0) {
                    devices.push_back(device);
                }
            }
        }
    }

    void saveDevices() {
        if (!initialized) return;
        try {
            preferences.putInt("device_count", devices.size());

            for (size_t i = 0; i < devices.size(); i++) {
                String key = "dev_" + String(i);
                preferences.putString(key.c_str(), devices[i].serialize());
            }
            preferences.end();
            preferences.begin("device_db", false);
        } catch (...) {
            Serial.println("❌ Failed to save devices");
        }
    }

    void addDevice(PairedDevice device) {
        for (auto& d : devices) {
            if (d.address == device.address || d.uniqueID == device.uniqueID) {
                d = device;
                saveDevices();
                return;
            }
        }

        if (devices.size() < MAX_DEVICES) {
            devices.push_back(device);
            saveDevices();
        }
    }

    void removeDevice(String address) {
        auto it = std::remove_if(devices.begin(), devices.end(),
            [address](PairedDevice& d) { return d.address == address; });

        if (it != devices.end()) {
            devices.erase(it, devices.end());
            saveDevices();
        }
    }

    std::vector<PairedDevice> getAllDevices() { return devices; }

    PairedDevice* getDeviceByAddress(String address) {
        for (auto& d : devices) {
            if (d.address == address) return &d;
        }
        return nullptr;
    }

    PairedDevice* getDeviceByUID(String uid) {
        for (auto& d : devices) {
            if (d.uniqueID == uid) return &d;
        }
        return nullptr;
    }

    std::vector<PairedDevice> getAutoConnectDevices() {
        std::vector<PairedDevice> result;
        for (auto& d : devices) {
            if (d.autoConnect) result.push_back(d);
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
        Serial.println("📚 PAIRED DEVICES (" + String(devices.size()) + ")");
        Serial.println("========================================");
        for (size_t i = 0; i < devices.size(); i++) {
            auto& d = devices[i];
            Serial.printf("#%d: %s | MAC: %s | Priority: %d | Auto: %s | Conn: %d\n",
                i+1, d.name.c_str(), d.address.c_str(), d.priority,
                d.autoConnect ? "YES" : "NO", d.connectionCount);
        }
        Serial.println("========================================");
    }
};

// ============================================================
//  MEMORY MANAGEMENT HELPERS
// ============================================================
void printMemoryInfo() {
    Serial.print("📊 Free Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.print(" bytes | Free PSRAM: ");
    Serial.print(ESP.getFreePsram());
    Serial.println(" bytes");
}

bool hasSufficientMemory(size_t required) {
    size_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < required + 4096) {
        Serial.printf("⚠️ Low memory: %d bytes free, need %d\n", freeHeap, required);
        return false;
    }
    return true;
}

// ============================================================
//  BLE GLOBALS
// ============================================================
BLEClient* pClient = NULL;
BLERemoteCharacteristic* pChar = NULL;
BLERemoteCharacteristic* pIdentityChar = NULL;
BLEScan* pScan = NULL;

// Reuse callback instances to prevent memory leaks
class ScanCallbacks;
class ClientCallbacks;
ScanCallbacks* scanCallbacksInstance = nullptr;
ClientCallbacks* clientCallbacksInstance = nullptr;

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
unsigned long lastLoopTime = 0;
bool bleInitialized = false;

DeviceDatabase deviceDB;

// ============================================================
//  FORWARD DECLARATIONS
// ============================================================
void connectToDevice(String address);
void processReceivedData(String encryptedData);
void continuousScan();
void deleteClient(BLEClient* client);

// ============================================================
//  HELPER: DELETE CLIENT
// ============================================================
void deleteClient(BLEClient* client) {
    if (client != NULL) {
        try {
            if (client->isConnected()) {
                client->disconnect();
                delay(50);
            }
        } catch (...) {
            // Ignore errors during cleanup
        }
        try {
            delete client;
        } catch (...) {
            // Ignore
        }
        client = NULL;
    }
    delay(50);
}

// ============================================================
//  HELPER: Read characteristic value
// ============================================================
String readCharacteristicValue(BLERemoteCharacteristic* pChar) {
    if (pChar == NULL) return "";
    try {
        auto value = pChar->readValue();
        return String(value.c_str());
    } catch (...) {
        return "";
    }
}

// ============================================================
//  CLIENT CALLBACKS
// ============================================================
class ClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient* pClient) {
        connected = true;
        connecting = false;
        successfulConnects++;

        Serial.println("========================================");
        Serial.println("✅ [BLE] Connected to Wrist!");
        Serial.print("📱 Connected to: ");
        Serial.println(pClient->getPeerAddress().toString().c_str());
        Serial.println("========================================");

        String address = pClient->getPeerAddress().toString().c_str();
        deviceDB.updateConnection(address, 0);

        waitingForData = true;
        dataRetryStartTime = millis();
        dataReceived = false;
    }

    void onDisconnect(BLEClient* pClient) {
        connected = false;
        connecting = false;

        Serial.println("❌ [BLE] Disconnected");
        Serial.println("🔄 Will try to reconnect...");

        pClient = NULL;
        pChar = NULL;
        pIdentityChar = NULL;

        dataReceived = false;
        waitingForData = false;
        currentDeviceAddress = "";
        currentDeviceUID = "";
    }
};

// ============================================================
//  NOTIFICATION CALLBACK
// ============================================================
void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* data, size_t len, bool isNotify) {
    if (len == 0 || data == NULL || len > 1024) return;

    dataReceptionAttempts++;

    try {
        String encryptedData;
        encryptedData.reserve(len + 1);
        encryptedData = String((char*)data).substring(0, len);
        Serial.print("📥 [BLE] Encrypted received: ");
        Serial.println(encryptedData);

        processReceivedData(encryptedData);
    } catch (...) {
        Serial.println("❌ Error processing notification");
    }
}

// ============================================================
//  PROCESS RECEIVED DATA
// ============================================================
void processReceivedData(String encryptedData) {
    try {
        String decryptedData = EncryptionManager::decrypt(encryptedData);

        if (decryptedData.length() == 0) {
            Serial.println("❌ Decryption failed!");
            return;
        }

        Serial.print("🔓 Decrypted: ");
        Serial.println(decryptedData);

        int firstColon = decryptedData.indexOf(':');
        int secondColon = decryptedData.indexOf(':', firstColon + 1);
        int thirdColon = decryptedData.indexOf(':', secondColon + 1);

        if (firstColon > 0 && secondColon > 0 && thirdColon > 0) {
            float hr = decryptedData.substring(0, firstColon).toFloat();
            float spo2 = decryptedData.substring(firstColon + 1, secondColon).toFloat();
            long ir = decryptedData.substring(secondColon + 1, thirdColon).toFloat();
            float temp = decryptedData.substring(thirdColon + 1).toFloat();

            successfulDataReceptions++;
            dataReceived = true;
            waitingForData = false;
            lastDataTime = millis();

            Serial.println("========================================");
            Serial.println("✅ DATA RECEIVED SUCCESSFULLY!");
            Serial.print("❤️ HR: ");
            Serial.print(hr);
            Serial.print(" | SpO2: ");
            Serial.print(spo2);
            Serial.print(" | IR: ");
            Serial.print(ir);
            Serial.print(" | Temp: ");
            Serial.println(temp);
            Serial.println("========================================");

            dataRetryStartTime = millis();
        } else {
            Serial.println("❌ Invalid data format!");
        }
    } catch (...) {
        Serial.println("❌ Error processing data");
    }
}

// ============================================================
//  SCAN CALLBACKS - MODIFIED TO AUTO-ADD DISCOVERED DEVICES
// ============================================================
class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        try {
            String deviceName = advertisedDevice.getName().c_str();
            String deviceAddress = advertisedDevice.getAddress().toString().c_str();
            int rssi = advertisedDevice.getRSSI();

            // Only process if device name matches our wrist band
            if (deviceName == WRIST_NAME) {
                Serial.println("========================================");
                Serial.print("📱 Found Wrist Band: ");
                Serial.print(deviceName);
                Serial.print(" @ ");
                Serial.print(deviceAddress);
                Serial.print(" (RSSI: ");
                Serial.print(rssi);
                Serial.println(" dBm)");
                Serial.println("========================================");

                // Check if device is already in database
                PairedDevice* existing = deviceDB.getDeviceByAddress(deviceAddress);
                
                if (!existing) {
                    // Auto-add new device to database
                    PairedDevice newDevice;
                    newDevice.address = deviceAddress;
                    newDevice.name = deviceName;
                    newDevice.uniqueID = "AUTO_" + String(millis(), HEX);
                    newDevice.priority = 10;
                    newDevice.autoConnect = true;
                    newDevice.connectionCount = 0;
                    newDevice.password = "SC2024_AUTO";
                    newDevice.lastSeen = millis();
                    newDevice.lastConnected = 0;
                    
                    deviceDB.addDevice(newDevice);
                    Serial.println("✅ Auto-added new device to database!");
                    deviceDB.printAllDevices();
                }

                // Only attempt connection if not already connected or connecting
                if (!connected && !connecting) {
                    Serial.println("🔗 Attempting to connect to wrist band...");
                    if (pScan) {
                        pScan->stop();
                    }
                    connectToDevice(deviceAddress);
                } else if (connecting) {
                    Serial.println("⚠️ Already connecting, ignoring new device");
                } else if (connected) {
                    Serial.println("✅ Already connected to a device");
                }
            }
        } catch (...) {
            Serial.println("❌ Error in scan callback");
        }
    }
};

// ============================================================
//  CONNECT TO DEVICE
// ============================================================
void connectToDevice(String address) {
    if (connecting || connected) {
        return;
    }

    if (!hasSufficientMemory(4096)) {
        Serial.println("❌ Insufficient memory for connection");
        return;
    }

    PairedDevice* device = deviceDB.getDeviceByAddress(address);
    if (!device) {
        Serial.println("❌ Device not in database!");
        return;
    }

    connecting = true;
    connectStartTime = millis();
    connectAttempts++;
    currentDeviceAddress = address;
    currentDeviceUID = device->uniqueID;

    Serial.println("========================================");
    Serial.println("🔗 Connecting to: " + device->name);
    Serial.print("📱 MAC: ");
    Serial.println(address);
    Serial.print("🆔 UID: ");
    Serial.println(device->uniqueID);
    Serial.print("🔢 Attempt #");
    Serial.println(connectAttempts);
    Serial.println("========================================");

    if (pClient) {
        deleteClient(pClient);
        pClient = NULL;
        delay(100);
    }

    try {
        pClient = BLEDevice::createClient();
        if (pClient == NULL) {
            Serial.println("❌ Failed to create client!");
            connecting = false;
            return;
        }

        if (clientCallbacksInstance == nullptr) {
            clientCallbacksInstance = new ClientCallbacks();
        }
        pClient->setClientCallbacks(clientCallbacksInstance);

        BLEAddress bleAddress(address.c_str());

        // Try to connect
        if (pClient->connect(bleAddress)) {
            Serial.println("✅ Connected! Discovering services...");
            delay(200);

            BLERemoteService* pService = pClient->getService(SERVICE_UUID);
            if (pService) {
                Serial.println("✅ Service found");

                pIdentityChar = pService->getCharacteristic(IDENTITY_UUID);
                if (pIdentityChar) {
                    String deviceUID = readCharacteristicValue(pIdentityChar);
                    Serial.print("📱 Device UID: ");
                    Serial.println(deviceUID);
                    
                    // Store the UID for future reference
                    if (deviceUID.length() > 0) {
                        device->uniqueID = deviceUID;
                        deviceDB.saveDevices();
                        currentDeviceUID = deviceUID;
                        Serial.println("✅ Device UID stored");
                    }
                }

                pChar = pService->getCharacteristic(CHARACTERISTIC_UUID);
                if (pChar) {
                    Serial.println("✅ Characteristic found");

                    if (pChar->canNotify()) {
                        pChar->registerForNotify(notifyCallback);
                        Serial.println("✅ Subscribed to notifications");

                        // Request initial data
                        String value = readCharacteristicValue(pChar);
                        if (value.length() > 0) {
                            notifyCallback(pChar, (uint8_t*)value.c_str(), value.length(), false);
                        }

                        connected = true;
                        connecting = false;
                        deviceDB.updateConnection(address, 0);

                        Serial.println("========================================");
                        Serial.println("✅ SUCCESSFULLY CONNECTED TO WRIST BAND!");
                        Serial.println("========================================");
                        return;
                    }
                }
            }

            Serial.println("❌ Failed to discover services");
            pClient->disconnect();
            deleteClient(pClient);
            pClient = NULL;
            connecting = false;
        } else {
            Serial.println("❌ Connection failed");
            deleteClient(pClient);
            pClient = NULL;
            connecting = false;
        }
    } catch (const std::exception& e) {
        Serial.printf("❌ Exception: %s\n", e.what());
        connecting = false;
        if (pClient) {
            deleteClient(pClient);
            pClient = NULL;
        }
    } catch (...) {
        Serial.println("❌ Unknown exception during connection");
        connecting = false;
        if (pClient) {
            deleteClient(pClient);
            pClient = NULL;
        }
    }
}

// ============================================================
//  CONTINUOUS SCANNING
// ============================================================
void continuousScan() {
    if (!connected && !connecting) {
        try {
            if (pScan == NULL) {
                pScan = BLEDevice::getScan();
            }
            if (pScan == NULL) {
                Serial.println("❌ Failed to create scanner!");
                return;
            }

            if (scanCallbacksInstance == nullptr) {
                scanCallbacksInstance = new ScanCallbacks();
                pScan->setAdvertisedDeviceCallbacks(scanCallbacksInstance, false);
                pScan->setActiveScan(true);
                pScan->setInterval(100);
                pScan->setWindow(50);
            }

            pScan->start(SCAN_INTERVAL, false);
        } catch (...) {
            Serial.println("❌ Scan error");
            pScan = NULL;
        }
    }
}

// ============================================================
//  MANAGE DATA RECEPTION
// ============================================================
void manageDataReception() {
    if (connected && waitingForData && !dataReceived) {
        unsigned long elapsed = millis() - dataRetryStartTime;

        if (elapsed > DATA_RETRY_TIMEOUT) {
            Serial.println("⏰ DATA RECEPTION TIMEOUT (1 minute)");
            Serial.println("🔄 Re-requesting data...");

            if (pChar) {
                try {
                    String value = readCharacteristicValue(pChar);
                    if (value.length() > 0) {
                        notifyCallback(pChar, (uint8_t*)value.c_str(), value.length(), false);
                    }
                } catch (...) {
                    Serial.println("❌ Error reading data");
                }

                dataRetryStartTime = millis();
            } else {
                Serial.println("❌ Characteristic lost! Reconnecting...");
                if (pClient) {
                    pClient->disconnect();
                }
                connected = false;
                waitingForData = false;
            }
        }
    }
}

// ============================================================
//  MANAGE CONNECTION TIMEOUT
// ============================================================
void manageConnectionTimeout() {
    if (connecting) {
        unsigned long elapsed = millis() - connectStartTime;

        if (elapsed > CONNECTION_TIMEOUT) {
            Serial.println("========================================");
            Serial.println("⏰ CONNECTION TIMEOUT (1 minute)");
            Serial.print("📊 Attempts: ");
            Serial.println(connectAttempts);
            Serial.println("🔄 Retrying...");
            Serial.println("========================================");

            if (pClient) {
                try {
                    pClient->disconnect();
                } catch (...) {}
                deleteClient(pClient);
                pClient = NULL;
            }
            connecting = false;
            pChar = NULL;
            pIdentityChar = NULL;
        }
    }
}

// ============================================================
//  AUTO RECONNECT
// ============================================================
void autoReconnect() {
    if (!connected && !connecting) {
        unsigned long now = millis();

        if (now - lastReconnectAttempt > RECONNECT_DELAY) {
            Serial.println("🔄 Attempting to find wrist band...");
            
            // Force a scan to find devices
            if (pScan) {
                pScan->stop();
                delay(100);
                pScan->start(SCAN_INTERVAL, false);
            }

            lastReconnectAttempt = now;
        }
    }
}

// ============================================================
//  STATUS REPORTING
// ============================================================
void printStatus() {
    static unsigned long lastStatusTime = 0;

    if (millis() - lastStatusTime > 10000) {
        Serial.println("========================================");
        Serial.println("📊 SYSTEM STATUS");
        Serial.print("🔗 Connected: ");
        Serial.println(connected ? "YES" : "NO");
        Serial.print("🔄 Connecting: ");
        Serial.println(connecting ? "YES" : "NO");
        Serial.print("📊 Data Received: ");
        Serial.println(dataReceived ? "YES" : "NO");
        Serial.print("📱 Paired Devices: ");
        Serial.println(deviceDB.getAllDevices().size());
        Serial.print("🔢 Connect Attempts: ");
        Serial.println(connectAttempts);
        Serial.print("✅ Successful Connects: ");
        Serial.println(successfulConnects);
        Serial.print("📥 Data Attempts: ");
        Serial.println(dataReceptionAttempts);
        Serial.print("📤 Successful Data: ");
        Serial.println(successfulDataReceptions);
        printMemoryInfo();

        if (connected && pClient) {
            Serial.print("📱 Connected to: ");
            Serial.println(pClient->getPeerAddress().toString().c_str());
        } else {
            Serial.println("📱 Scanning for wrist band...");
        }
        Serial.println("========================================");
        lastStatusTime = millis();
    }
}

// ============================================================
//  COMMAND PROCESSOR
// ============================================================
void processSerialCommand() {
    if (!Serial.available()) return;

    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "list") {
        deviceDB.printAllDevices();
    }
    else if (command == "scan") {
        if (pScan) {
            pScan->stop();
            delay(100);
            pScan->start(SCAN_INTERVAL, false);
            Serial.println("🔄 Scanning started...");
        }
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
        Serial.println("🔌 Disconnected");
    }
    else if (command == "clear") {
        // Clear all devices from database
        auto devices = deviceDB.getAllDevices();
        for (auto& d : devices) {
            deviceDB.removeDevice(d.address);
        }
        Serial.println("🗑️ All devices cleared");
    }
    else if (command == "memory") {
        printMemoryInfo();
    }
    else if (command == "help") {
        Serial.println("========================================");
        Serial.println("📖 COMMANDS");
        Serial.println("========================================");
        Serial.println("list                    - List all paired devices");
        Serial.println("scan                    - Force a BLE scan");
        Serial.println("connect                 - Auto-connect to best device");
        Serial.println("connect <MAC>           - Connect to specific device");
        Serial.println("disconnect              - Disconnect current device");
        Serial.println("clear                   - Clear all paired devices");
        Serial.println("memory                  - Show memory info");
        Serial.println("help                    - Show this help");
        Serial.println("========================================");
    }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
    // ====== INCREASE SERIAL BUFFER ======
    Serial.setRxBufferSize(SERIAL_RX_BUFFER_SIZE);
    Serial.setTxBufferSize(SERIAL_TX_BUFFER_SIZE);
    Serial.begin(115200);

    // ====== WAIT FOR POWER TO STABILIZE ======
    delay(3000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("=== SILVERCARE WAIST BELT BLE CLIENT ===");
    Serial.println("=== (AUTO-DISCOVERY VERSION) ===");
    Serial.println("========================================");

    // ====== DISABLE WATCHDOG ======
    esp_err_t wdtDeinitResult = esp_task_wdt_deinit();
    if (wdtDeinitResult == ESP_OK) {
        Serial.println("🛡️ Hardware Task Watchdog: DISABLED");
    }

    // ====== PRINT MEMORY INFO ======
    printMemoryInfo();

    // ====== INITIALIZE ENCRYPTION ======
    if (!EncryptionManager::init()) {
        Serial.println("⚠️ Encryption init failed, continuing anyway...");
    }

    // ====== INITIALIZE BLE WITH RETRY ======
    int bleRetries = 3;
    while (bleRetries > 0 && !bleInitialized) {
        try {
            BLEDevice::init("Waist_Belt");
            BLEDevice::setMTU(BLE_MTU_SIZE);
            BLEDevice::setPower(ESP_PWR_LVL_P9);

            bleInitialized = true;
            Serial.print("📱 Waist MAC: ");
            Serial.println(BLEDevice::getAddress().toString().c_str());
            Serial.print("📊 BLE MTU: ");
            Serial.println(BLE_MTU_SIZE);
            break;
        } catch (...) {
            Serial.printf("❌ BLE init attempt %d failed\n", 4 - bleRetries);
            bleRetries--;
            delay(1000);
        }
    }

    if (!bleInitialized) {
        Serial.println("❌ BLE initialization failed after retries!");
        delay(5000);
        ESP.restart();
    }

    // ====== INITIALIZE DEVICE DATABASE ======
    deviceDB.init();

    // ====== PRINT CONFIGURATION ======
    Serial.println("========================================");
    Serial.println("🔐 AES-256-GCM Encryption: ENABLED");
    Serial.println("🔍 Scanning for: " + String(WRIST_NAME));
    Serial.println("🔄 Auto-discovery: ENABLED");
    Serial.println("📊 Serial Buffer: " + String(SERIAL_RX_BUFFER_SIZE) + " bytes");
    Serial.println("📊 BLE MTU: " + String(BLE_MTU_SIZE) + " bytes");
    Serial.println("========================================");
    Serial.println("💡 Type 'help' for commands");
    Serial.println("🔍 Scanning for wrist bands...");
    Serial.println("========================================");

    lastReconnectAttempt = millis() - RECONNECT_DELAY;
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
    // ====== WATCHDOG RESET PROTECTION ======
    unsigned long currentTime = millis();
    if (currentTime - lastLoopTime > WATCHDOG_TIMEOUT) {
        Serial.println("⚠️ Loop timeout detected! Resetting...");
        printMemoryInfo();
        delay(100);
        ESP.restart();
    }
    lastLoopTime = currentTime;

    // ====== PROCESS SERIAL COMMANDS ======
    processSerialCommand();

    // ====== CONTINUOUS SCANNING ======
    if (!connected && !connecting && bleInitialized) {
        continuousScan();
    }

    // ====== MANAGE TIMEOUTS ======
    manageConnectionTimeout();
    manageDataReception();
    autoReconnect();

    // ====== STATUS REPORTING ======
    printStatus();

    // ====== SMALL DELAY ======
    delay(50);
}