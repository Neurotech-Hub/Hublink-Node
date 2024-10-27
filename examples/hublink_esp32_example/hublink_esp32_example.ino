#include <HublinkNode_ESP32.h>
#include <SPI.h>
#include <SD.h>

// Pins for SD card
const int sck = 12;
const int miso = 13;
const int mosi = 11;
const int cs = 10;

HublinkNode_ESP32 hublinkNode;

class CustomServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    Serial.println("Device connected");
    BLEDevice::getAdvertising()->stop();
    hublinkNode.deviceConnected = true;
    hublinkNode.watchdogTimer = millis();
    BLEDevice::setMTU(512);
  }

  void onDisconnect(BLEServer* pServer) {
    Serial.println("Device disconnected");
    hublinkNode.deviceConnected = false;
    hublinkNode.piReadyForFilenames = false;
    hublinkNode.fileTransferInProgress = false;
    hublinkNode.allFilesSent = false;
    BLEDevice::getAdvertising()->start();
    Serial.println("Restarting BLE advertising...");
  }
};

class CustomFilenameCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    String rxValue = String(pCharacteristic->getValue().c_str());
    Serial.printf("Requested file: %s\n", rxValue.c_str());
    if (rxValue != "") {
      hublinkNode.handleFileTransfer(rxValue);
    }
  }
};

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  delay(2000);  // Allow time for serial initialization

  // Setup SD card manually in user sketch
  SPI.begin(sck, miso, mosi, cs);
  while (!SD.begin(cs, SPI, 1000000)) {
    Serial.println("SD Card initialization failed!");
    delay(500);
  }
  Serial.println("SD Card initialized.");

  hublinkNode.initBLE("ESP32_BLE_SD");
  BLEDevice::getAdvertising()->start();  // or ->stop();

  // Set custom BLE callbacks
  hublinkNode.setBLECallbacks(new CustomServerCallbacks(), new CustomFilenameCallback());
}

void loop() {
  // Update connection status and handle watchdog timeout
  hublinkNode.updateConnectionStatus();

  // If device is connected, send available filenames
  if (hublinkNode.deviceConnected && !hublinkNode.fileTransferInProgress) {
    hublinkNode.sendAvailableFilenames();
  }

  delay(100);  // Avoid busy waiting
}
