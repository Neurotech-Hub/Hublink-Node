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
#include "RTCManager.h"

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

// Debug byte map for Serial1 debugging
enum DebugByte : uint8_t
{
    // Core function events (0x01-0x0F)
    HUBLINK_BEGIN_FUNC = 0x01,
    HUBLINK_END_FUNC = 0x02,
    HUBLINK_ERROR = 0x03,
    HUBLINK_WARNING = 0x04,
    HUBLINK_INFO = 0x05,

    // BLE initialization events (0x10-0x1F)
    HUBLINK_BLE_INIT_START = 0x10,
    HUBLINK_BLE_INIT_POWER = 0x11,
    HUBLINK_BLE_CREATE_SERVER = 0x12,
    HUBLINK_BLE_CREATE_SERVICE = 0x13,
    HUBLINK_BLE_CREATE_CHARS = 0x14,
    HUBLINK_BLE_CREATE_CHAR_FILENAME = 0x15,
    HUBLINK_BLE_CREATE_CHAR_TRANSFER = 0x16,
    HUBLINK_BLE_CREATE_CHAR_CONFIG = 0x17,
    HUBLINK_BLE_CREATE_CHAR_NODE = 0x18,
    HUBLINK_BLE_START_SERVICE = 0x19,
    HUBLINK_BLE_ADV_CONFIG = 0x1A,
    HUBLINK_BLE_ADV_SET_UUID = 0x1B,
    HUBLINK_BLE_ADV_SET_NAME = 0x1C,
    HUBLINK_BLE_ADV_START = 0x1D,
    HUBLINK_BLE_ADV_STOP = 0x1E,

    // BLE connection events (0x20-0x2F)
    HUBLINK_BLE_CONNECT = 0x20,
    HUBLINK_BLE_DISCONNECT = 0x21,
    HUBLINK_BLE_ERROR = 0x22,
    HUBLINK_BLE_MTU_UPDATE = 0x23,
    HUBLINK_BLE_PARAMS_UPDATE = 0x24,

    // BLE sync events (0x30-0x3F)
    HUBLINK_BLE_SYNC_START = 0x30,
    HUBLINK_BLE_SYNC_END = 0x31,
    HUBLINK_BLE_SYNC_RETRY = 0x32,
    HUBLINK_BLE_SYNC_TIMEOUT = 0x33,

    // File operations (0x40-0x4F)
    HUBLINK_FILE_OPEN = 0x40,
    HUBLINK_FILE_CLOSE = 0x41,
    HUBLINK_FILE_WRITE = 0x42,
    HUBLINK_FILE_READ = 0x43,
    HUBLINK_FILE_OPEN_ERROR = 0x44,
    HUBLINK_FILE_LIST_START = 0x45,
    HUBLINK_FILE_LIST_END = 0x46,
    HUBLINK_FILE_ENTRY_OPEN = 0x47,
    HUBLINK_FILE_ENTRY_PROCESS = 0x48,

    // SD card events (0x50-0x5F)
    HUBLINK_SD_CONNECT = 0x50,
    HUBLINK_SD_ERROR = 0x51,
    HUBLINK_SD_BEGIN_ERROR = 0x52,
    HUBLINK_SD_ROOT_ERROR = 0x53,

    // Meta JSON events (0x60-0x6F)
    HUBLINK_META_JSON_UPDATE = 0x60,
    HUBLINK_META_JSON_READ = 0x61,
    HUBLINK_META_JSON_READ_ERROR = 0x62,
    HUBLINK_META_JSON_PARSE_ERROR = 0x63,

    // Sleep events (0x70-0x7F)
    HUBLINK_SLEEP_ENTER = 0x70,
    HUBLINK_SLEEP_EXIT = 0x71,

    // Wake-up reason codes (0x80-0x8F)
    HUBLINK_WAKE_RESET = 0x80,
    HUBLINK_WAKE_TIMER = 0x81,
    HUBLINK_WAKE_OTHER = 0x82,

