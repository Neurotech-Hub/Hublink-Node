#include "HublinkNode_ESP32.h"
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
        hublinkNode.deviceConnected = true;
    }

    void onDisconnect(BLEServer* pServer) {
        Serial.println("Device disconnected");
        hublinkNode.deviceConnected = false;
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

    // Initialize BLE with HublinkNode
    hublinkNode.initBLE();

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
