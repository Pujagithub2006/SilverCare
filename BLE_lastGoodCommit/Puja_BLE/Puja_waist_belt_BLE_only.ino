// ============================================================
//  CRITICAL FIXES FOR ARDUINO IDE - MUST BE FIRST
// ============================================================
#define ESP_TASK_BT_BT_STACK_SIZE 10240
#define ESP_TASK_BT_GATTS_STACK_SIZE 10240
#define ESP_TASK_BT_GATTC_STACK_SIZE 10240
#define ESP_TASK_BT_BLE_STACK_SIZE 10240

// ============================================================
//  INCLUDES - BLUETOOTH ONLY
// ============================================================
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Preferences.h>
#include <vector>
#include <algorithm>

// ============================================================
//  CONFIGURATION - WRIST BAND (device we connect OUT to)
// ============================================================
#define WRIST_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define WRIST_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define WRIST_IDENTITY_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define WRIST_MAC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ac"
#define WRIST_NAME "SilverCare_Wrist"

// ============================================================
//  CONFIGURATION - WAIST BELT'S OWN IDENTITY
// ============================================================
#define WAIST_SERVICE_UUID "5fafc201-1fb5-459e-8fcc-c5c9c331914c"
#define WAIST_IDENTITY_UUID "cbb5483e-36e1-4688-b7f5-ea07361b26aa"
#define WAIST_PAIR_UUID "cbb5483e-36e1-4688-b7f5-ea07361b26ab"
#define WAIST_MAC_UUID "cbb5483e-36e1-4688-b7f5-ea07361b26ac"
#define WAIST_NAME "SilverCare_Waist"

// ============================================================
//  BLE CONFIGURATION
// ============================================================
#define BLE_MTU_SIZE 247

// ============================================================
//  TIME CONFIGURATION
// ============================================================
const unsigned long CONNECTION_TIMEOUT = 60000;
const unsigned long DATA_RETRY_TIMEOUT = 60000;
const unsigned long SCAN_INTERVAL = 5;
const unsigned long RECONNECT_DELAY = 30000;
const unsigned long MAC_VERIFY_TIMEOUT = 10000;

// ============================================================
//  DEVICE STRUCTURE
// ============================================================
struct PairedDevice {
    String address;
    String name;
    String uniqueID;
    String macAddress;
    String password;
    int priority;
    bool autoConnect;
    bool isActive;
    unsigned long lastSeen;
    unsigned long lastConnected;
    int connectionCount;
    float avgRSSI;
    bool macVerified;

    String serialize() {
        return address + "|" + name + "|" + uniqueID + "|" + macAddress + "|" +
               String(priority) + "|" + String(autoConnect) + "|" +
               String(connectionCount) + "|" + password + "|" +
               String(lastSeen) + "|" + String(lastConnected) + "|" +
               String(macVerified);
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
            int pos9 = data.indexOf('|', pos8 + 1);
            int pos10 = data.indexOf('|', pos9 + 1);

            if (pos1 > 0 && pos2 > 0 && pos3 > 0 && pos4 > 0 && pos5 > 0 && pos6 > 0) {
                device.address = data.substring(0, pos1);
                device.name = data.substring(pos1 + 1, pos2);
                device.uniqueID = data.substring(pos2 + 1, pos3);
                device.macAddress = data.substring(pos3 + 1, pos4);
                device.priority = data.substring(pos4 + 1, pos5).toInt();
                device.autoConnect = data.substring(pos5 + 1, pos6) == "1";
                device.connectionCount = data.substring(pos6 + 1, pos7).toInt();
                device.password = data.substring(pos7 + 1, pos8);
                device.lastSeen = data.substring(pos8 + 1, pos9).toInt();
                device.lastConnected = data.substring(pos9 + 1, pos10).toInt();
                device.macVerified = data.substring(pos10 + 1).toInt() == 1;
            }
        } catch (...) {
            device.address = "";
            device.name = "";
            device.uniqueID = "";
            device.macAddress = "";
            device.priority = 0;
            device.autoConnect = false;
            device.connectionCount = 0;
            device.password = "";
            device.lastSeen = 0;
            device.lastConnected = 0;
            device.macVerified = false;
        }
        device.isActive = false;
        device.avgRSSI = 0;

