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
// NOTE: WiFi.h removed - it was included but never used anywhere in this
// sketch. Dropping it frees a bit of heap/flash that BLE + mbedTLS need.
// If you add WiFi functionality later, just re-add: #include <WiFi.h>

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
#define WRIST_NAME "SilverCare_Wrist"

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
const unsigned long RECONNECT_DELAY = 30000;
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
        // NOTE: try/catch left in place for structural parity with the
        // original sketch, but be aware Arduino String parsing calls here
        // (indexOf/substring/toInt) do not throw C++ exceptions by default
        // on ESP32 - the real safety net is the index bounds-check below.
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

        // CRITICAL FIX: Reinitialize for each operation
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

        // CRITICAL FIX: Reinitialize for each operation
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

        // FIX: preferences.begin() returns a bool indicating success/failure.
        // The original code discarded that return value, so a corrupted NVS
        // partition (which can happen after a few crash-reset cycles) would
        // silently "succeed" with an unusable preferences handle, and the
        // catch(...) block below never fires because begin() does not throw.
        // We now explicitly check the return value and reset the namespace
        // if it fails, same recovery behavior as before but actually reliable.
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

// FIX: ScanCallbacks and ClientCallbacks are now allocated ONCE and reused,
// instead of being `new`'d every time connectToDevice()/continuousScan()
// runs. The original code called
//     pScan->setAdvertisedDeviceCallbacks(new ScanCallbacks(), false);
// on every single loop() iteration while not connected/connecting - which is
// most of the time. Each call leaked one ScanCallbacks object (never freed),
// and ClientCallbacks leaked one object per connection attempt. This is an
// unbounded heap leak. On ESP32 Arduino, C++ exceptions are disabled by
// default, so a failed `new` from heap exhaustion does not throw and get
// caught by the try/catch blocks - it calls abort(), which is exactly the
// kind of silent reset-loop you were seeing (ROM banner repeating, nothing
// from setup() ever printing). Reusing single static instances fixes this.
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
//  SCAN CALLBACKS
// ============================================================
class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        try {
            String deviceName = advertisedDevice.getName().c_str();
            String deviceAddress = advertisedDevice.getAddress().toString().c_str();
            int rssi = advertisedDevice.getRSSI();

            if (deviceName.length() > 0 && deviceName.length() < 64) {
                Serial.print("📱 Found: ");
                Serial.print(deviceName);
                Serial.print(" @ ");
                Serial.print(deviceAddress);
                Serial.print(" (RSSI: ");
                Serial.print(rssi);
                Serial.println(" dBm)");
            }

