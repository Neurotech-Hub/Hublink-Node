// HublinkNode_ESP32.cpp
#include "HublinkNode_ESP32.h"

HublinkNode_ESP32::HublinkNode_ESP32() :
    piReadyForFilenames(false), deviceConnected(false),
    fileTransferInProgress(false), currentFileName(""), allFilesSent(false),
    watchdogTimer(0) {}

void HublinkNode_ESP32::initBLE(String advName) {
    // Initialize BLE
    BLEDevice::init(advName);
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
    onDisconnect(); // clear all vars and notification flags
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

    if (deviceConnected && fileTransferInProgress && currentFileName != "") {
        Serial.print("Requested file: ");
        Serial.println(currentFileName);
        handleFileTransfer(currentFileName);
        currentFileName = "";
        fileTransferInProgress = false;
    }

    // value=1 for notifications, =2 for indications
    if (deviceConnected && !fileTransferInProgress && !allFilesSent && (pFilenameCharacteristic->getDescriptorByUUID(BLEUUID((uint16_t)0x2902))->getValue()[0] & 0x0F) > 0) {
        updateMtuSize(); // after negotiation
        Serial.print("MTU Size (negotiated): ");
        Serial.println(mtuSize);
        Serial.println("Sending filenames...");
        sendAvailableFilenames();
    }
}

void HublinkNode_ESP32::sendAvailableFilenames() {
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
            Serial.println(fileName);
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

// HublinkNode_ESP32.cpp
void HublinkNode_ESP32::onConnect() {
    Serial.println("Device connected");
    deviceConnected = true;
    watchdogTimer = millis();
    BLEDevice::setMTU(NEGOTIATE_MTU_SIZE);
}

void HublinkNode_ESP32::onDisconnect() {
    Serial.println("Device disconnected");
    if (pFilenameCharacteristic) {
        uint8_t disableNotifyValue[2] = {0x00, 0x00};  // 0x00 to disable notifications
        pFilenameCharacteristic->getDescriptorByUUID(BLEUUID((uint16_t)0x2902))->setValue(disableNotifyValue, 2);
    }
    deviceConnected = false;
    piReadyForFilenames = false;
    fileTransferInProgress = false;
    allFilesSent = false;
}

void HublinkNode_ESP32::updateMtuSize() {
    mtuSize = BLEDevice::getMTU() - MTU_HEADER_SIZE;
}