        return device;
    }
};

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

    PairedDevice* getDeviceByMAC(String mac) {
        for (auto& d : devices) {
            if (d.macAddress == mac) return &d;
        }
        return nullptr;
    }

    std::vector<PairedDevice> getAutoConnectDevices() {
        std::vector<PairedDevice> result;
        for (auto& d : devices) {
            if (d.autoConnect && d.macVerified) result.push_back(d);
        }
        return result;
    }

    PairedDevice* getHighestPriorityDevice() {
        if (devices.empty()) return nullptr;
        PairedDevice* best = &devices[0];
        for (auto& d : devices) {
            if (d.priority > best->priority && d.macVerified) {
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
            Serial.printf("#%d: %s | MAC: %s | UID: %s | Priority: %d | Verified: %s\n",
                i+1, d.name.c_str(), d.macAddress.c_str(), d.uniqueID.c_str(), 
                d.priority, d.macVerified ? "YES" : "NO");
        }
        Serial.println("========================================");
    }
};

// ============================================================
//  BLE GLOBALS - CLIENT SIDE
// ============================================================
BLEClient* pClient = NULL;
BLERemoteCharacteristic* pChar = NULL;
BLERemoteCharacteristic* pIdentityChar = NULL;
BLERemoteCharacteristic* pWristMacChar = NULL;
BLEScan* pScan = NULL;

class ScanCallbacks;
class ClientCallbacks;
ScanCallbacks* scanCallbacksInstance = nullptr;
ClientCallbacks* clientCallbacksInstance = nullptr;

bool connected = false;
bool connecting = false;
bool dataReceived = false;
bool waitingForData = false;
bool macVerified = false;
String currentDeviceAddress = "";
String currentDeviceUID = "";
String currentDeviceMAC = "";
unsigned long connectStartTime = 0;
unsigned long dataRetryStartTime = 0;
unsigned long lastReconnectAttempt = 0;
unsigned long lastDataTime = 0;
unsigned long macVerifyStartTime = 0;
int connectAttempts = 0;
int successfulConnects = 0;
int dataReceptionAttempts = 0;
int successfulDataReceptions = 0;
bool bleInitialized = false;

DeviceDatabase deviceDB;

// ============================================================
//  BLE GLOBALS - SERVER SIDE
// ============================================================
Preferences idPrefs;
String myUniqueID = "";
String myMacAddress = "";

BLEServer* pWaistServer = NULL;
BLEService* pWaistService = NULL;
BLECharacteristic* pWaistIdentityChar = NULL;
BLECharacteristic* pWaistPairChar = NULL;
BLECharacteristic* pWaistMacChar = NULL;
BLEAdvertising* pWaistAdvertising = NULL;

bool wristCentralConnected = false;
bool serverLinkVerified = false;
bool wristMacVerified = false;

// ============================================================
//  FORWARD DECLARATIONS
// ============================================================
void connectToDevice(String address);
void continuousScan();
void deleteClient(BLEClient* client);
bool mutualPairingConfirmed();

// ============================================================
//  MUTUAL PAIRING STATUS
// ============================================================
bool mutualPairingConfirmed() {
    return connected && serverLinkVerified && macVerified && wristMacVerified;
}

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
        macVerified = false;
        macVerifyStartTime = millis();

        Serial.println("========================================");
        Serial.println("✅ [BLE] Connected to Wrist!");
        Serial.print("📱 Connected to: ");
        Serial.println(pClient->getPeerAddress().toString().c_str());
        Serial.println("🔄 Verifying MAC address...");
        Serial.println("========================================");
    }

    void onDisconnect(BLEClient* pClient) {
        connected = false;
        connecting = false;
        macVerified = false;

        Serial.println("❌ [BLE] Disconnected from Wrist");
        Serial.println("🔄 Will try to reconnect...");

        pClient = NULL;
        pChar = NULL;
        pIdentityChar = NULL;
        pWristMacChar = NULL;

        dataReceived = false;
        waitingForData = false;
        currentDeviceAddress = "";
        currentDeviceUID = "";
        currentDeviceMAC = "";
    }
};

// ============================================================
//  SERVER CALLBACKS
// ============================================================
class WaistServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* srv) {
        wristCentralConnected = true;
        wristMacVerified = false;
        Serial.println("========================================");
        Serial.println("✅ [BLE-SERVER] Wrist band connected to our identity service");
        Serial.println("   Waiting for MAC verification...");
        Serial.println("========================================");
    }

    void onDisconnect(BLEServer* srv) {
        wristCentralConnected = false;
        serverLinkVerified = false;
        wristMacVerified = false;
        Serial.println("❌ [BLE-SERVER] Wrist band disconnected from our identity service");
        if (pWaistAdvertising) {
            pWaistAdvertising->start();
        }
    }
};

// ============================================================
//  PAIR CHARACTERISTIC CALLBACKS
// ============================================================
class WaistPairCharCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* ch) {
        String value = String(ch->getValue().c_str());
        if (!value.startsWith("PAIR:")) return;

