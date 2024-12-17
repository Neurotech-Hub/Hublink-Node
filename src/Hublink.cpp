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

bool Hublink::init(String defaultAdvName, bool allowOverride)
{
    // Set CPU frequency to minimum required for radio operation
    setCPUFrequency(CPUFrequency::MHz_80);

    // 2. Initialize SD
    if (!initSD())
    {
        Serial.println("✗ SD Card.");
        return false;
    }
    Serial.println("✓ SD Card.");

    // 3. Read meta.json and set advertising name
    metaJson = readMetaJson();
    if (allowOverride && configuredAdvName.length() > 0)
    {
        advName = configuredAdvName;
    }
    else
    {
        advName = defaultAdvName;
    }

    lastHublinkMillis = millis();
    return true;
}

String Hublink::readMetaJson()
{
    if (!initSD())
    {
        Serial.println("Failed to initialize SD card when reading meta.json");
        return "";
    }

    File configFile = SD.open(META_JSON_PATH, FILE_READ);
    if (!configFile)
    {
        Serial.println("No meta.json file found");
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
        configuredAdvName = hublink["advertise"].as<String>();
    }

    if (hublink.containsKey("advertise_every") && hublink.containsKey("advertise_for"))
    {
        uint32_t newEvery = hublink["advertise_every"];
        uint32_t newFor = hublink["advertise_for"];

        if (newEvery > 0 && newFor > 0)
        {
            bleConnectEvery = newEvery;
            bleConnectFor = newFor;
            Serial.printf("Updated intervals: every=%d, for=%d\n", bleConnectEvery, bleConnectFor);
        }
    }

    if (hublink.containsKey("disable"))
    {
        disable = hublink["disable"].as<bool>();
        Serial.printf("BLE disable flag set to: %s\n", disable ? "true" : "false");
    }

    // Convert the hublink object to a string for BLE characteristic
    serializeJson(doc, content);
    return content;
}

void Hublink::setCPUFrequency(CPUFrequency freq_mhz)
{
    setCpuFrequencyMhz(static_cast<uint32_t>(freq_mhz));
}

void Hublink::startAdvertising()
{
    NimBLEDevice::init(advName.c_str());

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

    pNodeCharacteristic->setValue(metaJson.c_str());

    pService->start();

    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    NimBLEAdvertisementData scanResponse;
    scanResponse.setName(advName.c_str());
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
bool Hublink::initSD()
{
    return SD.begin(cs, SPI, clkFreq);
}

void Hublink::sendAvailableFilenames()
{
    if (!initSD())
    {
        Serial.println("Failed to initialize SD card");
        return;
    }

    File root = SD.open("/");
    String accumulatedFileInfo = ""; // Accumulate file info here
    while (deviceConnected)
    {
        watchdogTimer = millis(); // Reset watchdog timer
        File entry = root.openNextFile();
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
    root.close();
}

void Hublink::handleFileTransfer(String fileName)
{
    // Begin SD communication with updated parameters
    if (!initSD())
    {
        Serial.println("Failed to initialize SD card");
        return;
    }

    File file = SD.open("/" + fileName);
    if (!file)
    {
        Serial.printf("Failed to open file: %s\n", fileName.c_str());
        return;
    }

    uint8_t buffer[mtuSize];
    while (file.available() && deviceConnected)
    {
        watchdogTimer = millis();
        int bytesRead = file.read(buffer, mtuSize);

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
    file.close();
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
    watchdogTimer = millis();
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
    allFilesSent = false;
    sendFilenames = false;
    watchdogTimer = 0;
}

void Hublink::updateMtuSize()
{
    mtuSize = NimBLEDevice::getMTU() - MTU_HEADER_SIZE;
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
    Serial.println("Config: Parsing JSON: " + configStr);

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

    // Handle boolean values specifically
    if (doc[key].is<bool>())
    {
        return doc[key].as<bool>() ? "true" : "false";
    }

    return doc[key].as<String>();
}

void Hublink::sleep(uint64_t milliseconds)
{
    esp_sleep_enable_timer_wakeup(milliseconds * 1000); // Convert to microseconds
    esp_light_sleep_start();
    delay(10); // wakeup delay
}

void Hublink::doBLE()
{
    printMemStats("BLE Start");

    startAdvertising();
    unsigned long subLoopStartTime = millis();
    bool didConnect = false;

    // update connection status
    while ((millis() - subLoopStartTime < bleConnectFor * 1000 && !didConnect) || deviceConnected)
    {
        // Watchdog timer
        if (deviceConnected && (millis() - watchdogTimer > WATCHDOG_TIMEOUT_MS))
        {
            Serial.println("Hublink node timeout detected, disconnecting...");
            // Disconnect using the connection handle
            if (pServer->getConnectedCount() > 0)
            {
                NimBLEConnInfo connInfo = pServer->getPeerInfo(0); // Get first connected peer
                pServer->disconnect(connInfo.getConnHandle());
            }
            delay(10); // Give BLE stack time to clean up
        }

        // Handle file transfers when connected
        if (deviceConnected && !currentFileName.isEmpty())
        {
            Serial.print("Requested file: ");
            Serial.println(currentFileName);
            handleFileTransfer(currentFileName);
            currentFileName = "";
        }

        // once sendFilenames is true
        if (deviceConnected && currentFileName.isEmpty() && !allFilesSent && sendFilenames)
        {
            updateMtuSize();
            Serial.print("MTU Size (negotiated): ");
            Serial.println(mtuSize);
            Serial.println("Sending filenames...");
            sendAvailableFilenames();
        }

        didConnect |= deviceConnected;
        delay(100);
    }

    stopAdvertising();

    printMemStats("BLE End");
}

void Hublink::sync()
{
    unsigned long currentTime = millis();
    if (!disable && currentTime - lastHublinkMillis >= bleConnectEvery * 1000)
    {
        Serial.println("Hublink started advertising... ");
        doBLE();
        Serial.println("Done advertising.");
        lastHublinkMillis = millis();
    }
    else if (!disable)
    {
        unsigned long remainingTime = (bleConnectEvery * 1000 - (currentTime - lastHublinkMillis)) / 1000;
        Serial.printf("Time until next BLE cycle: %lu seconds\n", remainingTime);
    }
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

void Hublink::setValidExtensions(const std::vector<String> &extensions)
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
        indicationSuccess = (code == BLE_HS_EDONE);
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
            delay(10);
        }
        return indicationSuccess;
    }
};

bool Hublink::sendIndication(NimBLECharacteristic *pChar, const uint8_t *data, size_t length)
{
    if (!deviceConnected)
        return false;

    static HublinkIndicationCallbacks indicationCallbacks;
    NimBLECharacteristicCallbacks *originalCallbacks = pChar->getCallbacks();

    pChar->setCallbacks(&indicationCallbacks);
    pChar->setValue(data, length);

    const int maxRetries = 3;
    const unsigned long timeout = 1000; // 1 second timeout
    bool success = false;

    for (int i = 0; i < maxRetries && !success; i++)
    {
        indicationCallbacks.reset();

        if (!pChar->indicate())
        {
            Serial.printf("Indication attempt %d failed to start\n", i + 1);
            delay(100);
            continue;
        }

        success = indicationCallbacks.waitForComplete(timeout);
        if (!success)
        {
            Serial.printf("Indication attempt %d failed or timed out\n", i + 1);
            delay(100);
        }
    }

    // Restore original callbacks
    pChar->setCallbacks(originalCallbacks);
    return success;
}
