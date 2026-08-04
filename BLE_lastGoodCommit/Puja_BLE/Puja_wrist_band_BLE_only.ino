#include <NimBLEDevice.h>
#include <Preferences.h>
#include <vector>
#include <algorithm>

// =========================================================================
//  BLE CONFIGURATION - OUR OWN SERVER (waist belt connects IN to us)
// =========================================================================
#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BLE_IDENTITY_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define BLE_MAC_UUID            "beb5483e-36e1-4688-b7f5-ea07361b26ac"
#define DEVICE_NAME              "SilverCare_Wrist"
#define PAIRING_PASSWORD         "SC2024_001"

// =========================================================================
//  BLE CONFIGURATION - WAIST BELT'S SERVER (we connect OUT to it)
// =========================================================================
#define WAIST_SERVICE_UUID   "5fafc201-1fb5-459e-8fcc-c5c9c331914c"
#define WAIST_IDENTITY_UUID  "cbb5483e-36e1-4688-b7f5-ea07361b26aa"
#define WAIST_PAIR_UUID      "cbb5483e-36e1-4688-b7f5-ea07361b26ab"
#define WAIST_MAC_UUID       "cbb5483e-36e1-4688-b7f5-ea07361b26ac"
#define WAIST_NAME            "SilverCare_Waist"

// =========================================================================
//  BLE SERVER VARIABLES (waist belt -> us)
// =========================================================================
bool deviceConnected = false;
NimBLEServer* pServer = NULL;
NimBLEService* pService = NULL;
NimBLECharacteristic* pCharacteristic = NULL;
NimBLECharacteristic* pIdentityCharacteristic = NULL;
NimBLECharacteristic* pMacCharacteristic = NULL;
NimBLEAdvertising* pAdvertising = NULL;

// =========================================================================
//  BLE CLIENT VARIABLES (us -> waist belt's server)
// =========================================================================
NimBLEScan* pWaistScan = NULL;
NimBLEClient* pWaistClient = NULL;
NimBLERemoteCharacteristic* pWaistPairChar = NULL;
NimBLERemoteCharacteristic* pWaistIdentityChar = NULL;
NimBLERemoteCharacteristic* pWaistMacChar = NULL;

bool connectingToWaist = false;
bool waistConnected = false;
bool clientLinkVerified = false;
bool waistMacVerified = false;
String waistDeviceID = "";
String waistMacAddress = "";
unsigned long waistConnectStartTime = 0;
const unsigned long WAIST_CONNECT_TIMEOUT = 30000;
unsigned long lastWaistScanAttempt = 0;
const unsigned long WAIST_SCAN_RETRY_DELAY = 5000;

// =========================================================================
//  UNIQUE DEVICE IDENTITY
// =========================================================================
String uniqueDeviceID = "";
String myMacAddress = "";
Preferences preferences;

// =========================================================================
//  FORWARD DECLARATIONS
// =========================================================================
void connectToWaist(NimBLEAddress address);
void continuousScanForWaist();
bool mutualPairingConfirmed();

// =========================================================================
//  MUTUAL PAIRING STATUS
// =========================================================================
bool mutualPairingConfirmed() {
    return deviceConnected && clientLinkVerified && waistMacVerified;
}

// =========================================================================
//  BLE SERVER CALLBACKS
// =========================================================================
class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    deviceConnected = true;
    Serial.println("========================================");
    Serial.println("✅ [BLE] WAIST BELT CONNECTED (to our server)!");
    Serial.print("📱 Connected to: ");
    Serial.println(connInfo.getAddress().toString().c_str());
    Serial.println("========================================");
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    deviceConnected = false;
    Serial.println("========================================");
    Serial.println("❌ [BLE] WAIST BELT DISCONNECTED (from our server)");
    Serial.println("🔄 Restarting advertising...");
    Serial.println("========================================");
    delay(100);
    if (pAdvertising) {
      pAdvertising->start();
    }
  }
};

class MyCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    Serial.println("📤 [BLE] Characteristic read request");
  }

  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    String value = pCharacteristic->getValue().c_str();
    Serial.print("📥 [BLE] Write request: ");
    Serial.println(value);

    if (value.startsWith("PAIR:")) {
      // Extract UID and MAC from pair request
      String pairData = value.substring(5);
      int separator = pairData.indexOf(':');
      if (separator > 0) {
        String peerUID = pairData.substring(0, separator);
        String peerMAC = pairData.substring(separator + 1);
       
        Serial.println("📥 [BLE] Peer UID: " + peerUID);
        Serial.println("📥 [BLE] Peer MAC: " + peerMAC);
       
        // Send ACK with our UID and MAC
        String response = "PAIR_ACK:" + uniqueDeviceID + ":" + myMacAddress;
        pCharacteristic->setValue(response.c_str());
        pCharacteristic->notify();
        Serial.println("✅ [BLE] Pairing acknowledged - sent ACK to waist belt");
        Serial.println("   Our UID: " + uniqueDeviceID);
        Serial.println("   Our MAC: " + myMacAddress);
      }
    }
  }

  void onNotify(NimBLECharacteristic* pCharacteristic) {
    Serial.println("📤 [BLE] Notification sent");
  }
};

