#include "Hublink.h"

// Define global variables
Hublink *g_hublink = nullptr;

// Define static members
HublinkServerCallbacks Hublink::serverCallbacks;
HublinkFilenameCallbacks Hublink::filenameCallbacks;
HublinkGatewayCallbacks Hublink::gatewayCallbacks;

Hublink::Hublink(uint8_t chipSelect, uint32_t clockFrequency)
    : cs(chipSelect),
      clkFreq(clockFrequency),
      piReadyForFilenames(false),
      deviceConnected(false),
      currentFileName(""),
      allFilesSent(false),
      watchdogTimer(0),
      sendFilenames(false)
{
    g_hublink = this; // Set the global pointer
}

bool Hublink::begin(String advName)
{
    // Set CPU frequency to minimum required for radio operation
    setCPUFrequency(CPUFrequency::MHz_80);

    // 2. Initialize SD
    if (!beginSD())
    {
        Serial.println("✗ SD Card.");
        return false;
    }
    Serial.println("✓ SD Card.");

    advertise = advName;
    // Reset to defaults
    advertise_every = DEFAULT_ADVERTISE_EVERY;
    advertise_for = DEFAULT_ADVERTISE_FOR;
    disable = DEFAULT_DISABLE;
    upload_path = DEFAULT_UPLOAD_PATH;
    append_path = DEFAULT_APPEND_PATH;
    try_reconnect = DEFAULT_TRY_RECONNECT;
    reconnect_attempts = DEFAULT_RECONNECT_ATTEMPTS;
    reconnect_every = DEFAULT_RECONNECT_EVERY;

    // 3. Read meta.json: set advertising name, subject id
    readMetaJson();

    lastHublinkMillis = millis();
    return true;
}

String Hublink::sanitizePath(const String &path)
{
    String sanitized = "";
    for (char c : path)
    {
        // Allow alphanumeric, hyphen, underscore, plus, period
        if (isalnum(c) || c == '-' || c == '_' || c == '+' || c == '.' || c == '/')
        {
            sanitized += c;
        }
    }

    // Replace multiple consecutive slashes with a single slash
    while (sanitized.indexOf("//") != -1)
    {
        sanitized.replace("//", "/");
    }

    // Remove trailing slash if present
    while (sanitized.endsWith("/"))
    {
        sanitized = sanitized.substring(0, sanitized.length() - 1);
    }

    return sanitized;
}

String Hublink::getNestedJsonValue(const JsonDocument &doc, const String &path)
{
    // Split path into parts (e.g., "subject:id" -> ["subject", "id"])
    int colonPos = path.indexOf(':');
    if (colonPos == -1)
        return "";

    String parent = path.substring(0, colonPos);
    String child = path.substring(colonPos + 1);

    // Check if parent exists and has child
    if (doc.containsKey(parent) && doc[parent].containsKey(child))
    {
        String value = doc[parent][child].as<String>();
        // Return empty string if the value is empty or only whitespace
        value.trim();
        return value;
    }

    return "";
}

String Hublink::processAppendPath(const JsonDocument &doc, const String &basePath, const String &appendPath)
{
    String finalPath = basePath;

    // Split append_path by '/'
    int startPos = 0;
    int slashPos;

    while ((slashPos = appendPath.indexOf('/', startPos)) != -1 || startPos < appendPath.length())
    {
        // Extract the current pair
        String pair;
        { // Create scope for temporary String management
            if (slashPos == -1)
            {
                pair = appendPath.substring(startPos);
                startPos = appendPath.length();
            }
            else
            {
                pair = appendPath.substring(startPos, slashPos);
                startPos = slashPos + 1;
            }

            // Get and validate the nested value
            String value = getNestedJsonValue(doc, pair);
            if (value.length() > 0)
            {
                // Add separator if needed
                if (!finalPath.endsWith("/"))
                {
                    finalPath += "/";
                }
                finalPath += value;
            }

            // Let Strings go out of scope naturally
        }
    }

    // Sanitize and return the complete path
    String result = sanitizePath(finalPath);
    finalPath = ""; // Clear the string

    return result;
}

