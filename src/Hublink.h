#ifndef Hublink_h
#define Hublink_h

#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <NimBLECharacteristic.h>
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

// CPU Frequency options that maintain radio functionality
// hublink.setCPUFrequency(CPUFrequency::MHz_80);
enum class CPUFrequency : uint32_t
{
    MHz_80 = 80,   // Minimum frequency for stable radio operation
    MHz_160 = 160, // Default frequency
    MHz_240 = 240  // Maximum frequency
};

// Forward declare callback classes
class HublinkServerCallbacks;
class HublinkFilenameCallbacks;
class HublinkGatewayCallbacks;

// Simple empty callbacks
class EmptyServerCallbacks : public NimBLEServerCallbacks
{
    void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override {}
    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override {}
};

class EmptyCharCallbacks : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override {}
};

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
    bool setBLECallbacks(NimBLEServerCallbacks *serverCallbacks = nullptr,
                         NimBLECharacteristicCallbacks *filenameCallbacks = nullptr,
                         NimBLECharacteristicCallbacks *configCallbacks = nullptr);
    String readMetaJson();

    // Connection events
    void onConnect();
    void onDisconnect();

    // File handling
    void handleFileTransfer(String fileName);
    void sendAvailableFilenames();
    bool isValidFile(String fileName);
    String parseGateway(NimBLECharacteristic *pCharacteristic, const String &key);

    // Public state variables
    String currentFileName;
    bool deviceConnected = false;
    bool sendFilenames = false;
    String validExtensions[4] = {".txt", ".csv", ".log", ".json"};

    // BLE configuration
    uint32_t bleConnectEvery = 300;
    uint32_t bleConnectFor = 30;
    bool disable = false;
    String advName;

    void sleep(uint64_t milliseconds);
    void setCPUFrequency(CPUFrequency freq_mhz);
    void doBLE();
    void sync();

    // Make callback classes friends
    friend class HublinkServerCallbacks;
    friend class HublinkFilenameCallbacks;
    friend class HublinkGatewayCallbacks;

protected:
    // Move these virtual methods to protected section
    virtual void handleFilenameWrite(NimBLECharacteristic *pCharacteristic)
    {
        String filename = String(pCharacteristic->getValue().c_str());
        Serial.println("Filename requested: " + filename);
        currentFileName = filename;
    }

    virtual void handleGatewayWrite(NimBLECharacteristic *pCharacteristic)
    {
        String rtc = parseGateway(pCharacteristic, "rtc");
        if (rtc.length() > 0)
        {
            Serial.println("Gateway settings received:");
            Serial.println("rtc: " + rtc);
        }
        sendFilenames = true;
    }

    // BLE characteristics
    NimBLECharacteristic *pFilenameCharacteristic;
    NimBLECharacteristic *pFileTransferCharacteristic;
    NimBLECharacteristic *pConfigCharacteristic;
    NimBLECharacteristic *pNodeCharacteristic;
    NimBLEServer *pServer;
    NimBLEService *pService;

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

    void createCallbacks();

    void printMemStats(const char *prefix);

    // Static callback instances
    static HublinkServerCallbacks serverCallbacks;
    static HublinkFilenameCallbacks filenameCallbacks;
    static HublinkGatewayCallbacks gatewayCallbacks;

private:
    // ... keep private members ...

    // Callback cleanup is critical for proper NimBLE deinit
    void cleanupCallbacks();
};

// Global pointer declaration
extern Hublink *g_hublink;

// Now define the callback classes
class HublinkServerCallbacks : public NimBLEServerCallbacks
{
public:
    void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override
    {
        if (g_hublink && pServer != nullptr)
        {
            g_hublink->onConnect();
        }
    }

    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override
    {
        if (g_hublink && pServer != nullptr)
        {
            g_hublink->onDisconnect();
        }
    }
};

class HublinkFilenameCallbacks : public NimBLECharacteristicCallbacks
{
public:
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
    {
        if (g_hublink && pCharacteristic)
        {
            g_hublink->currentFileName = String(pCharacteristic->getValue().c_str());
        }
    }
};

class HublinkGatewayCallbacks : public NimBLECharacteristicCallbacks
{
public:
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
    {
        if (g_hublink && pCharacteristic)
        {
            String rtc = g_hublink->parseGateway(pCharacteristic, "rtc");
            if (rtc.length() > 0)
            {
                Serial.println("Gateway settings received: " + rtc);
            }
            g_hublink->sendFilenames = true;
        }
    }
};

#endif
