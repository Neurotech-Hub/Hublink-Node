// HublinkNode_ESP32.cpp
#include "HublinkNode_ESP32.h"

HublinkNode_ESP32::HublinkNode_ESP32() :
    piReadyForFilenames(false), deviceConnected(false),
    fileTransferInProgress(false), currentFileName(""), allFilesSent(false),
    watchdogTimer(0) {}

void HublinkNode_ESP32::initBLE() {
    // Initialize BLE
    BLEDevice::init("ESP32_BLE_SD");
    pServer = BLEDevice::createServer();

    BLEService *pService = pServer->createService(SERVICE_UUID);
    
    pFilenameCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_FILENAME,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_INDICATE);
    pFilenameCharacteristic->addDescriptor(new BLE2902());

    pFileTransferCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_FILETRANSFER,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_INDICATE);
    pFileTransferCharacteristic->addDescriptor(new BLE2902());

    pService->start();
    BLEDevice::getAdvertising()->start();
    Serial.println("BLE advertising started.");
}

void HublinkNode_ESP32::setBLECallbacks(BLEServerCallbacks* serverCallbacks, BLECharacteristicCallbacks* filenameCallbacks) {
    pServer->setCallbacks(serverCallbacks);
    pFilenameCharacteristic->setCallbacks(filenameCallbacks);
}

void HublinkNode_ESP32::updateConnectionStatus() {
    // Watchdog timer
    if (deviceConnected && (millis() - watchdogTimer > WATCHDOG_TIMEOUT_MS)) {
        Serial.println("Watchdog timeout detected, disconnecting...");
        pServer->disconnect(pServer->getConnId());
    }
}

void HublinkNode_ESP32::sendAvailableFilenames() {
    mtuSize = BLEDevice::getMTU();
    File root = SD.open("/");
    while (deviceConnected) {
        watchdogTimer = millis();  // Reset watchdog timer
        File entry = root.openNextFile();
        if (!entry) {
            Serial.println("All filenames sent.");
            pFilenameCharacteristic->setValue("EOF");
            pFilenameCharacteristic->indicate();
            allFilesSent = true;
            break;
        }

        String fileName = entry.name();
        if (isValidFile(fileName)) {
            String fileInfo = fileName + "|" + String(entry.size());
            int index = 0;
            while (index < fileInfo.length() && deviceConnected) {
                watchdogTimer = millis();
                String chunk = fileInfo.substring(index, index + mtuSize);
                pFilenameCharacteristic->setValue(chunk.c_str());
                pFilenameCharacteristic->indicate();
                index += mtuSize;
            }
            pFilenameCharacteristic->setValue("EON");
            pFilenameCharacteristic->indicate();
        }
    }
    root.close();
}

void HublinkNode_ESP32::handleFileTransfer(String fileName) {
    mtuSize = BLEDevice::getMTU();
    File file = SD.open("/" + fileName);
    if (!file) {
        Serial.printf("Failed to open file: %s\n", fileName.c_str());
        return;
    }

    uint8_t buffer[mtuSize];
    while (file.available() && deviceConnected) {
        watchdogTimer = millis();
        int bytesRead = file.read(buffer, mtuSize);
        if (bytesRead > 0) {
            pFileTransferCharacteristic->setValue(buffer, bytesRead);
            pFileTransferCharacteristic->indicate();
        } else {
            Serial.println("Error reading from file.");
            break;
        }
    }
    pFileTransferCharacteristic->setValue("EOF");
    pFileTransferCharacteristic->indicate();
    file.close();
    Serial.println("File transfer complete.");
}

bool HublinkNode_ESP32::isValidFile(String fileName) {
    String lowerFileName = fileName;
    lowerFileName.toLowerCase();
    for (int i = 0; i < 3; i++) {
        if (lowerFileName.endsWith(validExtensions[i])) {
            return true;
        }
    }
    return false;
}