String Hublink::readMetaJson()
{
    if (!beginSD())
    {
        Serial.println("Failed to initialize SD card when reading meta.json");
        return "";
    }

    File configFile = SD.open(META_JSON_PATH, FILE_READ);
    if (!configFile)
    {
        Serial.println("No meta.json file found, using defaults");
        return "";
    }

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();

    if (error)
    {
        Serial.print("Failed to parse meta.json: ");
        Serial.println(error.c_str());
        return "";
    }

    JsonObject hublink = doc["hublink"];
    String content = "";

    // Parse configuration
    if (hublink.containsKey("advertise"))
    {
        advertise = hublink["advertise"].as<String>();
    }

    // Handle upload path and append path
    if (hublink.containsKey("upload_path"))
    {
        String path = hublink["upload_path"].as<String>();
        if (path.length() > 0)
        {
            upload_path = path;
            // Ensure path starts with /
            if (!upload_path.startsWith("/"))
            {
                upload_path = "/" + upload_path;
            }
            Serial.printf("Set upload_path from meta.json: %s\n", upload_path.c_str());
        }
    }

    // Handle append_path if it exists
    if (hublink.containsKey("append_path"))
    {
        String appendPath = hublink["append_path"].as<String>();
        if (appendPath.length() > 0)
        {
            upload_path = processAppendPath(doc, upload_path, appendPath);
            Serial.printf("Final upload_path: %s\n", upload_path.c_str());
        }
    }

    if (hublink.containsKey("advertise_every") && hublink.containsKey("advertise_for"))
    {
        uint32_t newEvery = hublink["advertise_every"];
        uint32_t newFor = hublink["advertise_for"];

        if (newEvery > 0 && newFor > 0)
        {
            advertise_every = newEvery;
            advertise_for = newFor;
            Serial.printf("Updated intervals: every=%d, for=%d\n", advertise_every, advertise_for);
        }
    }

    // Add new retry configuration parsing
    if (hublink.containsKey("try_reconnect"))
    {
        try_reconnect = hublink["try_reconnect"].as<bool>();
        Serial.printf("Retry enabled: %s\n", try_reconnect ? "true" : "false");
    }

    if (hublink.containsKey("reconnect_attempts"))
    {
        uint8_t attempts = hublink["reconnect_attempts"].as<uint8_t>();
        if (attempts > 0)
        {
            reconnect_attempts = attempts;
            Serial.printf("Retry attempts set to: %d\n", reconnect_attempts);
        }
    }

    if (hublink.containsKey("reconnect_every"))
    {
        uint32_t every = hublink["reconnect_every"].as<uint32_t>();
        if (every > 0)
        {
            reconnect_every = every; // Keep in seconds
            Serial.printf("Retry interval set to: %d seconds\n", every);
        }
    }

    if (hublink.containsKey("disable"))
    {
        disable = hublink["disable"].as<bool>();
        Serial.printf("BLE disable flag set to: %s\n", disable ? "true" : "false");
    }

    // Create JSON for upload_path
    StaticJsonDocument<128> pathDoc;
    pathDoc["upload_path"] = upload_path;
    upload_path_json = "";
    serializeJson(pathDoc, upload_path_json);
    Serial.printf("Encoded upload path JSON: %s\n", upload_path_json.c_str());

    return content;
}

void Hublink::setCPUFrequency(CPUFrequency freq_mhz)
{
    setCpuFrequencyMhz(static_cast<uint32_t>(freq_mhz));
}

void Hublink::startAdvertising()
{
    NimBLEDevice::init(advertise.c_str());
    pServer = NimBLEDevice::createServer();

    if (!pServer)
    {
        Serial.println("Failed to create server!");
        return;
    }

    // Set callbacks using static instances
    pServer->setCallbacks(&serverCallbacks);

    pService = pServer->createService(SERVICE_UUID);

    pFilenameCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_FILENAME,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE);
    pFilenameCharacteristic->setCallbacks(&filenameCallbacks);

    pFileTransferCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_FILETRANSFER,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::INDICATE);

    pConfigCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_GATEWAY,
        NIMBLE_PROPERTY::WRITE);
    pConfigCharacteristic->setCallbacks(&gatewayCallbacks); // Set characteristic callback

    pNodeCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_NODE,
        NIMBLE_PROPERTY::READ);

    pNodeCharacteristic->setValue(upload_path_json.c_str());
    Serial.printf("Node characteristic value: %s\n", upload_path_json.c_str());

    pService->start();

    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    NimBLEAdvertisementData scanResponse;
    scanResponse.setName(advertise.c_str());
    pAdvertising->setScanResponseData(scanResponse);
    pAdvertising->start();
}