        String peerID = value.substring(5);
        int macSeparator = peerID.indexOf(':');
        String peerUID = peerID.substring(0, macSeparator);
        String peerMAC = peerID.substring(macSeparator + 1);

        Serial.println("📥 [BLE-SERVER] PAIR request from Device ID: " + peerUID);
        Serial.println("📥 [BLE-SERVER] Peer MAC Address: " + peerMAC);

        // Verify the device is in our paired database
        PairedDevice* known = deviceDB.getDeviceByUID(peerUID);
        if (known != nullptr) {
            // Verify MAC address matches
            if (known->macAddress == peerMAC) {
                serverLinkVerified = true;
                wristMacVerified = true;
                
                // Send ACK with our MAC address
                String ack = "PAIR_ACK:" + myUniqueID + ":" + myMacAddress;
                ch->setValue(ack.c_str());
                ch->notify();
                
                Serial.println("✅ [BLE-SERVER] MAC address verified for '" + known->name + "'");
                Serial.println("✅ [BLE-SERVER] PAIR acknowledged - reverse link verified");
                Serial.println("🤝 MUTUAL PAIRING COMPLETE!");
            } else {
                Serial.println("❌ [BLE-SERVER] MAC address mismatch!");
                Serial.println("   Expected: " + known->macAddress);
                Serial.println("   Received: " + peerMAC);
            }
        } else {
            Serial.println("❌ [BLE-SERVER] PAIR rejected - Device ID not in paired database");
            Serial.println("   Device ID: " + peerUID);
        }
    }
};

// ============================================================
//  MAC CHARACTERISTIC CALLBACKS (for wrist reading our MAC)
// ============================================================
class WaistMacCharCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic* ch) {
        Serial.println("📤 [BLE-SERVER] MAC address read by wrist band");
        // MAC is already set in the characteristic
    }
};

