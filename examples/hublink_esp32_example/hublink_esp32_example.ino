#include <HublinkNode_ESP32.h>
#include <SPI.h>
#include <SD.h>

const bool TRACK_FILES_SENT = true;
const String BLE_AVD_NAME = "ESP32_BLE_SD";

// Pins for SD card
const int mosi = 35;
const int sck = 36;
const int miso = 37;
const int cs = A0;

HublinkNode_ESP32 hublinkNode;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    hublinkNode.onConnect();
  }

  void onDisconnect(BLEServer* pServer) override {
    hublinkNode.onDisconnect();
    Serial.println("Restarting BLE advertising...");
    BLEDevice::getAdvertising()->start();
  }
};

class FilenameCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    hublinkNode.currentFileName = String(pCharacteristic->getValue().c_str());
    if (hublinkNode.currentFileName != "") {
      hublinkNode.fileTransferInProgress = true;
    }
  }
};

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  delay(2000);  // Allow time for serial initialization
  Serial.println("Hello, hublink node.");

  // Setup SD card manually in user sketch
  SPI.begin(sck, miso, mosi, cs);
  while (!SD.begin(cs, SPI, 1000000)) {
    Serial.println("SD Card initialization failed!");
    delay(500);
  }
  Serial.println("SD Card initialized.");

  hublinkNode.initBLE(BLE_AVD_NAME, TRACK_FILES_SENT);
  // Set custom BLE callbacks
  hublinkNode.setBLECallbacks(new ServerCallbacks(), new FilenameCallback());
  BLEDevice::getAdvertising()->start();  // or ->stop();
  Serial.println("BLE advertising started.");
}

void loop() {
  // Update connection status and handle watchdog timeout
  hublinkNode.updateConnectionStatus();

  delay(100);  // Avoid busy waiting
}