void Hublink::stopAdvertising()
{
    BLEDevice::stopAdvertising();
    // Critical: Clean up callbacks before deinit to prevent crashes
    // Order matters: disconnect clients -> remove callbacks -> stop advertising -> deinit
    if (pServer != nullptr)
    {
        // First disconnect any connected clients
        if (pServer->getConnectedCount() > 0)
        {
            pServer->disconnect(0);
            delay(50); // Give time for disconnect to complete
        }

        // Remove all callbacks before deinit
        cleanupCallbacks();

        // Stop advertising
        NimBLEDevice::getAdvertising()->stop();
        delay(50);
    }

    resetBLEState();
    NimBLEDevice::deinit(true);
    delay(100); // Add delay after deinit
}

void Hublink::cleanupCallbacks()
{
    // Remove all callbacks before deinit to prevent crashes
    if (pServer != nullptr)
    {
        pServer->setCallbacks(nullptr);
    }
    if (pFilenameCharacteristic != nullptr)
    {
        pFilenameCharacteristic->setCallbacks(nullptr);
    }
    if (pConfigCharacteristic != nullptr)
    {
        pConfigCharacteristic->setCallbacks(nullptr);
    }
}

// Use SD.begin(cs, SPI, clkFreq) whenever SD functions are needed in this way:
bool Hublink::beginSD()
{
    return SD.begin(cs, SPI, clkFreq);
}

void Hublink::sendAvailableFilenames()
{
    // Check if root directory is valid
    if (!rootFileOpen) // Changed from checking rootFile directly
    {
        Serial.println("Invalid root directory handle");
        return;
    }

    String accumulatedFileInfo = "";
    while (deviceConnected)
    {
        watchdogTimer = millis();
        File entry = rootFile.openNextFile();
        if (!entry)
        {
            Serial.println("All filenames sent.");
            int index = 0;
            while (index < accumulatedFileInfo.length() && deviceConnected)
            {
                watchdogTimer = millis();
                String chunk = accumulatedFileInfo.substring(index, index + mtuSize);
                if (!sendIndication(pFilenameCharacteristic, (uint8_t *)chunk.c_str(), chunk.length()))
                {
                    Serial.println("Failed to send indication");
                    break;
                }
                index += mtuSize;
            }
            // Send "EOF" as a separate indication to signal the end
            if (!sendIndication(pFilenameCharacteristic, (uint8_t *)"EOF", 3))
            {
                Serial.println("Failed to send EOF indication");
            }
            allFilesSent = true;
            accumulatedFileInfo = ""; // Clear the accumulated string
            break;
        }

        String fileName = entry.name();
        if (isValidFile(fileName))
        {
            Serial.println(fileName);
            String fileInfo = fileName + "|" + String(entry.size());
            if (!accumulatedFileInfo.isEmpty())
            {
                accumulatedFileInfo += ";"; // Separate entries with a semicolon
            }
            accumulatedFileInfo += fileInfo;
        }
    }
}

void Hublink::handleFileTransfer(String fileName)
{
    // Check if transfer file is valid
    if (!transferFileOpen) // Changed from checking transferFile directly
    {
        Serial.printf("Failed to use file: %s\n", fileName.c_str());
        if (!sendIndication(pFileTransferCharacteristic, (uint8_t *)"NFF", 3))
        {
            Serial.println("Failed to send NFF (no file found) indication");
        }
        return;
    }

    uint8_t buffer[mtuSize];
    while (transferFile.available() && deviceConnected)
    {
        watchdogTimer = millis();
        int bytesRead = transferFile.read(buffer, mtuSize);

        if (bytesRead > 0)
        {
            if (!sendIndication(pFileTransferCharacteristic, buffer, bytesRead))
            {
                Serial.println("Failed to send file chunk indication");
                break;
            }
        }
        else
        {
            Serial.println("Error reading from file.");
            break;
        }
    }
    if (!sendIndication(pFileTransferCharacteristic, (uint8_t *)"EOF", 3))
    {
        Serial.println("Failed to send EOF indication");
    }
    Serial.println("File transfer complete.");
}

