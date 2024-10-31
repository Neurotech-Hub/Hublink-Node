#include <HublinkNode_ESP32.h>
#include <SPI.h>
#include <SD.h>

// Pins for SD card
const int sck = 12;
const int miso = 13;
const int mosi = 11;
const int cs = 10;

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

  hublinkNode.initBLE("ESP32_BLE_SD");
  // Set custom BLE callbacks
  hublinkNode.setBLECallbacks(new ServerCallbacks(), new FilenameCallback());
  BLEDevice::getAdvertising()->start();  // or ->stop();
  Serial.println("BLE advertising started.");
}

void loop() {
  // Update connection status and handle watchdog timeout
  hublinkNode.updateConnectionStatus();

  // If device is connected, send available filenames
  // if (hublinkNode.deviceConnected && !hublinkNode.fileTransferInProgress) {
  //   hublinkNode.sendAvailableFilenames();
  // }

  delay(100);  // Avoid busy waiting
}