            // FIX (Unique Device ID pairing): we no longer require the MAC
            // to already match a database entry before attempting a
            // connection. BLE peripherals can use random/private addresses
            // that rotate over time, so a MAC-only pre-check would cause a
            // legitimately paired wrist band to stop being recognized the
            // moment its address changes. Instead we connect to ANY device
            // advertising our expected name, and let connectToDevice()
            // verify (and self-heal) identity via the Identity
            // characteristic's stable Device ID after the connection is up.
            if (deviceName == WRIST_NAME && !connected && !connecting) {
                Serial.println("📡 Found a SilverCare Wrist advertising - attempting connection to verify identity");
                if (pScan) {
                    pScan->stop();
                }
                connectToDevice(deviceAddress);
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

    // FIX (Unique Device ID pairing): we intentionally do NOT require a
    // pre-existing database entry keyed by MAC before connecting. The device
    // is identified authoritatively by the stable Device ID read from the
    // Identity characteristic AFTER the BLE connection is established (see
    // below). This lets a paired wrist band still be recognized even if its
    // BLE address has changed since it was first paired.
    connecting = true;
    connectStartTime = millis();
    connectAttempts++;
    currentDeviceAddress = address;
    currentDeviceUID = ""; // not known yet - resolved after identity read below

    Serial.println("========================================");
    Serial.println("🔗 Connecting to advertised address: " + address);
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

        // FIX: reuse a single ClientCallbacks instance instead of `new`-ing
        // one on every connection attempt (was leaking memory each retry).
        if (clientCallbacksInstance == nullptr) {
            clientCallbacksInstance = new ClientCallbacks();
        }
        pClient->setClientCallbacks(clientCallbacksInstance);

        BLEAddress bleAddress(address.c_str());

        if (pClient->connect(bleAddress)) {
            Serial.println("✅ Connected! Discovering services...");
            delay(200);

            BLERemoteService* pService = pClient->getService(SERVICE_UUID);
            if (pService) {
                Serial.println("✅ Service found");

                pIdentityChar = pService->getCharacteristic(IDENTITY_UUID);
                if (pIdentityChar) {
                    String deviceUID = readCharacteristicValue(pIdentityChar);

                    if (deviceUID.length() == 0) {
                        Serial.println("❌ Could not read Device ID from identity characteristic!");
                        pClient->disconnect();
                        connecting = false;
                        return;
                    }

                    // FIX (Unique Device ID pairing): resolve which paired
                    // device this is primarily by its stable Device ID, not
                    // by MAC. Fall back to a MAC match only for
                    // backward-compatibility with entries added manually
                    // (e.g. via the "add" serial command) before their real
                    // Device ID was ever learned.
                    PairedDevice* paired = deviceDB.getDeviceByUID(deviceUID);
                    if (!paired) {
                        paired = deviceDB.getDeviceByAddress(address);
                    }

                    if (!paired) {
                        Serial.println("❌ Unknown device - Device ID '" + deviceUID +
                                        "' is not in the paired device database. Rejecting.");
                        pClient->disconnect();
                        connecting = false;
                        return;
                    }

                    // Self-heal: keep the stored record's MAC and Device ID
                    // in sync with what we just observed, so future scans
                    // and reconnects work even if the BLE address rotated.
                    bool recordChanged = false;
                    if (paired->address != address) {
                        Serial.println("🔄 MAC address changed for '" + paired->name +
                                        "' (" + paired->address + " -> " + address + ") - updating record");
                        paired->address = address;
                        recordChanged = true;
                    }
                    if (paired->uniqueID != deviceUID) {
                        paired->uniqueID = deviceUID;
                        recordChanged = true;
                    }
                    if (recordChanged) {
                        deviceDB.saveDevices();
                    }

                    currentDeviceUID = paired->uniqueID;
                    currentDeviceAddress = paired->address;

                    Serial.println("✅ Device identity verified: " + paired->name +
                                    " (ID: " + paired->uniqueID + ")");
                }

                pChar = pService->getCharacteristic(CHARACTERISTIC_UUID);
                if (pChar) {
                    Serial.println("✅ Characteristic found");

                    if (pChar->canNotify()) {
                        pChar->registerForNotify(notifyCallback);
                        Serial.println("✅ Subscribed to notifications");

                        String value = readCharacteristicValue(pChar);
                        if (value.length() > 0) {
                            notifyCallback(pChar, (uint8_t*)value.c_str(), value.length(), false);
                        }

                        connected = true;
                        connecting = false;
                        deviceDB.updateConnection(address, 0);

                        Serial.println("========================================");
                        Serial.println("✅ SUCCESSFULLY CONNECTED!");
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

            // FIX: reuse a single ScanCallbacks instance instead of `new`-ing
            // a fresh one on every loop() call. This was the main cause of
            // the unbounded heap leak / crash-reset loop: this function runs
            // constantly while idle-scanning (i.e. most of the time), so the
            // old code leaked one ScanCallbacks object per loop iteration.
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
            Serial.println("🔄 Attempting to reconnect...");

            std::vector<PairedDevice> autoDevices = deviceDB.getAutoConnectDevices();

            if (autoDevices.size() > 0) {
                for (auto& device : autoDevices) {
                    if (device.address != currentDeviceAddress) {
                        connectToDevice(device.address);
                        if (connecting) break;
                    }
                }
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
        }
        Serial.println("========================================");
        lastStatusTime = millis();
    }
}

// ============================================================
//  ADD DEMO DEVICES
// ============================================================
void addDemoDevices() {
    Serial.println("📝 Adding demo devices...");

    PairedDevice device1;
    device1.address = "AA:BB:CC:DD:EE:01";
    device1.name = "SilverCare_Wrist_01";
    device1.uniqueID = "UID_001";
    device1.priority = 10;
    device1.autoConnect = true;
    device1.connectionCount = 0;
    device1.password = "SC2024_001";
    device1.lastSeen = 0;
    device1.lastConnected = 0;
    deviceDB.addDevice(device1);

    PairedDevice device2;
    device2.address = "AA:BB:CC:DD:EE:02";
    device2.name = "SilverCare_Wrist_02";
    device2.uniqueID = "UID_002";
    device2.priority = 8;
    device2.autoConnect = true;
    device2.connectionCount = 0;
    device2.password = "SC2024_002";
    device2.lastSeen = 0;
    device2.lastConnected = 0;
    deviceDB.addDevice(device2);

    PairedDevice device3;
    device3.address = "AA:BB:CC:DD:EE:03";
    device3.name = "SilverCare_Wrist_03";
    device3.uniqueID = "UID_003";
    device3.priority = 5;
    device3.autoConnect = false;
    device3.connectionCount = 0;
    device3.password = "SC2024_003";
    device3.lastSeen = 0;
    device3.lastConnected = 0;
    deviceDB.addDevice(device3);
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
    else if (command.startsWith("add ")) {
        String data = command.substring(4);
        PairedDevice device;

        int macStart = data.indexOf("MAC:") + 4;
        int macEnd = data.indexOf(',', macStart);
        if (macStart > 4 && macEnd > macStart) {
            device.address = data.substring(macStart, macEnd);
        }

        int nameStart = data.indexOf("Name:") + 5;
        int nameEnd = data.indexOf(',', nameStart);
        if (nameStart > 5 && nameEnd > nameStart) {
            device.name = data.substring(nameStart, nameEnd);
        }

        int uidStart = data.indexOf("UID:") + 4;
        int uidEnd = data.indexOf(',', uidStart);
        if (uidStart > 4 && uidEnd > uidStart) {
            device.uniqueID = data.substring(uidStart, uidEnd);
        }

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
        device.lastSeen = 0;
        device.lastConnected = 0;

        if (device.address.length() > 0 && device.uniqueID.length() > 0) {
            deviceDB.addDevice(device);
            Serial.println("✅ Device added successfully!");
        } else {
            Serial.println("❌ Invalid device data!");
        }
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
    else if (command.startsWith("priority ")) {
        String address = command.substring(9, command.indexOf(' ', 9));
        int priority = command.substring(command.lastIndexOf(' ') + 1).toInt();
        PairedDevice* device = deviceDB.getDeviceByAddress(address);
        if (device) {
            device->priority = priority;
            deviceDB.saveDevices();
            Serial.println("✅ Priority updated to " + String(priority));
        } else {
            Serial.println("❌ Device not found");
        }
    }
    else if (command == "memory") {
        printMemoryInfo();
    }
    else if (command == "help") {
        Serial.println("========================================");
        Serial.println("📖 COMMANDS");
        Serial.println("========================================");
        Serial.println("list                    - List all paired devices");
        Serial.println("connect                 - Auto-connect to best device");
        Serial.println("connect <MAC>           - Connect to specific device");
        Serial.println("disconnect              - Disconnect current device");
        Serial.println("add MAC:XX,Name:YY,...  - Add new device");
        Serial.println("remove <MAC>            - Remove device");
        Serial.println("priority <MAC> <num>    - Set device priority (0-10)");
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
    Serial.println("=== (Arduino IDE - FIXED VERSION) ===");
    Serial.println("========================================");

    // ====== DISABLE WATCHDOG ======
    // FIX: The original code called esp_task_wdt_init() to *reconfigure* the
    // hardware Task Watchdog (TWDT) that the ESP32 Arduino core (3.x / IDF 5.x)
    // already auto-initializes and auto-subscribes the loop task to at boot.
    // Re-initializing it here drops that existing subscription, so the
    // framework's internal watchdog-feed hook (and/or the BLE stack's task)
    // keeps calling esp_task_wdt_reset() against a now-orphaned task handle,
    // producing the repeating "esp_task_wdt_reset(): task not found" spam.
    //
    // Since this sketch already implements its own software watchdog
    // (see lastLoopTime / WATCHDOG_TIMEOUT in loop()), we don't need the
    // hardware TWDT at all - we just deinit it outright instead of
    // reconfiguring it. This also removes the old disableCore0WDT()/
    // disableCore1WDT() calls, which are legacy pre-IDF4 APIs that don't
    // apply to the TWDT on newer cores and were redundant here anyway.
    esp_err_t wdtDeinitResult = esp_task_wdt_deinit();
    if (wdtDeinitResult == ESP_OK) {
        Serial.println("🛡️ Hardware Task Watchdog: DISABLED (using software watchdog instead)");
    } else {
        Serial.println("⚠️ Task Watchdog deinit returned non-OK (may not have been active) - continuing");
    }

    // ====== PRINT MEMORY INFO ======
    printMemoryInfo();

    // ====== INITIALIZE ENCRYPTION (FIXED) ======
    if (!EncryptionManager::init()) {
        Serial.println("⚠️ Encryption init failed, continuing anyway...");
    }

    // ====== INITIALIZE BLE WITH RETRY ======
    int bleRetries = 3;
    while (bleRetries > 0 && !bleInitialized) {
        try {
            BLEDevice::init("");
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

    // ====== ADD DEMO DEVICES IF NONE EXIST ======
    if (deviceDB.getAllDevices().size() == 0) {
        addDemoDevices();
    }

    deviceDB.printAllDevices();

    // ====== PRINT CONFIGURATION ======
    Serial.println("========================================");
    Serial.println("🔐 AES-256-GCM Encryption: ENABLED (FIXED)");
    Serial.println("⏱️ Connection Timeout: 1 MINUTE");
    Serial.println("⏱️ Data Reception Timeout: 1 MINUTE");
    Serial.println("🔄 Continuous Scanning: ENABLED");
    Serial.println("📚 Multi-Device Support: ENABLED");
    Serial.println("📊 Serial Buffer: " + String(SERIAL_RX_BUFFER_SIZE) + " bytes");
    Serial.println("📊 BLE MTU: " + String(BLE_MTU_SIZE) + " bytes");
    Serial.println("========================================");
    Serial.println("💡 Type 'help' for commands");
    Serial.println("🔄 Starting continuous scan...");
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

    // ====== CONTINUOUS SCANNING (NO GAPS) ======
    if (!connected && !connecting && bleInitialized) {
        continuousScan();
    }

    // ====== MANAGE TIMEOUTS ======
    manageConnectionTimeout();
    manageDataReception();
    autoReconnect();

    // ====== STATUS REPORTING ======
    printStatus();

    // ====== SMALL DELAY TO PREVENT WATCHDOG ======
    delay(50);
}