bool Hublink::isValidFile(String fileName)
{
    // Exclude files that start with a dot
    if (fileName.startsWith("."))
    {
        return false;
    }

    // Convert filename to lowercase
    String lowerFileName = fileName;
    lowerFileName.toLowerCase();

    // Check for valid extensions
    for (const auto &ext : validExtensions) // Use range-based for loop
    {
        if (lowerFileName.endsWith(ext))
        {
            return true;
        }
    }
    return false;
}

void Hublink::onConnect()
{
    Serial.println("Hublink node connected.");
    deviceConnected = true;
    didConnect = true;
    watchdogTimer = millis();
    watchdogTimeoutMs = 10000; // Reset to default on each new connection

    // Get the connection handle from the first connected client
    uint16_t connHandle = pServer->getPeerInfo(0).getConnHandle();

    // Now we can properly update connection parameters in correct order:
    // connHandle, minInterval, maxInterval, latency, timeout
    pServer->updateConnParams(connHandle, 6, 8, 0, 100);
    NimBLEDevice::setMTU(NEGOTIATE_MTU_SIZE);
}

void Hublink::onDisconnect()
{
    Serial.println("Hublink node disconnected.");
    deviceConnected = false;
}

void Hublink::resetBLEState()
{
    deviceConnected = false;
    piReadyForFilenames = false;
    currentFileName = "";
    currentFileName = "";
    allFilesSent = false;
    sendFilenames = false;
    watchdogTimer = 0;
    didConnect = false;

    // Enhanced meta.json cleanup
    if (metaJsonTransferInProgress)
    {
        cleanupMetaJsonTransfer();
    }

    // Reset any path construction temporaries
    if (tempMetaJsonFile)
    {
        tempMetaJsonFile.close();
    }

    // Clean up file handles
    if (rootFileOpen)
    {
        rootFile.close();
        rootFileOpen = false;
    }
    if (transferFileOpen)
    {
        transferFile.close();
        transferFileOpen = false;
    }
}

void Hublink::updateMtuSize()
{
    mtuSize = NimBLEDevice::getMTU() - MTU_HEADER_SIZE;
    Serial.printf("Updated MTU size: %d\n", mtuSize);
}

String Hublink::parseGateway(NimBLECharacteristic *pCharacteristic, const String &key)
{
    std::string value = pCharacteristic->getValue().c_str();
    if (value.empty())
    {
        Serial.println("Config: None");
        return "";
    }

    String configStr = String(value.c_str());
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, configStr);

    if (error)
    {
        Serial.print("Config: JSON parsing failed: ");
        Serial.println(error.c_str());
        return "";
    }

    if (!doc.containsKey(key))
    {
        return "";
    }

    // Only print the config if the key was found
    Serial.println("Config: Parsing JSON: " + configStr);

    // Handle boolean values specifically
    if (doc[key].is<bool>())
    {
        return doc[key].as<bool>() ? "true" : "false";
    }

    return doc[key].as<String>();
}

void Hublink::sleep(uint64_t seconds)
{
    uint64_t microseconds = seconds * 1000000ULL; // Convert seconds to microseconds
    esp_sleep_enable_timer_wakeup(microseconds);
    esp_light_sleep_start();
    delay(10); // wakeup delay
}

