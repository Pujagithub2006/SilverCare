// =========================================================================
//  SILVERCARE - WAIST BELT BLE TEST (NimBLE Client)
//  Purpose: Test BLE connectivity between Waist and Wrist
// =========================================================================

#include <NimBLEDevice.h>
#include <NimBLEClient.h>
#include <NimBLEScan.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <string>

// =========================================================================
//  BLE CONFIGURATION - Must match Wrist band
// =========================================================================
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define IDENTITY_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define WRIST_NAME          "SilverCare_Wrist"

// =========================================================================
//  TIME CONFIGURATION
// =========================================================================
const unsigned long SCAN_INTERVAL = 3;        // Scan duration in seconds
const unsigned long RECONNECT_DELAY = 5000;   // Delay between reconnect attempts
const unsigned long WATCHDOG_TIMEOUT = 5000;  // Watchdog timeout

// =========================================================================
//  BLE GLOBALS
// =========================================================================
NimBLEClient* pClient = NULL;
NimBLERemoteCharacteristic* pChar = NULL;
NimBLERemoteCharacteristic* pIdentityChar = NULL;
NimBLEScan* pScan = NULL;

bool connected = false;
bool connecting = false;
bool dataReceived = false;

String currentDeviceAddress = "";
String currentDeviceUID = "";

unsigned long connectStartTime = 0;
unsigned long lastReconnectAttempt = 0;
int connectAttempts = 0;
int successfulConnects = 0;
unsigned long lastLoopTime = 0;
bool bleInitialized = false;

// =========================================================================
//  ENCRYPTION MANAGER (Simplified - matches Wrist)
// =========================================================================
class EncryptionManager {
public:
    static String encrypt(const String& plaintext) {
        if (plaintext.length() == 0 || plaintext.length() > 50) return plaintext;
        String result = "";
        result.reserve(plaintext.length() * 2 + 1);
        for (int i = 0; i < plaintext.length(); i++) {
            char c = plaintext[i] ^ 0x55;
            if (c < 16) result += "0";
            result += String(c, HEX);
        }
        return result;
    }
    
    static String decrypt(const String& encrypted) {
        if (encrypted.length() == 0 || encrypted.length() % 2 != 0) return encrypted;
        String result = "";
        result.reserve(encrypted.length() / 2);
        for (int i = 0; i < encrypted.length(); i += 2) {
            String byteStr = encrypted.substring(i, i + 2);
            char c = strtol(byteStr.c_str(), NULL, 16);
            result += (char)(c ^ 0x55);
        }
        return result;
    }
};

// =========================================================================
//  FORWARD DECLARATIONS
// =========================================================================
void connectToDevice(String address);
void processReceivedData(String encryptedData);
void continuousScan();
void deleteClient(NimBLEClient* client);

// =========================================================================
//  CLIENT CALLBACKS
// =========================================================================
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) {
        connected = true;
        connecting = false;
        successfulConnects++;
        
        Serial.println("========================================");
        Serial.println("✅ [BLE] Connected to Wrist Band!");
        Serial.print("📱 Connected to: ");
        Serial.println(pClient->getPeerAddress().toString().c_str());
        Serial.println("========================================");
        
        String address = pClient->getPeerAddress().toString().c_str();
        currentDeviceAddress = address;
    }

    void onDisconnect(NimBLEClient* pClient, int reason) {
        connected = false;
        connecting = false;
        
        Serial.println("========================================");
        Serial.println("❌ [BLE] Disconnected from Wrist");
        Serial.print("📱 Reason: ");
        Serial.println(reason);
        Serial.println("🔄 Will try to reconnect...");
        Serial.println("========================================");
        
        pChar = NULL;
        pIdentityChar = NULL;
        currentDeviceUID = "";
        dataReceived = false;
    }
};

