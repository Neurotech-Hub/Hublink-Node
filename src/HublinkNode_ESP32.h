#ifndef HublinkNode_ESP32_h
#define HublinkNode_ESP32_h

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <SD.h>
#include <SPI.h>

#define SERVICE_UUID "57617368-5501-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_FILENAME "57617368-5502-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_FILETRANSFER "57617368-5503-0001-8000-00805f9b34fb"

class HublinkNode_ESP32 {
public:
    HublinkNode_ESP32();
    void initBLE(String advName);
    void updateConnectionStatus();
    void handleFileTransfer(String fileName);
    void sendAvailableFilenames();
    bool isValidFile(String fileName);
    void onConnect();
    void onDisconnect();
    void updateMtuSize();
    void setBLECallbacks(BLEServerCallbacks* serverCallbacks, BLECharacteristicCallbacks* filenameCallbacks);
    String currentFileName;
    bool fileTransferInProgress;
    bool deviceConnected;
    String validExtensions[3] = { ".txt", ".csv", ".log" };

private:
    BLECharacteristic *pFilenameCharacteristic;
    BLECharacteristic *pFileTransferCharacteristic;
    BLEServer *pServer;

    String macAddress;
    bool piReadyForFilenames;
    bool allFilesSent;
    unsigned long watchdogTimer;

    uint16_t mtuSize = 20; // negotiate for higher
    const uint16_t NEGOTIATE_MTU_SIZE = 512;
    const uint16_t MTU_HEADER_SIZE = 3;
    const uint16_t WATCHDOG_TIMEOUT_MS = 5000;
};

#endif