bool Hublink::doBLE()
{
    startAdvertising();
    unsigned long subLoopStartTime = millis();
    bool successfulTransfer = false;

    // update connection status
    while ((millis() - subLoopStartTime < advertise_for * 1000 && !didConnect) || deviceConnected)
    {
        // Watchdog timer
        if (deviceConnected && (millis() - watchdogTimer > watchdogTimeoutMs))
        {
            Serial.println("Hublink node timeout detected, disconnecting...");

            // Cleanup any in-progress transfers
            if (metaJsonTransferInProgress)
            {
                cleanupMetaJsonTransfer();
            }

            // Cleanup any open file handles
            if (rootFileOpen)
            {
                rootFile.close();
                rootFileOpen = false;
            }
            if (transferFileOpen)
            {
                transferFile.close();
                transferFileOpen = false;
            }

            // Disconnect using the connection handle
            if (pServer->getConnectedCount() > 0)
            {
                NimBLEConnInfo connInfo = pServer->getPeerInfo(0);
                pServer->disconnect(connInfo.getConnHandle());
            }
            delay(10);
        }

        // Handle file transfers when connected
        if (deviceConnected && !currentFileName.isEmpty())
        {
            Serial.print("Requested file: ");
            Serial.println(currentFileName);

            // Initialize SD before opening file
            if (!beginSD())
            {
                Serial.println("Failed to initialize SD card for file transfer");
                currentFileName = "";
                continue;
            }

            // Close any previously open transfer file
            if (transferFileOpen)
            {
                transferFile.close();
                transferFileOpen = false;
            }

            // Open new file and check if successful
            transferFile = SD.open("/" + currentFileName);
            if (transferFile)
            {
                transferFileOpen = true;
                handleFileTransfer(currentFileName);
            }
            else
            {
                Serial.println("Failed to open file for transfer");
            }

            // Clean up after transfer
            if (transferFileOpen)
            {
                transferFile.close();
                transferFileOpen = false;
            }
            currentFileName = "";
        }

        // once sendFilenames is true
        if (deviceConnected && currentFileName.isEmpty() && !allFilesSent && sendFilenames)
        {
            updateMtuSize();
            Serial.print("MTU Size (negotiated): ");
            Serial.println(mtuSize);
            Serial.println("Sending filenames...");

            // Initialize SD before opening root directory
            if (!beginSD())
            {
                Serial.println("Failed to initialize SD card for file listing");
                continue;
            }

            // Close any previously open root directory
            if (rootFileOpen)
            {
                rootFile.close();
                rootFileOpen = false;
            }

            // Open root directory and check if successful
            rootFile = SD.open("/");
            if (rootFile)
            {
                rootFileOpen = true;
                sendAvailableFilenames();
            }
            else
            {
                Serial.println("Failed to open root directory");
            }

            // Clean up after sending filenames
            if (rootFileOpen)
            {
                rootFile.close();
                rootFileOpen = false;
            }
        }

        if (deviceConnected && allFilesSent)
        {
            successfulTransfer = true;
        }

        didConnect |= deviceConnected;
        delay(100);
    }

    // Final cleanup of any remaining open handles
    if (rootFileOpen)
    {
        rootFile.close();
        rootFileOpen = false;
    }
    if (transferFileOpen)
    {
        transferFile.close();
        transferFileOpen = false;
    }

    stopAdvertising();

    if (metaJsonUpdated)
    {
        readMetaJson(); // update any new meta.json values
    }
    metaJsonUpdated = false;

    return successfulTransfer;
}

/**
 * Time units for BLE connection parameters:
 *
 * @param temporaryConnectFor: seconds - Temporary override for advertising duration
 * @param advertise_every: seconds - Time between advertising cycles (set in meta.json)
 * @param advertise_for: seconds - Duration of each advertising cycle (set in meta.json)
 * @param reconnect_every: seconds - Time between reconnection attempts (set in meta.json)
 */