// =========================================================================
//  BLE CLIENT CALLBACKS
// =========================================================================
class WaistClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* client) {
        waistConnected = true;
        connectingToWaist = false;
        waistMacVerified = false;
        Serial.println("✅ [BLE-CLIENT] Connected to Waist Belt's identity service");
        Serial.println("🔄 Verifying waist MAC address...");
    }

    void onDisconnect(NimBLEClient* client, int reason) {
        waistConnected = false;
        connectingToWaist = false;
        clientLinkVerified = false;
        waistMacVerified = false;
        pWaistPairChar = NULL;
        pWaistIdentityChar = NULL;
        pWaistMacChar = NULL;
        Serial.println("❌ [BLE-CLIENT] Disconnected from Waist Belt - will rescan");
    }
};

// =========================================================================
//  SCAN CALLBACKS
// =========================================================================
class WaistScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        if (advertisedDevice->getName() == WAIST_NAME && !waistConnected && !connectingToWaist) {
            Serial.println("📡 [BLE-CLIENT] Found SilverCare Waist Belt - connecting");
            NimBLEDevice::getScan()->stop();
            connectToWaist(advertisedDevice->getAddress());
        }
    }
};

// =========================================================================
//  NOTIFY CALLBACK FOR THE WAIST BELT'S PAIR CHARACTERISTIC
// =========================================================================
void waistPairNotifyCallback(NimBLERemoteCharacteristic* ch, uint8_t* data, size_t len, bool isNotify) {
    if (len == 0 || data == NULL) return;
    String msg = String((char*)data).substring(0, len);
    Serial.println("📥 [BLE-CLIENT] Waist Belt reply: " + msg);

    if (msg.startsWith("PAIR_ACK:")) {
        String ackData = msg.substring(9);
        int separator = ackData.indexOf(':');
        if (separator > 0) {
            waistDeviceID = ackData.substring(0, separator);
            String waistMAC = ackData.substring(separator + 1);
           
            Serial.println("📥 [BLE-CLIENT] Waist UID: " + waistDeviceID);
            Serial.println("📥 [BLE-CLIENT] Waist MAC: " + waistMAC);
           
            // Verify MAC matches
            if (waistMAC == waistMacAddress) {
                clientLinkVerified = true;
                waistMacVerified = true;
                Serial.println("========================================");
                Serial.println("🤝 MUTUAL PAIRING COMPLETE!");
                Serial.println("   Waist Belt ID: " + waistDeviceID);
                Serial.println("   Waist MAC: " + waistMAC);
                Serial.println("   Our ID: " + uniqueDeviceID);
                Serial.println("   Our MAC: " + myMacAddress);
                Serial.println("   ✅ MAC addresses verified!");
                Serial.println("   Now sending data...");
                Serial.println("========================================");
            } else {
                Serial.println("❌ [BLE-CLIENT] MAC address mismatch!");
                Serial.println("   Expected: " + waistMacAddress);
                Serial.println("   Received: " + waistMAC);
            }
        }
    }
}

