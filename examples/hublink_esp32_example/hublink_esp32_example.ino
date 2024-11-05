#include <HublinkNode_ESP32.h>
#include <SPI.h>
#include <SD.h>

// Pins for SD card
const int mosi = 35;
const int sck = 36;
const int miso = 37;
const int cs = A0;

// --- HUBLINK HEADER START --- //
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
// --- HUBLINK HEADER END --- //

void setup() {
  // Initialize serial for debugging
  Serial.begin(9600);
  delay(2000);  // Allow time for serial initialization
  Serial.println("Hello, hublink node.");

  // Setup SD card manually in user sketch
  SPI.begin(sck, miso, mosi, cs);
  while (!SD.begin(cs, SPI, 1000000)) {
    Serial.println("SD Card initialization failed!");
    delay(500);
  }
  Serial.println("SD Card initialized.");

  // --- HUBLINK SETUP START --- //
  hublinkNode.initBLE("ESP32_BLE_SD");
  hublinkNode.setBLECallbacks(new ServerCallbacks(), new FilenameCallback());
  BLEDevice::getAdvertising()->start();  // USE CASE DEPENDENT or ->stop();
  // --- HUBLINK SETUP END --- //

  Serial.println("BLE setup done, looping....");
}

void loop() {
  hublinkNode.updateConnectionStatus(); // Update connection and watchdog timeout

  delay(100);  // Avoid busy waiting
}