bool Hublink::sync(uint32_t temporaryConnectFor)
{
    uint32_t currentTime = millis();
    bool connectionSuccess = false;

    // Add safety check for cleanup at the start of each sync
    if (!deviceConnected && metaJsonTransferInProgress)
    {
        cleanupMetaJsonTransfer();
    }

    // Cast to uint32_t to ensure consistent arithmetic
    uint32_t timeSinceLastSync = (uint32_t)(currentTime - lastHublinkMillis);
    uint32_t syncThreshold = (uint32_t)(advertise_every * 1000UL); // Use UL suffix for clarity

    if (!disable && (temporaryConnectFor > 0 || timeSinceLastSync >= syncThreshold))
    {
        // Serial.printf("Time since last sync: %lu ms (threshold: %lu ms)\n",
        //               currentTime - lastHublinkMillis,
        //               advertise_every * 1000);

        // Serial.printf("Retry state - attempted: %s, currentAttempt: %d/%d, timeSinceLastRetry: %lu ms\n",
        //               connectionAttempted ? "true" : "false",
        //               connectionAttempted ? currentRetryAttempt + 1 : 0,
        //               reconnect_attempts,
        //               currentTime - lastRetryMillis);

        if (connectionAttempted && currentRetryAttempt >= reconnect_attempts)
        {
            // Serial.println("Max retries reached, resetting retry state");
            connectionAttempted = false;
            currentRetryAttempt = 0;
            lastHublinkMillis = currentTime;
            return false;
        }

        if (!connectionAttempted ||
            (currentRetryAttempt < reconnect_attempts &&
             currentTime - lastRetryMillis >= reconnect_every * 1000UL)) // Convert to ms here
        {
            Serial.println("Hublink started advertising... ");

            uint32_t originalConnectFor = advertise_for;
            if (temporaryConnectFor > 0)
            {
                Serial.printf("Using temporary connection duration: %d seconds\n", temporaryConnectFor);
                advertise_for = temporaryConnectFor;
            }

            connectionSuccess = doBLE();

            if (!connectionSuccess && try_reconnect && temporaryConnectFor == 0) // Only retry if not temporary
            {
                if (!connectionAttempted)
                {
                    connectionAttempted = true;
                    currentRetryAttempt = 1;
                    Serial.println("First connection attempt failed, enabling retry logic");
                }
                else
                {
                    currentRetryAttempt++;
                    Serial.printf("Retry attempt %d/%d scheduled\n",
                                  currentRetryAttempt,
                                  reconnect_attempts);
                }
                lastRetryMillis = currentTime;
            }
            else
            {
                if (connectionSuccess)
                {
                    Serial.println("Connection successful, resetting retry state");
                }
                else if (!try_reconnect || temporaryConnectFor > 0)
                {
                    Serial.println("Retries disabled, not attempting reconnection");
                }
                connectionAttempted = false;
                currentRetryAttempt = 0;
            }

            advertise_for = originalConnectFor;

            Serial.printf("Done advertising. Connection %s.\n",
                          connectionSuccess ? "successful" : "failed");
            lastHublinkMillis = millis();
        }
        else
        {
            // Serial.printf("Skipping connection attempt - max retries reached or waiting for retry interval (%lu ms remaining)\n",
            //               reconnect_every - (currentTime - lastRetryMillis));
        }
    }
    // else
    // {
    //     if (disable)
    //     {
    //         Serial.println("Sync skipped - BLE disabled");
    //     }
    //     else
    //     {
    //         uint32_t timeUntilNext = (advertise_every * 1000) - (currentTime - lastHublinkMillis);
    //         Serial.printf("Sync skipped - %lu ms until next connection attempt\n", timeUntilNext);
    //     }
    // }

    if (!connectionSuccess && metaJsonTransferInProgress)
    {
        // Ensure cleanup if connection attempt failed
        cleanupMetaJsonTransfer();
    }

    return connectionSuccess;
}

void Hublink::printMemStats(const char *prefix)
{
    static uint32_t lastMinFreeHeap = 0;
    uint32_t currentMinFreeHeap = ESP.getMinFreeHeap();

    Serial.printf("%s - Free: %lu, Min: %lu, Max Block: %lu, PSRAM: %lu",
                  prefix,
                  ESP.getFreeHeap(),
                  currentMinFreeHeap,
                  ESP.getMaxAllocHeap(),
                  ESP.getFreePsram());

    // Only show the difference if this isn't the first call
    if (lastMinFreeHeap > 0)
    {
        int32_t diff = currentMinFreeHeap - lastMinFreeHeap;
        Serial.printf(", Min Diff: %+ld", diff); // %+ld will show the sign (+ or -) before the number
    }

    Serial.println(); // End the line

    lastMinFreeHeap = currentMinFreeHeap;
}

void Hublink::setTimestampCallback(TimestampCallback callback)
{
    _timestampCallback = callback;
}

void Hublink::handleTimestamp(const String &timestamp)
{
    if (_timestampCallback != nullptr && timestamp.length() > 0)
    {
        uint32_t unix_timestamp = timestamp.toInt();
        _timestampCallback(unix_timestamp);
    }
}

void Hublink::addValidExtension(const String &extension)
{
    // Convert to lowercase for case-insensitive comparison
    String lowerExt = extension;
    lowerExt.toLowerCase();
    // Add dot if not present
    if (!lowerExt.startsWith("."))
    {
        lowerExt = "." + lowerExt;
    }
    validExtensions.push_back(lowerExt);
}