// =========================================================================
//  CONNECT TO WAIST BELT
// =========================================================================
void connectToWaist(NimBLEAddress address) {
    if (connectingToWaist || waistConnected) return;

    connectingToWaist = true;
    waistConnectStartTime = millis();
    waistMacVerified = false;

    Serial.println("========================================");
    Serial.println("🔗 [BLE-CLIENT] Connecting to Waist Belt at " + String(address.toString().c_str()));
    Serial.println("========================================");

    if (pWaistClient == NULL) {
        pWaistClient = NimBLEDevice::createClient();
        pWaistClient->setClientCallbacks(new WaistClientCallbacks(), false);
    }

    if (!pWaistClient->connect(address)) {
        Serial.println("❌ [BLE-CLIENT] Failed to connect to Waist Belt");
        connectingToWaist = false;
        return;
    }

    // Wait a moment for services to be discovered
    delay(500);

    NimBLERemoteService* pWaistService = pWaistClient->getService(WAIST_SERVICE_UUID);
    if (pWaistService == nullptr) {
        Serial.println("❌ [BLE-CLIENT] Waist Belt service not found - disconnecting");
        pWaistClient->disconnect();
        connectingToWaist = false;
        return;
    }
    Serial.println("✅ [BLE-CLIENT] Waist Belt service found");

    // Read the waist belt's Device ID
    pWaistIdentityChar = pWaistService->getCharacteristic(WAIST_IDENTITY_UUID);
    if (pWaistIdentityChar != nullptr && pWaistIdentityChar->canRead()) {
        waistDeviceID = pWaistIdentityChar->readValue().c_str();
        Serial.println("🆔 [BLE-CLIENT] Waist Belt Device ID: " + waistDeviceID);
    } else {
        Serial.println("⚠️ [BLE-CLIENT] Could not read Waist Belt Device ID");
    }

    // Read the waist belt's MAC address
    pWaistMacChar = pWaistService->getCharacteristic(WAIST_MAC_UUID);
    if (pWaistMacChar != nullptr && pWaistMacChar->canRead()) {
        waistMacAddress = pWaistMacChar->readValue().c_str();
        Serial.println("📥 [BLE-CLIENT] Waist MAC Address: " + waistMacAddress);
    } else {
        Serial.println("⚠️ [BLE-CLIENT] Could not read Waist MAC Address");
    }

    // Get the pair characteristic
    pWaistPairChar = pWaistService->getCharacteristic(WAIST_PAIR_UUID);
    if (pWaistPairChar != nullptr) {
        Serial.println("✅ [BLE-CLIENT] Waist Belt pair characteristic found");
       
        // Subscribe to notifications
        if (pWaistPairChar->canNotify()) {
            pWaistPairChar->subscribe(true, waistPairNotifyCallback);
            Serial.println("✅ [BLE-CLIENT] Subscribed to pair characteristic notifications");
        }
       
        // Send PAIR request with our ID and MAC
        if (pWaistPairChar->canWrite()) {
            String pairMsg = "PAIR:" + uniqueDeviceID + ":" + myMacAddress;
            pWaistPairChar->writeValue(pairMsg.c_str(), pairMsg.length());
            Serial.println("📤 [BLE-CLIENT] Sent PAIR request to Waist Belt");
            Serial.println("   Our ID: " + uniqueDeviceID);
            Serial.println("   Our MAC: " + myMacAddress);
        } else {
            Serial.println("❌ [BLE-CLIENT] Waist Belt PAIR characteristic not writable");
        }
    } else {
        Serial.println("❌ [BLE-CLIENT] Waist Belt PAIR characteristic not found");
        pWaistClient->disconnect();
        connectingToWaist = false;
        return;
    }

    connectingToWaist = false;
}

// =========================================================================
//  CONTINUOUS SCANNING FOR THE WAIST BELT
// =========================================================================
void continuousScanForWaist() {
    if (waistConnected || connectingToWaist) return;
    if (millis() - lastWaistScanAttempt < WAIST_SCAN_RETRY_DELAY) return;
    lastWaistScanAttempt = millis();

    try {
        if (pWaistScan == NULL) {
            pWaistScan = NimBLEDevice::getScan();
            pWaistScan->setScanCallbacks(new WaistScanCallbacks());
            pWaistScan->setActiveScan(true);
            pWaistScan->setInterval(100);
            pWaistScan->setWindow(50);
        }
        Serial.println("🔍 [BLE-CLIENT] Scanning for Waist Belt...");
        pWaistScan->start(3, false);
    } catch (...) {
        Serial.println("❌ [BLE-CLIENT] Waist Belt scan error");
        pWaistScan = NULL;
    }
}

// =========================================================================
//  SEND HEART DATA OVER BLE
// =========================================================================
void sendHeartData(String data) {
    if (!deviceConnected || pCharacteristic == NULL) return;
    if (!mutualPairingConfirmed()) {
        Serial.println("⏳ Mutual pairing not confirmed - holding data");
        return;
    }

    if (data.length() > 0 && data.length() < 50) {
        pCharacteristic->setValue(data.c_str());
        pCharacteristic->notify();
        Serial.print("📤 BLE Sent: ");
        Serial.println(data);
    }
}

