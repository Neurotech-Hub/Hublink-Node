#ifndef Hublink_h
#define Hublink_h

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <SD.h>
#include <SPI.h>
#include <esp_sleep.h>
#include <ArduinoJson.h>
#include "esp_system.h"

// BLE UUIDs
#define SERVICE_UUID "57617368-5501-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_FILENAME "57617368-5502-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_FILETRANSFER "57617368-5503-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_GATEWAY "57617368-5504-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_NODE "57617368-5505-0001-8000-00805f9b34fb"

// File paths
#define META_JSON_PATH "/meta.json"

class Hublink
{
public:
    // Constructor & core functions
    Hublink(uint8_t chipSelect = SS, uint32_t clockFrequency = 1000000);

    // BLE control
    bool init(String defaultAdvName = "HUBLINK", bool allowOverride = true);
    bool initSD();
    void startAdvertising();
    void stopAdvertising();
    void updateConnectionStatus();
    void updateMtuSize();
    void setBLECallbacks(BLEServerCallbacks *serverCallbacks,
                         BLECharacteristicCallbacks *filenameCallbacks,
                         BLECharacteristicCallbacks *configCallbacks);
    String readMetaJson(); // Reads and parses meta.json file

    // Connection events
    void onConnect();
    void onDisconnect();

    // File handling
    void handleFileTransfer(String fileName);
    void sendAvailableFilenames();
    bool isValidFile(String fileName);
    String parseGateway(BLECharacteristic *pCharacteristic, const String &key);

    // Public state variables
    String currentFileName;
    bool deviceConnected = false;
    bool sendFilenames = false; // Set when config received via BLE
    String validExtensions[4] = {".txt", ".csv", ".log", ".json"};

    // BLE configuration
    uint32_t bleConnectEvery = 300; // Seconds between advertising periods
    uint32_t bleConnectFor = 30;    // Seconds of each advertising period
    bool disable = false;           // BLE enable flag
    String advName;

    void sleep(uint64_t milliseconds);
    void setCPUFrequency(uint32_t freq_mhz);

    // Virtual methods for callbacks
    virtual void doBLE();

    void sync(); // called from loop()

    ~Hublink();

protected:
    // Default callback classes
    class DefaultServerCallbacks : public BLEServerCallbacks
    {
        Hublink *hublink;

    public:
        DefaultServerCallbacks(Hublink *hl) : hublink(hl) {}
        void onConnect(BLEServer *pServer) override { hublink->handleServerConnect(pServer); }
        void onDisconnect(BLEServer *pServer) override { hublink->handleServerDisconnect(pServer); }
    };

    class DefaultFilenameCallback : public BLECharacteristicCallbacks
    {
        Hublink *hublink;

    public:
        DefaultFilenameCallback(Hublink *hl) : hublink(hl) {}
        void onWrite(BLECharacteristic *pCharacteristic) override
        {
            hublink->handleFilenameWrite(pCharacteristic);
        }
    };

    class DefaultGatewayCallback : public BLECharacteristicCallbacks
    {
        Hublink *hublink;

    public:
        DefaultGatewayCallback(Hublink *hl) : hublink(hl) {}
        void onWrite(BLECharacteristic *pCharacteristic) override
        {
            hublink->handleGatewayWrite(pCharacteristic);
        }
    };

    // Virtual methods that can be overridden
    virtual void handleServerConnect(BLEServer *pServer)
    {
        Serial.println("Device connected!");
        onConnect();
    }

    virtual void handleServerDisconnect(BLEServer *pServer)
    {
        Serial.println("Device disconnected!");
        onDisconnect();
    }

    virtual void handleFilenameWrite(BLECharacteristic *pCharacteristic)
    {
        String filename = String(pCharacteristic->getValue().c_str());
        Serial.println("Filename requested: " + filename);
        currentFileName = filename;
    }

    virtual void handleGatewayWrite(BLECharacteristic *pCharacteristic)
    {
        String rtc = parseGateway(pCharacteristic, "rtc");
        if (rtc.length() > 0)
        {
            Serial.println("Gateway settings received:");
            Serial.println("rtc: " + rtc);
        }
        sendFilenames = true;
    }

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
    const uint16_t NEGOTIATE_MTU_SIZE = 515; // 512 + MTU_HEADER_SIZE
    const uint16_t MTU_HEADER_SIZE = 3;
    const uint16_t WATCHDOG_TIMEOUT_MS = 10000; // gateway could be pinging server

    // SD card configuration
    uint8_t cs;
    uint32_t clkFreq;

    // Node content handling
    String metaJson;
    String configuredAdvName = "";

    // Helper functions
    void resetBLEState();

    unsigned long lastHublinkMillis;

    // Default callback instances
    DefaultServerCallbacks *defaultServerCallbacks;
    DefaultFilenameCallback *defaultFilenameCallbacks;
    DefaultGatewayCallback *defaultGatewayCallbacks;

    String defaultAdvName;
    bool allowOverride;
};

#endif
