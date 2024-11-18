#ifndef HublinkNode_ESP32_h
#define HublinkNode_ESP32_h

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <SD.h>
#include <SPI.h>

// BLE UUIDs
#define SERVICE_UUID "57617368-5501-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_FILENAME "57617368-5502-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_FILETRANSFER "57617368-5503-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_GATEWAY "57617368-5504-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_NODE "57617368-5505-0001-8000-00805f9b34fb"

class HublinkNode_ESP32
{
public:
    // Constructor & core functions
    HublinkNode_ESP32(uint8_t chipSelect = SS, uint32_t clockFrequency = 1000000);
    bool initializeSD();

    // BLE control
    void initBLE(String defaultAdvName, bool allowOverride = true);
    void deinitBLE();
    void startAdvertising();
    void stopAdvertising();
    void updateConnectionStatus();
    void updateMtuSize();
    void setBLECallbacks(BLEServerCallbacks *serverCallbacks,
                         BLECharacteristicCallbacks *filenameCallbacks,
                         BLECharacteristicCallbacks *configCallbacks);

    // Connection events
    void onConnect();
    void onDisconnect();

    // File handling
    void handleFileTransfer(String fileName);
    void sendAvailableFilenames();
    bool isValidFile(String fileName);
    String parseGateway(BLECharacteristic *pCharacteristic, const String &key);
    void setNodeChar();

    // Public state variables
    String currentFileName;
    bool deviceConnected = false;
    bool sendFilenames = false; // Set when config received via BLE
    String validExtensions[4] = {".txt", ".csv", ".log", ".node"};

    // BLE configuration
    uint32_t bleConnectEvery = 300; // Seconds between advertising periods
    uint32_t bleConnectFor = 30;    // Seconds of each advertising period
    bool disable = false;           // BLE enable flag
    String advName;

private:
    // BLE characteristics
    BLECharacteristic *pFilenameCharacteristic;
    BLECharacteristic *pFileTransferCharacteristic;
    BLECharacteristic *pConfigCharacteristic;
    BLECharacteristic *pNodeCharacteristic;
    BLEServer *pServer;

    // State tracking
    String macAddress;
    bool piReadyForFilenames;
    bool allFilesSent;
    unsigned long watchdogTimer;

    // MTU configuration
    uint16_t mtuSize = 20;
    const uint16_t NEGOTIATE_MTU_SIZE = 512;
    const uint16_t MTU_HEADER_SIZE = 3;
    const uint16_t WATCHDOG_TIMEOUT_MS = 5000;

    // SD card configuration
    uint8_t cs;
    uint32_t clkFreq;

    // Callback storage
    BLEServerCallbacks *serverCallbacks = nullptr;
    BLECharacteristicCallbacks *filenameCallbacks = nullptr;
    BLECharacteristicCallbacks *configCallbacks = nullptr;

    // Node content handling
    String nodeContent;              // Stores parsed node file content
    String configuredAdvName = "";   // Name from hublink.node file
    static const char *DEFAULT_NAME; // Default advertising name
    String setNodeContent();         // Reads and parses hublink.node file

    // Helper functions
    void processLine(const String &line, String &nodeContent);
    void resetBLEState();
};

#endif