// =========================================================================
//  NOTIFICATION CALLBACK
// =========================================================================
void notifyCallback(NimBLERemoteCharacteristic* pChar, uint8_t* data, size_t len, bool isNotify) {
    if (len == 0 || data == NULL || len > 1024) return;
    
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

// =========================================================================
//  PROCESS RECEIVED DATA
// =========================================================================
void processReceivedData(String encryptedData) {
    try {
        String decryptedData = EncryptionManager::decrypt(encryptedData);
        
        if (decryptedData.length() == 0) {
            Serial.println("❌ Decryption failed!");
            return;
        }
        
        Serial.print("🔓 Decrypted: ");
        Serial.println(decryptedData);
        
        // Parse the data (HR:SPO2:IR:Temp)
        int firstColon = decryptedData.indexOf(':');
        int secondColon = decryptedData.indexOf(':', firstColon + 1);
        int thirdColon = decryptedData.indexOf(':', secondColon + 1);
        
        if (firstColon > 0 && secondColon > 0 && thirdColon > 0) {
            float hr = decryptedData.substring(0, firstColon).toFloat();
            float spo2 = decryptedData.substring(firstColon + 1, secondColon).toFloat();
            long ir = decryptedData.substring(secondColon + 1, thirdColon).toFloat();
            float temp = decryptedData.substring(thirdColon + 1).toFloat();
            
            dataReceived = true;
            
            Serial.println("========================================");
            Serial.println("✅ DATA RECEIVED SUCCESSFULLY!");
            Serial.print("❤️ HR: ");
            Serial.print(hr);
            Serial.print(" BPM | SpO2: ");
            Serial.print(spo2);
            Serial.print("% | IR: ");
            Serial.print(ir);
            Serial.print(" | Temp: ");
            Serial.print(temp);
            Serial.println("°C");
            Serial.println("========================================");
        } else {
            Serial.println("❌ Invalid data format!");
        }
    } catch (...) {
        Serial.println("❌ Error processing data");
    }
}

// =========================================================================
//  SCAN CALLBACKS
// =========================================================================
class ScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        try {
            String deviceName = advertisedDevice->getName().c_str();
            String deviceAddress = advertisedDevice->getAddress().toString().c_str();
            int rssi = advertisedDevice->getRSSI();
            
            if (deviceName.length() > 0) {
                Serial.print("📱 Found: ");
                Serial.print(deviceName);
                Serial.print(" @ ");
                Serial.print(deviceAddress);
                Serial.print(" (RSSI: ");
                Serial.print(rssi);
                Serial.println(" dBm)");
            }
            
            // Check if this is our Wrist band
            if (deviceName == WRIST_NAME && !connected && !connecting) {
                Serial.println("========================================");
                Serial.println("🎯 FOUND WRIST BAND!");
                Serial.println("📱 Name: " + deviceName);
                Serial.println("📱 Address: " + deviceAddress);
                Serial.println("========================================");
                
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

// =========================================================================
//  CONNECT TO DEVICE - FIXED VERSION
// =========================================================================
void connectToDevice(String address) {
    if (connecting || connected) {
        return;
    }
    
    connecting = true;
    connectStartTime = millis();
    connectAttempts++;
    currentDeviceAddress = address;
    
    Serial.println("========================================");
    Serial.println("🔗 Connecting to: " + address);
    Serial.print("🔢 Attempt #");
    Serial.println(connectAttempts);
    Serial.println("========================================");
    
    if (pClient) {
        deleteClient(pClient);
        pClient = NULL;
        delay(100);
    }
    
    try {
        pClient = NimBLEDevice::createClient();
        if (pClient == NULL) {
            Serial.println("❌ Failed to create client!");
            connecting = false;
            return;
        }
        
        pClient->setClientCallbacks(new ClientCallbacks(), false);
        
        // Convert String to NimBLEAddress - use std::string
        std::string addrStr = address.c_str();
        NimBLEAddress bleAddress(addrStr, BLE_ADDR_PUBLIC);
        
        if (pClient->connect(bleAddress, true)) {
            Serial.println("✅ Connected! Discovering services...");
            delay(200);
            
            NimBLERemoteService* pService = pClient->getService(SERVICE_UUID);
            if (pService) {
                Serial.println("✅ Service found");
                
                // Read identity
                pIdentityChar = pService->getCharacteristic(IDENTITY_UUID);
                if (pIdentityChar) {
                    String deviceUID = pIdentityChar->readValue().c_str();
                    currentDeviceUID = deviceUID;
                    Serial.println("🆔 Device UID: " + deviceUID);
                }
                
                // Get data characteristic
                pChar = pService->getCharacteristic(CHARACTERISTIC_UUID);
                if (pChar) {
                    Serial.println("✅ Characteristic found");
                    
                    if (pChar->canNotify()) {
                        pChar->subscribe(true, notifyCallback);
                        Serial.println("✅ Subscribed to notifications");
                        
                        // Send pairing request
                        String pairMsg = "PAIR:WAIST_" + String(millis(), HEX);
                        pChar->writeValue(pairMsg.c_str());
                        Serial.println("📤 Pairing request sent: " + pairMsg);
                        
                        // Send PING to test communication
                        String pingMsg = "PING:TEST_" + String(millis(), HEX);
                        pChar->writeValue(pingMsg.c_str());
                        Serial.println("📤 PING sent: " + pingMsg);
                        
                        connected = true;
                        connecting = false;
                        
                        Serial.println("========================================");
                        Serial.println("✅ SUCCESSFULLY CONNECTED TO WRIST BAND!");
                        Serial.println("📱 UID: " + currentDeviceUID);
                        Serial.println("========================================");
                        return;
                    }
                }
            }
            
            Serial.println("❌ Failed to discover services");
            pClient->disconnect();
            connecting = false;
        } else {
            Serial.println("❌ Connection failed");
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

// =========================================================================
//  DELETE CLIENT
// =========================================================================
void deleteClient(NimBLEClient* client) {
    if (client != NULL) {
        try {
            if (client->isConnected()) {
                client->disconnect();
                delay(50);
            }
            NimBLEDevice::deleteClient(client);
        } catch (...) {}
    }
    delay(50);
}

// =========================================================================
//  CONTINUOUS SCANNING
// =========================================================================
void continuousScan() {
    if (!connected && !connecting) {
        try {
            if (pScan == NULL) {
                pScan = NimBLEDevice::getScan();
            }
            if (pScan == NULL) {
                Serial.println("❌ Failed to create scanner!");
                return;
            }
            
            pScan->setScanCallbacks(new ScanCallbacks(), false);
            pScan->setActiveScan(true);
            pScan->setInterval(100);
            pScan->setWindow(50);
            
            pScan->start(SCAN_INTERVAL, false);
        } catch (...) {
            Serial.println("❌ Scan error");
            pScan = NULL;
        }
    }
}

// =========================================================================
//  AUTO RECONNECT
// =========================================================================
void autoReconnect() {
    if (!connected && !connecting) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > RECONNECT_DELAY) {
            Serial.println("🔄 Scanning for wrist band...");
            lastReconnectAttempt = now;
        }
    }
}

// =========================================================================
//  SEND COMMAND TO WRIST
// =========================================================================
void sendCommandToWrist(String command) {
    if (!connected || pChar == NULL) {
        Serial.println("❌ Not connected to wrist!");
        return;
    }
    
    try {
        pChar->writeValue(command.c_str());
        Serial.println("📤 Command sent: " + command);
    } catch (...) {
        Serial.println("❌ Failed to send command");
    }
}

// =========================================================================
//  STATUS REPORTING
// =========================================================================
void printStatus() {
    static unsigned long lastStatusTime = 0;
    
    if (millis() - lastStatusTime > 10000) {
        Serial.println("========================================");
        Serial.println("📊 SYSTEM STATUS - WAIST BELT");
        Serial.print("🔗 Connected: ");
        Serial.println(connected ? "YES" : "NO");
        Serial.print("🔄 Connecting: ");
        Serial.println(connecting ? "YES" : "NO");
        Serial.print("📊 Data Received: ");
        Serial.println(dataReceived ? "YES" : "NO");
        Serial.print("🔢 Connect Attempts: ");
        Serial.println(connectAttempts);
        Serial.print("✅ Successful Connects: ");
        Serial.println(successfulConnects);
        Serial.print("🆔 Device UID: ");
        Serial.println(currentDeviceUID);
        Serial.print("💾 Free Heap: ");
        Serial.println(ESP.getFreeHeap());
        
        if (connected && pClient) {
            Serial.print("📱 Connected to: ");
            Serial.println(pClient->getPeerAddress().toString().c_str());
        }
        Serial.println("========================================");
        lastStatusTime = millis();
    }
}

// =========================================================================
//  PROCESS SERIAL COMMANDS
// =========================================================================
void processSerialCommand() {
    if (!Serial.available()) return;
    
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "help") {
        Serial.println("========================================");
        Serial.println("📖 COMMANDS - WAIST BELT");
        Serial.println("========================================");
        Serial.println("scan       - Force a BLE scan");
        Serial.println("disconnect - Disconnect from wrist");
        Serial.println("status     - Show connection status");
        Serial.println("ping       - Send PING to wrist");
        Serial.println("pair       - Send PAIR request");
        Serial.println("help       - Show this help");
        Serial.println("========================================");
    }
    else if (command == "scan") {
        if (pScan) {
            pScan->stop();
            delay(100);
            pScan->start(SCAN_INTERVAL, false);
            Serial.println("🔄 Scanning started...");
        }
    }
    else if (command == "disconnect") {
        if (pClient) {
            pClient->disconnect();
        }
        connected = false;
        connecting = false;
        Serial.println("🔌 Disconnected");
    }
    else if (command == "status") {
        printStatus();
    }
    else if (command == "ping") {
        sendCommandToWrist("PING:" + String(millis()));
    }
    else if (command == "pair") {
        sendCommandToWrist("PAIR:WAIST_" + String(millis(), HEX));
    }
}

// =========================================================================
//  SETUP
// =========================================================================
void setup() {
    Serial.setRxBufferSize(4096);
    Serial.setTxBufferSize(4096);
    Serial.begin(115200);
    delay(2000);
    
    Serial.println();
    Serial.println("========================================");
    Serial.println("=== SILVERCARE WAIST BELT BLE TEST ===");
    Serial.println("========================================");
    
    // Disable watchdog
    esp_task_wdt_deinit();
    
    Serial.println("🔍 Looking for: " + String(WRIST_NAME));
    Serial.println("📱 Service UUID: " + String(SERVICE_UUID));
    
    // Initialize BLE
    int bleRetries = 3;
    while (bleRetries > 0 && !bleInitialized) {
        try {
            NimBLEDevice::init("");
            NimBLEDevice::setMTU(247);
            NimBLEDevice::setPower(ESP_PWR_LVL_P9);
            
            bleInitialized = true;
            Serial.print("📱 Waist MAC: ");
            Serial.println(NimBLEDevice::getAddress().toString().c_str());
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
    
    Serial.println("========================================");
    Serial.println("💡 Type 'help' for commands");
    Serial.println("🔍 Scanning for wrist band...");
    Serial.println("========================================");
    
    lastReconnectAttempt = millis() - RECONNECT_DELAY;
}

// =========================================================================
//  LOOP
// =========================================================================
void loop() {
    unsigned long currentTime = millis();
    if (currentTime - lastLoopTime > WATCHDOG_TIMEOUT) {
        Serial.println("⚠️ Loop timeout detected! Resetting...");
        delay(100);
        ESP.restart();
    }
    lastLoopTime = currentTime;
    
    processSerialCommand();
    
    if (!connected && !connecting && bleInitialized) {
        continuousScan();
    }
    
    autoReconnect();
    printStatus();
    
    delay(50);
}