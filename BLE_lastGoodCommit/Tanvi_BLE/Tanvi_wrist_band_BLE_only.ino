// =========================================================================
//  SILVERCARE - WRIST BAND BLE TEST (NimBLE Server)
//  Purpose: Test BLE connectivity between Wrist and Waist
// =========================================================================

#include <NimBLEDevice.h>
#include <Preferences.h>
#include <esp_task_wdt.h>

// =========================================================================
//  BLE CONFIGURATION
// =========================================================================
#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BLE_IDENTITY_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define DEVICE_NAME             "SilverCare_Wrist"

// =========================================================================
//  TEST DATA CONFIGURATION
// =========================================================================
#define TEST_SERVICE_UUID     "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define TEST_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define TEST_IDENTITY_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a9"

// =========================================================================
//  BLE GLOBALS
// =========================================================================
bool deviceConnected = false;
NimBLEServer* pServer = NULL;
NimBLEService* pService = NULL;
NimBLECharacteristic* pCharacteristic = NULL;
NimBLECharacteristic* pIdentityCharacteristic = NULL;
NimBLEAdvertising* pAdvertising = NULL;

String uniqueDeviceID = "";
Preferences preferences;

unsigned long connectionStartTime = 0;
unsigned long lastDataSendTime = 0;
unsigned long lastStatusPrint = 0;

// =========================================================================
//  FORWARD DECLARATIONS
// =========================================================================
void sendTestData();
void sendIdentity();

// =========================================================================
//  ENCRYPTION MANAGER (Simplified)
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
//  BLE SERVER CALLBACKS
// =========================================================================
class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    deviceConnected = true;
    connectionStartTime = millis();
    
    Serial.println("========================================");
    Serial.println("✅ [BLE] WRIST BAND CONNECTED!");
    Serial.print("📱 Connected to: ");
    Serial.println(connInfo.getAddress().toString().c_str());
    Serial.print("📱 Connection ID: ");
    Serial.println(connInfo.getConnHandle());
    Serial.println("========================================");
    
    // Send initial test data
    sendTestData();
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    deviceConnected = false;
    
    Serial.println("========================================");
    Serial.println("❌ [BLE] WRIST BAND DISCONNECTED");
    Serial.print("📱 Reason: ");
    Serial.println(reason);
    Serial.println("🔄 Restarting advertising...");
    Serial.println("========================================");
    
    delay(100);
    if (pAdvertising) {
      pAdvertising->start();
    }
  }
};

// =========================================================================
//  CHARACTERISTIC CALLBACKS
// =========================================================================
class MyCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    Serial.println("📤 [BLE] Characteristic read request");
  }
  
  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    String value = pCharacteristic->getValue().c_str();
    Serial.print("📥 [BLE] Write request: ");
    Serial.println(value);
    
    if (value.startsWith("PAIR:")) {
      String response = "PAIR_ACK:" + uniqueDeviceID;
      pCharacteristic->setValue(response.c_str());
      pCharacteristic->notify();
      Serial.println("✅ Pairing acknowledged");
    } else if (value.startsWith("PING:")) {
      String response = "PONG:" + String(millis());
      pCharacteristic->setValue(response.c_str());
      pCharacteristic->notify();
      Serial.println("✅ PING responded with PONG");
    } else {
      // Echo back any received data
      String response = "ECHO:" + value;
      pCharacteristic->setValue(response.c_str());
      pCharacteristic->notify();
      Serial.println("✅ Echoed back: " + response);
    }
  }
  
  void onNotify(NimBLECharacteristic* pCharacteristic) {
    Serial.println("📤 [BLE] Notification sent");
  }
  
  void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) {
    Serial.print("📱 [BLE] Subscription changed: ");
    Serial.println(subValue);
  }
};

// =========================================================================
//  SEND TEST DATA VIA BLE
// =========================================================================
void sendTestData() {
  if (!deviceConnected || pCharacteristic == NULL) return;
  
  // Generate test sensor data
  int testHR = 72 + random(-5, 5);
  int testSPO2 = 97 + random(-2, 2);
  long testIR = 45000 + random(-2000, 2000);
  float testTemp = 32.5 + random(-5, 5) / 10.0;
  
  // Format: HR:SPO2:IR:Temp
  String plainData = String(testHR) + ":" + 
                     String(testSPO2) + ":" + 
                     String(testIR) + ":" + 
                     String(testTemp, 1);
  
  if (plainData.length() > 0 && plainData.length() < 50) {
    String encryptedData = EncryptionManager::encrypt(plainData);
    pCharacteristic->setValue(encryptedData.c_str());
    pCharacteristic->notify();
    
    Serial.print("📤 BLE Sent - HR:");
    Serial.print(testHR);
    Serial.print(" SPO2:");
    Serial.print(testSPO2);
    Serial.print(" IR:");
    Serial.print(testIR);
    Serial.print(" Temp:");
    Serial.println(testTemp, 1);
  }
}

// =========================================================================
//  SEND IDENTITY
// =========================================================================
void sendIdentity() {
  if (!deviceConnected || pIdentityCharacteristic == NULL) return;
  
  pIdentityCharacteristic->setValue(uniqueDeviceID.c_str());
  Serial.println("🆔 Identity sent: " + uniqueDeviceID);
}

