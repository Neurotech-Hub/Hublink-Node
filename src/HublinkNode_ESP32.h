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
#define CHARACTERISTIC_UUID_GATEWAY "57617368-5504-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_NODE "57617368-5505-0001-8000-00805f9b34fb"

class HublinkNode_ESP32
{
public:
    // Constructor with default values for SD card configuration
    HublinkNode_ESP32(uint8_t chipSelect = SS, uint32_t clockFrequency = 1000000);
    void initBLE(String advName);
    void updateConnectionStatus();
    void handleFileTransfer(String fileName);
    void sendAvailableFilenames();
    bool isValidFile(String fileName);
    void onConnect();
    void onDisconnect();
    void updateMtuSize();
    void setBLECallbacks(BLEServerCallbacks *serverCallbacks,
                         BLECharacteristicCallbacks *filenameCallbacks,
                         BLECharacteristicCallbacks *configCallbacks);
    bool initializeSD();
    /**
     * Parse configuration data from BLE characteristic
     * @param pCharacteristic BLE characteristic containing config data
     * @param key Key to search for in config string
     * @return Value associated with key, or empty string if not found
     *
     * Config format: key1=value1;key2=value2;key3=value3
     * Example: BLE_CONNECT_EVERY=300;BLE_CONNECT_FOR=30;rtc=2024-03-21 14:30:00
     */
    String parseGateway(BLECharacteristic *pCharacteristic, const String &key);
    void setNodeChar();

    String currentFileName;
    bool fileTransferInProgress;
    bool deviceConnected;
    String validExtensions[3] = {".txt", ".csv", ".log"};
    /**
     * Flag indicating if configuration has been received via BLE.
     * When true, triggers the sending of available filenames to connected client.
     */
    bool gatewayChanged;

    // Time between BLE advertising periods (in seconds)
    uint32_t bleConnectEvery = 300; // 5 minutes

    // How long to keep BLE active each time (in seconds)
    uint32_t bleConnectFor = 30; // 30 seconds

    bool disable = false; // Default to enabled BLE

private:
    BLECharacteristic *pFilenameCharacteristic;
    BLECharacteristic *pFileTransferCharacteristic;
    BLECharacteristic *pConfigCharacteristic;
    BLECharacteristic *pNodeCharacteristic;
    BLEServer *pServer;

    String macAddress;
    bool piReadyForFilenames;
    bool allFilesSent;
    unsigned long watchdogTimer;

    uint16_t mtuSize = 20; // negotiate for higher
    const uint16_t NEGOTIATE_MTU_SIZE = 512;
    const uint16_t MTU_HEADER_SIZE = 3;
    const uint16_t WATCHDOG_TIMEOUT_MS = 10000;

    // SD card configuration
    uint8_t cs;
    uint32_t clkFreq;

    // Store callback pointers for cleanup
    BLEServerCallbacks *serverCallbacks = nullptr;
    BLECharacteristicCallbacks *filenameCallbacks = nullptr;
    BLECharacteristicCallbacks *configCallbacks = nullptr;

    // Helper function to process lines from hublink.node file
    void processLine(const String &line, String &nodeContent);
};

#endif