    // Cleanup events (0x90-0x9F)
    HUBLINK_CLEANUP_START = 0x90,
    HUBLINK_CLEANUP_BLE = 0x91,
    HUBLINK_CLEANUP_FILES = 0x92,
    HUBLINK_CLEANUP_COMPLETE = 0x93,

    // Transfer events (0xA0-0xAF)
    HUBLINK_TRANSFER_START = 0xA0,
    HUBLINK_TRANSFER_SUCCESS = 0xA1,
    HUBLINK_TRANSFER_FAIL = 0xA2,
    HUBLINK_TRANSFER_TIMEOUT = 0xA3,
    HUBLINK_TRANSFER_WRITE_ERROR = 0xA4,
    HUBLINK_TRANSFER_CHUNK_START = 0xA5,
    HUBLINK_TRANSFER_CHUNK_SENT = 0xA6,
    HUBLINK_TRANSFER_EOF_SENT = 0xA7,
    HUBLINK_TRANSFER_INDICATION_FAIL = 0xA8
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
    bool doDebug = false;
    void debug(DebugByte byte, bool doDelay = true);

    // BLE control
    bool begin(String advName = "HUBLINK");
    bool beginSD();
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

    /**
     * Check if a key exists in meta.json
     *
     * Example usage:
     * if (hublink.hasMetaKey("subject", "doFoo")) {
     *     bool doFoo = hublink.getMeta<bool>("subject", "doFoo");
     * }
     *
     * @param parent Parent key in meta.json
     * @param child Child key under parent
     * @return true if both parent and child keys exist
     */
    bool hasMetaKey(const char *parent, const char *child);

    /**
     * Get meta.json value by parent and child keys with type conversion
     *
     * Example usage:
     * bool doFoo = hublink.getMeta<bool>("subject", "doFoo");
     * String id = hublink.getMeta<String>("subject", "id");
     * int value = hublink.getMeta<int>("subject", "count");
     *
     * Testing for missing values:
     * String id = hublink.getMeta<String>("subject", "id");
     * if (id.isEmpty()) {
     *     Serial.println("ID not found");
     * }
     */
    template <typename T>
    T getMeta(const char *parent, const char *child)
    {
        if (!metaDocValid)
        {
            readMetaJson();
        }

        if (!metaDoc.containsKey(parent))
        {
            Serial.printf("Warning: Parent key '%s' not found in meta.json\n", parent);
            return T();
        }

        JsonObject parentObj = metaDoc[parent];
        if (!parentObj.containsKey(child))
        {
            Serial.printf("Warning: Child key '%s' not found under '%s'\n", child, parent);
            return T();
        }

        return parentObj[child].as<T>();
    }

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

    // Add file handle tracking
    File rootFile;
    File transferFile;
    bool rootFileOpen = false;
    bool transferFileOpen = false;

    // Add document as protected member for getMeta access
    DynamicJsonDocument metaDoc;
    static const size_t META_DOC_SIZE = 2048; // Add constant for size
    bool metaDocValid = false;

    // Add with other private members
    RTCManager _rtc;
    bool _isRTCInitialized;
    bool _isSDInitialized;
    // Consider adding other component initialization flags:
    // bool _isBLEInitialized;
    // etc.
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
            Serial.println("Timestamp: " + timestamp);
            if (timestamp.length() > 0)
            {
                g_hublink->handleTimestamp(timestamp);
                Serial.println("Timestamp callback complete.");
            }

            // Handle sendFilenames flag
            String sendFilenames = g_hublink->parseGateway(pCharacteristic, "sendFilenames");
            g_hublink->sendFilenames = (sendFilenames == "true");
            Serial.println("Send filenames callback complete.");

            // Handle watchdogTimeoutMs
            String watchdogTimeout = g_hublink->parseGateway(pCharacteristic, "watchdogTimeoutMs");
            if (watchdogTimeout.length() > 0)
            {
                g_hublink->watchdogTimeoutMs = watchdogTimeout.toInt();
                Serial.println("Watchdog timeout callback complete.");
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
        Serial.println("Gateway callback complete.");
    }
};

#endif