// =========================================================================
//  BLE SERVER INITIALIZATION
// =========================================================================
void initBLEServer() {
  Serial.println("📱 Initializing BLE Server...");
  
  NimBLEDevice::init(DEVICE_NAME);
  NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  pService = pServer->createService(TEST_SERVICE_UUID);
  
  // Main data characteristic
  pCharacteristic = pService->createCharacteristic(
    TEST_CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::READ |
    NIMBLE_PROPERTY::WRITE |
    NIMBLE_PROPERTY::NOTIFY
  );
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  
  // Identity characteristic
  pIdentityCharacteristic = pService->createCharacteristic(
    TEST_IDENTITY_UUID,
    NIMBLE_PROPERTY::READ
  );
  pIdentityCharacteristic->setValue(uniqueDeviceID.c_str());
  
  pService->start();
  
  pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(TEST_SERVICE_UUID);
  
  NimBLEAdvertisementData advData;
  advData.setName(DEVICE_NAME);
  advData.setCompleteServices(NimBLEUUID(TEST_SERVICE_UUID));
  pAdvertising->setAdvertisementData(advData);
  
  NimBLEAdvertisementData scanData;
  scanData.setName(DEVICE_NAME);
  pAdvertising->setScanResponseData(scanData);
  
  pAdvertising->start();
  
  Serial.println("✅ BLE Server initialized");
  Serial.println("📱 Device Name: " + String(DEVICE_NAME));
  Serial.println("📱 Service UUID: " + String(TEST_SERVICE_UUID));
  Serial.println("📱 Characteristic UUID: " + String(TEST_CHARACTERISTIC_UUID));
  Serial.println("📱 Identity UUID: " + String(TEST_IDENTITY_UUID));
}

// =========================================================================
//  SETUP
// =========================================================================
void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("========================================");
  Serial.println("=== SILVERCARE WRIST BAND BLE TEST ===");
  Serial.println("========================================");

  // Disable watchdog
  esp_task_wdt_deinit();

  // Initialize Preferences
  preferences.begin("wrist_id", false);
  uniqueDeviceID = preferences.getString("unique_id", "");
  if (uniqueDeviceID.length() == 0) {
    uniqueDeviceID = "SCW_" + String(millis(), HEX);
    preferences.putString("unique_id", uniqueDeviceID);
    preferences.end();
    preferences.begin("wrist_id", false);
  }
  preferences.end();
  
  Serial.println("🆔 Device ID: " + uniqueDeviceID);
  Serial.println("📱 Device Name: " + String(DEVICE_NAME));

  // Initialize BLE
  initBLEServer();
  
  Serial.println("========================================");
  Serial.println("✅ System Ready!");
  Serial.println("💾 Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println("========================================");
  Serial.println("📱 Waiting for BLE connection...");
  Serial.println("📱 Device Name: " + String(DEVICE_NAME));
  Serial.println("📱 Service UUID: " + String(TEST_SERVICE_UUID));
  Serial.println("========================================");
  
  // Send initial identity
  sendIdentity();
}

// =========================================================================
//  LOOP
// =========================================================================
void loop() {
  unsigned long currentTime = millis();
  
  // Send test data every 2 seconds when connected
  if (deviceConnected && currentTime - lastDataSendTime > 2000) {
    sendTestData();
    lastDataSendTime = currentTime;
  }
  
  // Print connection status every 5 seconds
  if (currentTime - lastStatusPrint > 5000) {
    if (deviceConnected) {
      Serial.println("📱 BLE: CONNECTED - Sending test data");
      unsigned long connectionDuration = (currentTime - connectionStartTime) / 1000;
      Serial.println("📱 Connection duration: " + String(connectionDuration) + " seconds");
    } else {
      Serial.println("📱 BLE: Waiting for connection...");
    }
    lastStatusPrint = currentTime;
  }
  
  // Check for serial commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd == "help") {
      Serial.println("========================================");
      Serial.println("📖 COMMANDS");
      Serial.println("========================================");
      Serial.println("send     - Send test data manually");
      Serial.println("identity - Show device identity");
      Serial.println("status   - Show connection status");
      Serial.println("help     - Show this help");
      Serial.println("========================================");
    }
    else if (cmd == "send") {
      if (deviceConnected) {
        sendTestData();
        Serial.println("✅ Test data sent");
      } else {
        Serial.println("❌ Not connected to any device");
      }
    }
    else if (cmd == "identity") {
      Serial.println("🆔 Device ID: " + uniqueDeviceID);
    }
    else if (cmd == "status") {
      Serial.println("========================================");
      Serial.println("📊 STATUS");
      Serial.println("🔗 Connected: " + String(deviceConnected ? "YES" : "NO"));
      Serial.println("🆔 Device ID: " + uniqueDeviceID);
      Serial.println("📱 Device Name: " + String(DEVICE_NAME));
      Serial.println("💾 Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
      if (deviceConnected) {
        unsigned long duration = (millis() - connectionStartTime) / 1000;
        Serial.println("⏱️ Connected for: " + String(duration) + " seconds");
      }
      Serial.println("========================================");
    }
  }
  
  delay(10);
}