void Hublink::clearValidExtensions()
{
    validExtensions.clear();
}

void Hublink::addValidExtensions(const std::vector<String> &extensions)
{
    validExtensions.clear();
    for (const String &ext : extensions)
    {
        addValidExtension(ext);
    }
}

const std::vector<String> &Hublink::getValidExtensions() const
{
    return validExtensions;
}

class HublinkIndicationCallbacks : public NimBLECharacteristicCallbacks
{
private:
    std::atomic<bool> indicationComplete{false};
    std::atomic<bool> indicationSuccess{false};

public:
    void onStatus(NimBLECharacteristic *pCharacteristic, int code) override
    {
        indicationComplete = true;
        indicationSuccess = false; // Default to failure

        switch (code)
        {
        case BLE_HS_EDONE:
            indicationSuccess = true;
            break;
        case BLE_HS_ETIMEOUT:
            Serial.printf("Indication status: TIMEOUT (code: %d) on characteristic: %s\n",
                          code, pCharacteristic->getUUID().toString().c_str());
            break;
        default:
            Serial.printf("Indication status: UNKNOWN (code: %d) on characteristic: %s\n",
                          code, pCharacteristic->getUUID().toString().c_str());
            break;
        }
    }

    void reset()
    {
        indicationComplete = false;
        indicationSuccess = false;
    }

    bool waitForComplete(unsigned long timeout)
    {
        unsigned long start = millis();
        while (!indicationComplete && (millis() - start < timeout))
        {
            delay(1); // avoid tight loop
        }
        // Serial.printf("Indication complete, took %lu ms\n", millis() - start);
        return indicationComplete; // Return true if we got any response
    }
};

bool Hublink::sendIndication(NimBLECharacteristic *pChar, const uint8_t *data, size_t length)
{
    if (!deviceConnected)
    {
        return false;
    }

    static HublinkIndicationCallbacks indicationCallbacks;
    NimBLECharacteristicCallbacks *originalCallbacks = pChar->getCallbacks();

    pChar->setCallbacks(&indicationCallbacks);
    pChar->setValue(data, length);

    const int maxRetries = 3;
    const unsigned long timeout = 1000;
    bool success = false;

    for (int i = 0; i < maxRetries && !success && deviceConnected; i++)
    {
        indicationCallbacks.reset();

        if (!pChar->indicate())
        {
            delay(10);
            continue;
        }

        success = indicationCallbacks.waitForComplete(timeout);
        if (!success)
        {
            delay(10);
        }
    }

    pChar->setCallbacks(originalCallbacks);
    return success;
}

bool Hublink::beginMetaJsonTransfer()
{
    if (!beginSD())
    {
        Serial.println("Failed to initialize SD for meta.json transfer");
        return false;
    }

    // Remove any existing temporary file
    if (SD.exists(tempMetaJsonPath))
    {
        SD.remove(tempMetaJsonPath);
    }

    tempMetaJsonFile = SD.open(tempMetaJsonPath, FILE_WRITE);
    if (!tempMetaJsonFile)
    {
        Serial.println("Failed to create temporary meta.json file");
        return false;
    }

    metaJsonTransferInProgress = true;
    lastMetaJsonId = 0;
    metaJsonLastChunkTime = millis();

    Serial.println("Meta.json transfer started");
    return true;
}

bool Hublink::validateJsonStructure(const String &jsonStr)
{
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, jsonStr);

    if (error)
    {
        Serial.print("JSON validation failed: ");
        Serial.println(error.c_str());
        return false;
    }

    // Verify required structure
    if (!doc.containsKey("hublink"))
    {
        Serial.println("Missing required 'hublink' key");
        return false;
    }

    return true;
}

bool Hublink::processMetaJsonChunk(const String &data)
{
    if (!metaJsonTransferInProgress || !tempMetaJsonFile)
    {
        Serial.println("No active meta.json transfer");
        return false;
    }

    size_t bytesWritten = tempMetaJsonFile.print(data);
    if (bytesWritten != data.length())
    {
        Serial.println("Failed to write chunk to temporary file");
        cleanupMetaJsonTransfer();
        return false;
    }

    metaJsonLastChunkTime = millis();
    return true;
}