// =========================================================================
//  BLE SERVER INITIALIZATION
// =========================================================================
void initBLEServer() {
    NimBLEDevice::init(DEVICE_NAME);
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    // Get our MAC address
    myMacAddress = NimBLEDevice::getAddress().toString().c_str();
    Serial.println("📱 Our MAC Address: " + myMacAddress);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    pService = pServer->createService(BLE_SERVICE_UUID);

    pCharacteristic = pService->createCharacteristic(
        BLE_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ |
        NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::NOTIFY
    );
    pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

    pIdentityCharacteristic = pService->createCharacteristic(
        BLE_IDENTITY_UUID,
        NIMBLE_PROPERTY::READ
    );
    pIdentityCharacteristic->setValue(uniqueDeviceID.c_str());

    // MAC Address characteristic - so waist can read our MAC
    pMacCharacteristic = pService->createCharacteristic(
        BLE_MAC_UUID,
        NIMBLE_PROPERTY::READ
    );
    pMacCharacteristic->setValue(myMacAddress.c_str());

    pService->start();

    pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);

    NimBLEAdvertisementData advData;
    advData.setName(DEVICE_NAME);
    advData.setCompleteServices(NimBLEUUID(BLE_SERVICE_UUID));
    pAdvertising->setAdvertisementData(advData);

    NimBLEAdvertisementData scanData;
    scanData.setName(DEVICE_NAME);
    pAdvertising->setScanResponseData(scanData);

    pAdvertising->start();
    Serial.println("📱 BLE Server started, advertising as: " + String(DEVICE_NAME));
    Serial.println("📱 Advertising MAC: " + myMacAddress);
}

// =========================================================================
//  SETUP BLE
// =========================================================================
void setupBLE() {
    // Initialize Preferences for device ID
    bool prefsOpened = preferences.begin("wrist_id", false);
    if (!prefsOpened) {
        preferences.end();
        preferences.begin("wrist_id", true);
        preferences.clear();
        preferences.end();
        prefsOpened = preferences.begin("wrist_id", false);
    }

    uniqueDeviceID = preferences.getString("unique_id", "");
    if (uniqueDeviceID.length() == 0) {
        uniqueDeviceID = "SCW_" + String(millis(), HEX);
        preferences.putString("unique_id", uniqueDeviceID);
    }
    preferences.end();
    Serial.println("🆔 Device ID: " + uniqueDeviceID);

    // Initialize BLE Server
    initBLEServer();
    Serial.println("📱 BLE Advertising as: " + String(DEVICE_NAME));
    Serial.println("📱 Service UUID: " + String(BLE_SERVICE_UUID));
    Serial.println("🔍 Will also scan for and connect to: " + String(WAIST_NAME) + " (mutual pairing)");
}

// =========================================================================
//  MAIN BLE LOOP - CALL THIS IN YOUR main loop()
// =========================================================================
void bleLoop() {
    // Scan for waist belt
    if (!waistConnected && !connectingToWaist) {
        continuousScanForWaist();
    }
   
    if (connectingToWaist && millis() - waistConnectStartTime > WAIST_CONNECT_TIMEOUT) {
        Serial.println("⏰ [BLE-CLIENT] Waist Belt connect timeout - retrying");
        if (pWaistClient) pWaistClient->disconnect();
        connectingToWaist = false;
    }

    // Print connection status periodically
    static unsigned long lastStatusPrint = 0;
    if (millis() - lastStatusPrint > 10000) {
        Serial.println("========================================");
        Serial.println("📊 BLE CONNECTION STATUS");
        Serial.println("📱 Server link (waist -> us): " + String(deviceConnected ? "CONNECTED" : "WAITING"));
        Serial.println("📱 Client link (us -> waist): " + String(waistConnected ? (clientLinkVerified ? "VERIFIED" : "CONNECTED (unverified)") : "SCANNING"));
        Serial.println("✅ MAC Verified (waist): " + String(waistMacVerified ? "YES" : "NO"));
        Serial.println("🤝 Mutual Pairing: " + String(mutualPairingConfirmed() ? "CONFIRMED - sending data" : "WAITING"));
        Serial.println("========================================");
        lastStatusPrint = millis();
    }
}

// =========================================================================
//  EXAMPLE SETUP FUNCTION
// =========================================================================
void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("========================================");
    Serial.println("=== SILVERCARE WRIST BAND (MAC VERIFIED) ===");
    Serial.println("========================================");

    // Setup BLE
    setupBLE();

    Serial.println("========================================");
    Serial.println("✅ BLE System Ready!");
    Serial.println("🔐 MAC Address Verification: REQUIRED");
    Serial.println("📱 Waiting for BLE connection (and mutual pairing)...");
    Serial.println("========================================");
}

// =========================================================================
//  EXAMPLE LOOP FUNCTION
// =========================================================================
void loop() {
    // Handle BLE operations
    bleLoop();
   
    // Your other code here...
    delay(100);
}
