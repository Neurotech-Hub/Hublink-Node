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
#include <vector>
#include <string>
#include <atomic>

// BLE UUIDs
#define SERVICE_UUID "57617368-5501-0001-8000-00805f9b34fb"
// READ/WRITE/INDICATE
#define CHARACTERISTIC_UUID_FILENAME "57617368-5502-0001-8000-00805f9b34fb"
// READ/INDICATE
#define CHARACTERISTIC_UUID_FILETRANSFER "57617368-5503-0001-8000-00805f9b34fb"
// WRITE
#define CHARACTERISTIC_UUID_GATEWAY "57617368-5504-0001-8000-00805f9b34fb"
// READ
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
class HublinkIndicationCallbacks;

// Add near the top with other definitions
typedef void (*TimestampCallback)(uint32_t timestamp);

class Hublink
{
public:
    // Constructor & core functions
    Hublink(uint8_t chipSelect = SS, uint32_t clockFrequency = 1000000);

    // BLE control
    bool begin(String advName = "HUBLINK");
    bool initSD();
    void startAdvertising();
    void stopAdvertising();
    void updateMtuSize();
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
    std::vector<String> validExtensions = {".txt", ".csv", ".log", ".json"};

    // BLE configuration
    String advertise;

    void sleep(uint64_t milliseconds);
    void setCPUFrequency(CPUFrequency freq_mhz);
    bool doBLE();
    bool sync(uint32_t temporaryConnectFor = 0);

    // Make callback classes friends
    friend class HublinkServerCallbacks;
    friend class HublinkFilenameCallbacks;
    friend class HublinkGatewayCallbacks;
    friend class HublinkIndicationCallbacks;

    // Public methods
    void setTimestampCallback(TimestampCallback callback);
    void addValidExtension(const String &extension);
    void clearValidExtensions();
    void addValidExtensions(const std::vector<String> &extensions);
    const std::vector<String> &getValidExtensions() const;

    // Add new public methods
    void handleMetaJsonChunk(uint32_t id, const String &data);

    // BLE configuration (initialized with defaults)
    uint32_t advertise_every = DEFAULT_ADVERTISE_EVERY;
    uint32_t advertise_for = DEFAULT_ADVERTISE_FOR;
    bool disable = DEFAULT_DISABLE;
    String upload_path = DEFAULT_UPLOAD_PATH;
    String append_path = DEFAULT_APPEND_PATH;
    bool try_reconnect = DEFAULT_TRY_RECONNECT;
    uint8_t reconnect_attempts = DEFAULT_RECONNECT_ATTEMPTS;
    uint32_t reconnect_every = DEFAULT_RECONNECT_EVERY;

protected:
    // BLE characteristics
    NimBLECharacteristic *pFilenameCharacteristic;
    NimBLECharacteristic *pFileTransferCharacteristic;
    NimBLECharacteristic *pConfigCharacteristic;
    NimBLECharacteristic *pNodeCharacteristic;
    NimBLEServer *pServer;
    NimBLEService *pService;

    // Helper function for reliable indications
    bool sendIndication(NimBLECharacteristic *pChar, const uint8_t *data, size_t length);

    // State tracking
    String macAddress;
    bool piReadyForFilenames;
    bool allFilesSent;
    unsigned long watchdogTimer;

    // MTU configuration
    uint16_t mtuSize = 20;
    const uint16_t NEGOTIATE_MTU_SIZE = 515; // 512 + MTU_HEADER_SIZE
    const uint16_t MTU_HEADER_SIZE = 3;
    uint32_t watchdogTimeoutMs = 10000; // Default 10 seconds

    // SD card configuration
    uint8_t cs;
    uint32_t clkFreq;

    // Node content handling
    String metaJson;
    String configuredAdvName = "";
    String upload_path_json = ""; // JSON-encoded upload path for BLE characteristic

    // Helper functions
    void resetBLEState();

    unsigned long lastHublinkMillis;

    void printMemStats(const char *prefix);

    // Static callback instances - these must exist for the lifetime of the BLE connection
    static HublinkServerCallbacks serverCallbacks;
    static HublinkFilenameCallbacks filenameCallbacks;
    static HublinkGatewayCallbacks gatewayCallbacks;

    // Add to protected members
    TimestampCallback _timestampCallback = nullptr;
    void handleTimestamp(const String &timestamp);

    // Meta.json transfer state
    bool metaJsonTransferInProgress = false;
    uint32_t lastMetaJsonId = 0;
    String tempMetaJsonPath = "/meta.json.tmp";
    File tempMetaJsonFile;
    unsigned long metaJsonLastChunkTime = 0;
    const unsigned long META_JSON_TIMEOUT_MS = 5000; // 5 second timeout
    bool metaJsonUpdated = false;

    // Meta.json handling methods
    bool beginMetaJsonTransfer();
    bool processMetaJsonChunk(const String &data);
    bool finalizeMetaJsonTransfer();
    void cleanupMetaJsonTransfer();
    bool validateJsonStructure(const String &jsonStr);

    // Add retry tracking
    uint8_t currentRetryAttempt = 0;
    unsigned long lastRetryMillis = 0;
    bool connectionAttempted = false;

    // Connection state tracking
    bool didConnect = false; // Tracks if a connection was established during the current sync cycle

    // Critical: Must be called before NimBLE deinit to prevent crashes
    // Removes all callbacks to prevent dangling references during deinit
    void cleanupCallbacks();

    // Default values for BLE configuration
    static constexpr uint32_t DEFAULT_ADVERTISE_EVERY = 300; // 5 minutes
    static constexpr uint32_t DEFAULT_ADVERTISE_FOR = 30;    // 30 seconds
    static constexpr bool DEFAULT_DISABLE = false;
    static constexpr const char *DEFAULT_UPLOAD_PATH = "/";
    static constexpr const char *DEFAULT_APPEND_PATH = "";
    static constexpr bool DEFAULT_TRY_RECONNECT = true;
    static constexpr uint8_t DEFAULT_RECONNECT_ATTEMPTS = 3;
    static constexpr uint32_t DEFAULT_RECONNECT_EVERY = 30; // seconds

    // Helper function to extract nested JSON values
    String getNestedJsonValue(const JsonDocument &doc, const String &path);

    // Path processing helpers
    String sanitizePath(const String &path);
    String processAppendPath(const JsonDocument &doc, const String &basePath, const String &appendPath);
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
            // Handle timestamp
            String timestamp = g_hublink->parseGateway(pCharacteristic, "timestamp");
            if (timestamp.length() > 0)
            {
                g_hublink->handleTimestamp(timestamp);
            }

            // Handle sendFilenames flag
            String sendFilenames = g_hublink->parseGateway(pCharacteristic, "sendFilenames");
            g_hublink->sendFilenames = (sendFilenames == "true");

            // Handle watchdogTimeoutMs
            String watchdogTimeout = g_hublink->parseGateway(pCharacteristic, "watchdogTimeoutMs");
            if (watchdogTimeout.length() > 0)
            {
                g_hublink->watchdogTimeoutMs = watchdogTimeout.toInt();
            }

            // Handle meta.json transfer
            String metaJsonId = g_hublink->parseGateway(pCharacteristic, "metaJsonId");
            String metaJsonData = g_hublink->parseGateway(pCharacteristic, "metaJsonData");

            if (metaJsonId.length() > 0 && metaJsonData.length() > 0)
            {
                // Block other operations during meta.json transfer
                g_hublink->sendFilenames = false;
                g_hublink->currentFileName = "";

                g_hublink->handleMetaJsonChunk(metaJsonId.toInt(), metaJsonData);
            }
        }
    }
};

#endif