bool Hublink::finalizeMetaJsonTransfer()
{
    if (!metaJsonTransferInProgress || !tempMetaJsonFile)
    {
        return false;
    }

    tempMetaJsonFile.close();

    // Validate the complete JSON file
    File validateFile = SD.open(tempMetaJsonPath);
    if (!validateFile)
    {
        Serial.println("Failed to open temp file for validation");
        cleanupMetaJsonTransfer();
        return false;
    }

    String jsonContent = validateFile.readString();
    validateFile.close();

    if (!validateJsonStructure(jsonContent))
    {
        Serial.println("Invalid JSON structure in transferred file");
        cleanupMetaJsonTransfer();
        return false;
    }

    String backupPath = String(META_JSON_PATH) + ".bak";

    // Backup existing meta.json if it exists
    if (SD.exists(META_JSON_PATH))
    {
        if (SD.exists(backupPath))
        {
            SD.remove(backupPath);
        }
        SD.rename(META_JSON_PATH, backupPath);
    }

    // Replace meta.json with new file
    if (!SD.rename(tempMetaJsonPath, META_JSON_PATH))
    {
        Serial.println("Failed to rename temporary file to meta.json");
        cleanupMetaJsonTransfer();
        return false;
    }

    // Remove backup file after successful transfer
    if (SD.exists(backupPath))
    {
        SD.remove(backupPath);
        Serial.println("Removed backup meta.json file");
    }

    metaJsonTransferInProgress = false;
    return true;
}

void Hublink::cleanupMetaJsonTransfer()
{
    if (tempMetaJsonFile)
    {
        tempMetaJsonFile.close();
    }

    if (SD.exists(tempMetaJsonPath))
    {
        SD.remove(tempMetaJsonPath);
    }

    // Reset all meta.json related state
    metaJsonTransferInProgress = false;
    lastMetaJsonId = 0;
    metaJsonLastChunkTime = 0;

    // Clear the temporary path
    tempMetaJsonPath = "";
}

void Hublink::handleMetaJsonChunk(uint32_t id, const String &data)
{
    Serial.printf("Meta.json chunk received - ID: %d, Transfer in progress: %s, Last ID: %d\n",
                  id, metaJsonTransferInProgress ? "yes" : "no", lastMetaJsonId);

    // Check for timeout
    if (metaJsonTransferInProgress &&
        (millis() - metaJsonLastChunkTime > META_JSON_TIMEOUT_MS))
    {
        Serial.printf("Meta.json transfer timed out (Last successful ID: %d)\n", lastMetaJsonId);
        cleanupMetaJsonTransfer();
        return;
    }

    // Handle EOF
    if (id == 0 && data == "EOF")
    {
        if (!metaJsonTransferInProgress)
        {
            Serial.println("Ignoring EOF - no active transfer");
            return;
        }

        if (lastMetaJsonId == 0)
        {
            Serial.println("Ignoring EOF - no chunks processed");
            cleanupMetaJsonTransfer();
            return;
        }

        if (finalizeMetaJsonTransfer())
        {
            Serial.println("Meta.json transfer completed successfully");
        }
        else
        {
            Serial.println("Failed to finalize meta.json transfer");
            cleanupMetaJsonTransfer();
        }
        return;
    }

    // Handle regular chunks
    if (!metaJsonTransferInProgress)
    {
        if (id == 1)
        {
            Serial.println("Starting new meta.json transfer");
            if (!beginMetaJsonTransfer())
            {
                Serial.println("Failed to begin meta.json transfer");
                return;
            }
        }
        else
        {
            Serial.printf("Unexpected chunk ID %d when no transfer in progress\n", id);
            return;
        }
    }

    // Verify sequence
    if (id != lastMetaJsonId + 1)
    {
        Serial.printf("Invalid sequence: expected %d, got %d\n", lastMetaJsonId + 1, id);
        cleanupMetaJsonTransfer();
        return;
    }

    // Process chunk
    if (processMetaJsonChunk(data))
    {
        lastMetaJsonId = id;
        Serial.printf("Successfully processed chunk %d\n", id);
    }
    else
    {
        Serial.println("Failed to process chunk");
        cleanupMetaJsonTransfer();
    }
}