// ============================================================
//  NOTIFICATION CALLBACK
// ============================================================
void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* data, size_t len, bool isNotify) {
    if (len == 0 || data == NULL || len > 1024) return;

    if (!mutualPairingConfirmed()) {
        Serial.println("⏳ [BLE] Data received but mutual pairing not yet confirmed - ignoring");
        return;
    }

    dataReceptionAttempts++;

    try {
        String receivedData;
        receivedData.reserve(len + 1);
        receivedData = String((char*)data).substring(0, len);
        Serial.print("📥 [BLE] Data received: ");
        Serial.println(receivedData);
       
        Serial.println("========================================");
        Serial.println("✅ DATA RECEIVED SUCCESSFULLY!");
        Serial.print("📊 Data: ");
        Serial.println(receivedData);
        Serial.println("========================================");
       
        successfulDataReceptions++;
        dataReceived = true;
        waitingForData = false;
        lastDataTime = millis();
        dataRetryStartTime = millis();
    } catch (...) {
        Serial.println("❌ Error processing notification");
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

            if (deviceName == WRIST_NAME && !connected && !connecting) {
                Serial.println("📡 Found a SilverCare Wrist advertising - attempting connection");
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
//  VERIFY WRIST MAC ADDRESS
// ============================================================
bool verifyWristMAC() {
    if (pWristMacChar == NULL) {
        Serial.println("❌ [BLE] Wrist MAC characteristic not available");
        return false;
    }

    try {
        String wristMAC = readCharacteristicValue(pWristMacChar);
        if (wristMAC.length() == 0) {
            Serial.println("❌ [BLE] Could not read wrist MAC address");
            return false;
        }

        Serial.println("📥 [BLE] Wrist MAC Address: " + wristMAC);
        Serial.println("📥 [BLE] Our stored MAC for this device: " + currentDeviceMAC);

        if (wristMAC == currentDeviceMAC) {
            macVerified = true;
            Serial.println("✅ [BLE] Wrist MAC address verified!");
            return true;
        } else {
            Serial.println("❌ [BLE] Wrist MAC address mismatch!");
            return false;
        }
    } catch (...) {
        Serial.println("❌ [BLE] Error verifying wrist MAC");
        return false;
    }
}

// ============================================================
//  CONNECT TO DEVICE
// ============================================================
void connectToDevice(String address) {
    if (connecting || connected) {
        return;
    }

    connecting = true;
    connectStartTime = millis();
    connectAttempts++;
    currentDeviceAddress = address;
    currentDeviceUID = "";
    currentDeviceMAC = "";
    macVerified = false;

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

        if (clientCallbacksInstance == nullptr) {
            clientCallbacksInstance = new ClientCallbacks();
        }
        pClient->setClientCallbacks(clientCallbacksInstance);

        BLEAddress bleAddress(address.c_str());

        if (pClient->connect(bleAddress)) {
            Serial.println("✅ Connected! Discovering services...");
            delay(200);

            BLERemoteService* pService = pClient->getService(WRIST_SERVICE_UUID);
            if (pService) {
                Serial.println("✅ Service found");

                pIdentityChar = pService->getCharacteristic(WRIST_IDENTITY_UUID);
                if (pIdentityChar) {
                    String deviceUID = readCharacteristicValue(pIdentityChar);

                    if (deviceUID.length() == 0) {
                        Serial.println("❌ Could not read Device ID from identity characteristic!");
                        pClient->disconnect();
                        connecting = false;
                        return;
                    }

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

                    // Get MAC address from wrist
                    pWristMacChar = pService->getCharacteristic(WRIST_MAC_UUID);
                    if (pWristMacChar) {
                        String wristMAC = readCharacteristicValue(pWristMacChar);
                        if (wristMAC.length() > 0) {
                            Serial.println("📥 [BLE] Wrist MAC: " + wristMAC);
                            currentDeviceMAC = wristMAC;
                            
                            // Verify MAC matches stored
                            if (paired->macAddress == wristMAC) {
                                macVerified = true;
                                Serial.println("✅ [BLE] Wrist MAC verified!");
                            } else {
                                Serial.println("❌ [BLE] MAC mismatch! Expected: " + 
                                              paired->macAddress + " Got: " + wristMAC);
                                pClient->disconnect();
                                connecting = false;
                                return;
                            }
                        }
                    }

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

                pChar = pService->getCharacteristic(WRIST_CHARACTERISTIC_UUID);
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
                        Serial.println("✅ SUCCESSFULLY CONNECTED! (client-side link verified)");
                        Serial.println("   MAC Verification: " + String(macVerified ? "PASSED" : "PENDING"));
                        Serial.println("   Waiting for wrist band to complete reverse link...");
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
            pWristMacChar = NULL;
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
                    if (device.address != currentDeviceAddress && device.macVerified) {
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
        Serial.print("🔗 Connected (client -> wrist server): ");
        Serial.println(connected ? "YES" : "NO");
        Serial.print("🔄 Connecting: ");
        Serial.println(connecting ? "YES" : "NO");
        Serial.print("🔁 Reverse link (wrist -> our server): ");
        Serial.println(wristCentralConnected ? (serverLinkVerified ? "VERIFIED" : "CONNECTED (unverified)") : "NOT CONNECTED");
        Serial.print("✅ MAC Verified (wrist -> us): ");
        Serial.println(wristMacVerified ? "YES" : "NO");
        Serial.print("✅ MAC Verified (us -> wrist): ");
        Serial.println(macVerified ? "YES" : "NO");
        Serial.print("🤝 Mutual Pairing: ");
        Serial.println(mutualPairingConfirmed() ? "CONFIRMED - data accepted" : "WAITING");
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
    device1.uniqueID = "SCW_001";
    device1.macAddress = "AA:BB:CC:DD:EE:01";
    device1.priority = 10;
    device1.autoConnect = true;
    device1.connectionCount = 0;
    device1.password = "SC2024_001";
    device1.lastSeen = 0;
    device1.lastConnected = 0;
    device1.macVerified = true;
    deviceDB.addDevice(device1);

    PairedDevice device2;
    device2.address = "AA:BB:CC:DD:EE:02";
    device2.name = "SilverCare_Wrist_02";
    device2.uniqueID = "SCW_002";
    device2.macAddress = "AA:BB:CC:DD:EE:02";
    device2.priority = 8;
    device2.autoConnect = true;
    device2.connectionCount = 0;
    device2.password = "SC2024_002";
    device2.lastSeen = 0;
    device2.lastConnected = 0;
    device2.macVerified = true;
    deviceDB.addDevice(device2);
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
        macVerified = false;
        Serial.println("🔌 Disconnected");
    }
    else if (command.startsWith("add ")) {
        String data = command.substring(4);
        PairedDevice device;

        int macStart = data.indexOf("MAC:") + 4;
        int macEnd = data.indexOf(',', macStart);
        if (macStart > 4 && macEnd > macStart) {
            device.address = data.substring(macStart, macEnd);
            device.macAddress = data.substring(macStart, macEnd);
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
        device.macVerified = true;

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
            macVerified = false;
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
    else if (command == "pairstatus") {
        Serial.println("🤝 Mutual pairing: " + String(mutualPairingConfirmed() ? "CONFIRMED" : "NOT YET"));
        Serial.println("   Client link (us -> wrist): " + String(connected ? "UP" : "DOWN"));
        Serial.println("   Server link (wrist -> us): " + String(serverLinkVerified ? "VERIFIED" : "NOT VERIFIED"));
        Serial.println("   MAC Verified (us -> wrist): " + String(macVerified ? "YES" : "NO"));
        Serial.println("   MAC Verified (wrist -> us): " + String(wristMacVerified ? "YES" : "NO"));
        Serial.println("   Our Device ID: " + myUniqueID);
        Serial.println("   Our MAC: " + myMacAddress);
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
        Serial.println("pairstatus              - Show mutual pairing status");
        Serial.println("help                    - Show this help");
        Serial.println("========================================");
    }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(3000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("=== SILVERCARE WAIST BELT (MAC VERIFIED) ===");
    Serial.println("========================================");

    int bleRetries = 3;
    while (bleRetries > 0 && !bleInitialized) {
        try {
            BLEDevice::init("");
            BLEDevice::setMTU(BLE_MTU_SIZE);
            BLEDevice::setPower(ESP_PWR_LVL_P9);

            bleInitialized = true;
            myMacAddress = BLEDevice::getAddress().toString().c_str();
            Serial.print("📱 Waist MAC: ");
            Serial.println(myMacAddress);
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

    deviceDB.init();

    if (deviceDB.getAllDevices().size() == 0) {
        addDemoDevices();
    }

    deviceDB.printAllDevices();

    bool idPrefsOpened = idPrefs.begin("waist_id", false);
    if (!idPrefsOpened) {
        idPrefs.end();
        idPrefs.begin("waist_id", true);
        idPrefs.clear();
        idPrefs.end();
        idPrefsOpened = idPrefs.begin("waist_id", false);
        Serial.println(idPrefsOpened ? "🔄 Waist ID preferences reset" : "❌ Waist ID preferences still unavailable");
    }
    myUniqueID = idPrefs.getString("unique_id", "");
    if (myUniqueID.length() == 0) {
        myUniqueID = "SCB_" + String(millis(), HEX);
        idPrefs.putString("unique_id", myUniqueID);
    }
    idPrefs.end();
    Serial.println("🆔 Waist Belt Device ID: " + myUniqueID);

    // START OUR OWN GATT SERVER
    try {
        pWaistServer = BLEDevice::createServer();
        pWaistServer->setCallbacks(new WaistServerCallbacks());

        pWaistService = pWaistServer->createService(WAIST_SERVICE_UUID);

        pWaistIdentityChar = pWaistService->createCharacteristic(
            WAIST_IDENTITY_UUID,
            BLECharacteristic::PROPERTY_READ
        );
        pWaistIdentityChar->setValue(myUniqueID.c_str());

        // MAC Address characteristic - so wrist can read our MAC
        pWaistMacChar = pWaistService->createCharacteristic(
            WAIST_MAC_UUID,
            BLECharacteristic::PROPERTY_READ
        );
        pWaistMacChar->setValue(myMacAddress.c_str());
        pWaistMacChar->setCallbacks(new WaistMacCharCallbacks());

        pWaistPairChar = pWaistService->createCharacteristic(
            WAIST_PAIR_UUID,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_NOTIFY
        );
        pWaistPairChar->setCallbacks(new WaistPairCharCallbacks());
        pWaistPairChar->addDescriptor(new BLE2902());

        pWaistService->start();

        pWaistAdvertising = BLEDevice::getAdvertising();
        pWaistAdvertising->addServiceUUID(WAIST_SERVICE_UUID);
        pWaistAdvertising->setScanResponse(true);

        BLEAdvertisementData waistAdvData;
        waistAdvData.setName(WAIST_NAME);
        pWaistAdvertising->setAdvertisementData(waistAdvData);

        pWaistAdvertising->start();
        Serial.println("📡 Waist Belt now advertising as: " + String(WAIST_NAME));
        Serial.println("📡 Advertising MAC: " + myMacAddress);
    } catch (...) {
        Serial.println("❌ Failed to start Waist Belt's own GATT server/advertising");
    }

    Serial.println("========================================");
    Serial.println("📡 Continuous Scanning: ENABLED");
    Serial.println("📡 Own Advertising (reverse link): ENABLED");
    Serial.println("🔐 MAC Address Verification: REQUIRED");
    Serial.println("🤝 Mutual Pairing Required Before Data Accepted: YES");
    Serial.println("📚 Multi-Device Support: ENABLED");
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
    processSerialCommand();

    if (!connected && !connecting && bleInitialized) {
        continuousScan();
    }

    manageConnectionTimeout();
    manageDataReception();
    autoReconnect();

    printStatus();

    delay(50);
}