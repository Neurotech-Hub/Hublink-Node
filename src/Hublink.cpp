#include "Hublink.h"
#include "esp_system.h"

Hublink::Hublink(uint8_t chipSelect, uint32_t clockFrequency) : cs(chipSelect), clkFreq(clockFrequency),
                                                                piReadyForFilenames(false), deviceConnected(false),
                                                                currentFileName(""), allFilesSent(false),
                                                                watchdogTimer(0), sendFilenames(false) {}

const char *Hublink::DEFAULT_NAME = "HUBNODE";

String Hublink::setupNode()
{
    // Set CPU frequency to 80MHz
    setCpuFrequencyMhz(80);

    if (!initializeSD())
    {
        Serial.println("Failed to initialize SD card when reading node content");
        return "";
    }

    File nodeFile = SD.open("/hublink.node", FILE_READ);
    if (!nodeFile)
    {
        Serial.println("No hublink.node file found");
        return "";
    }

    String content = "";
    String currentLine = "";
    disable = false; // Reset disable flag before parsing

    while (nodeFile.available())
    {
        char c = nodeFile.read();
        if (c == '\n' || c == '\r')
        {
            if (currentLine.length() > 0)
            {
                processLine(currentLine, content);
            }
            currentLine = "";
        }
        else
        {
            currentLine += c;
        }
    }

    if (currentLine.length() > 0)
    {
        processLine(currentLine, content);
    }

    nodeFile.close();
    return content;
}

void Hublink::init(String defaultAdvName, bool allowOverride)
{
    // Read node content first
    nodeContent = setupNode();

    // Determine advertising name
    String advertisingName = defaultAdvName;
    if (allowOverride && configuredAdvName.length() > 0)
    {
        advertisingName = configuredAdvName;
    }

    // Initialize BLE
    BLEDevice::init(advertisingName);
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

    pConfigCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_GATEWAY,
        BLECharacteristic::PROPERTY_WRITE);

    pNodeCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_NODE,
        BLECharacteristic::PROPERTY_READ);

    pService->start();
    setNodeChar();
}

// void Hublink::deinitBLE()
// {
//     Serial.printf("Free heap before deinit: %d\n", ESP.getFreeHeap());

//     // Disconnect any active connections
//     if (pServer && deviceConnected)
//     {
//         pServer->disconnect(pServer->getConnId());
//     }

//     // No need to clean up callbacks since they're global objects now

//     // Deinitialize BLE stack
//     BLEDevice::deinit(true);

//     Serial.printf("Free heap after deinit: %d\n", ESP.getFreeHeap());
// }

void Hublink::startAdvertising()
{
    resetBLEState();
    BLEDevice::getAdvertising()->start();
}

void Hublink::stopAdvertising()
{
    BLEDevice::getAdvertising()->stop();
}

// Use SD.begin(cs, SPI, clkFreq) whenever SD functions are needed in this way:
bool Hublink::initializeSD()
{
    if (!SD.begin(cs, SPI, clkFreq))
    {
        Serial.println("Failed to initialize SD in Hublink library");
        return false;
    }
    return true;
}

void Hublink::setBLECallbacks(BLEServerCallbacks *newServerCallbacks,
                              BLECharacteristicCallbacks *newFilenameCallbacks,
                              BLECharacteristicCallbacks *newConfigCallbacks)
{
    // Simply set the callbacks without memory management
    pServer->setCallbacks(newServerCallbacks);
    pFilenameCharacteristic->setCallbacks(newFilenameCallbacks);
    pConfigCharacteristic->setCallbacks(newConfigCallbacks);
}

void Hublink::updateConnectionStatus()
{
    // Watchdog timer
    if (deviceConnected && (millis() - watchdogTimer > WATCHDOG_TIMEOUT_MS))
    {
        Serial.println("Hublink node timeout detected, disconnecting...");
        pServer->disconnect(pServer->getConnId());
    }

    if (deviceConnected && !currentFileName.isEmpty())
    {
        Serial.print("Requested file: ");
        Serial.println(currentFileName);
        handleFileTransfer(currentFileName);
        currentFileName = "";
    }

    if (deviceConnected && currentFileName.isEmpty() && !allFilesSent && sendFilenames)
    {
        updateMtuSize();
        Serial.print("MTU Size (negotiated): ");
        Serial.println(mtuSize);
        Serial.println("Sending filenames...");
        sendAvailableFilenames();
    }
}

void Hublink::sendAvailableFilenames()
{
    if (!initializeSD())
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
                pFilenameCharacteristic->setValue(chunk.c_str());
                pFilenameCharacteristic->indicate();
                index += mtuSize;
            }
            // Send "EOF" as a separate indication to signal the end
            pFilenameCharacteristic->setValue("EOF");
            pFilenameCharacteristic->indicate();
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
    if (!initializeSD())
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
            pFileTransferCharacteristic->setValue(buffer, bytesRead);
            pFileTransferCharacteristic->indicate();
        }
        else
        {
            Serial.println("Error reading from file.");
            break;
        }
    }
    pFileTransferCharacteristic->setValue("EOF");
    pFileTransferCharacteristic->indicate();
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
    BLEDevice::setMTU(NEGOTIATE_MTU_SIZE);
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
}

void Hublink::updateMtuSize()
{
    mtuSize = BLEDevice::getMTU() - MTU_HEADER_SIZE;
}

String Hublink::parseGateway(BLECharacteristic *pCharacteristic, const String &key)
{
    std::string value = pCharacteristic->getValue().c_str();
    if (value.empty())
    {
        Serial.println("Config: Empty characteristic value");
        return "";
    }

    String configStr = String(value.c_str());
    Serial.println("Config: Parsing string: " + configStr);
    int startPos = 0;
    int endPos;

    while ((endPos = configStr.indexOf(';', startPos)) != -1)
    {
        String pair = configStr.substring(startPos, endPos);
        int equalPos = pair.indexOf('=');

        if (equalPos != -1)
        {
            String currentKey = pair.substring(0, equalPos);
            String currentValue = pair.substring(equalPos + 1);

            if (currentKey == key && currentValue.length() > 0)
            {
                return currentValue;
            }
        }

        startPos = endPos + 1;
    }

    // Check the last pair if it doesn't end with semicolon
    String pair = configStr.substring(startPos);
    int equalPos = pair.indexOf('=');
    if (equalPos != -1)
    {
        String currentKey = pair.substring(0, equalPos);
        String currentValue = pair.substring(equalPos + 1);

        if (currentKey == key && currentValue.length() > 0)
        {
            return currentValue;
        }
    }

    return "";
}

void Hublink::setNodeChar()
{
    pNodeCharacteristic->setValue(nodeContent.c_str());
    // Serial.println("Node characteristic set to: " + nodeContent);
}

// Helper function to process each line
void Hublink::processLine(const String &line, String &nodeContent)
{
    if (line.length() > 0)
    {
        // Add to nodeContent
        if (nodeContent.length() > 0)
        {
            nodeContent += ";";
        }
        nodeContent += line;

        // Parse configuration
        int equalPos = line.indexOf('=');
        if (equalPos != -1)
        {
            String key = line.substring(0, equalPos);
            String value = line.substring(equalPos + 1);

            if (key == "advertise")
            {
                configuredAdvName = value;
            }
            else if (key == "interval")
            {
                int dashPos = value.indexOf(',');
                if (dashPos != -1)
                {
                    String everyStr = value.substring(0, dashPos);
                    String forStr = value.substring(dashPos + 1);

                    uint32_t newEvery = everyStr.toInt();
                    uint32_t newFor = forStr.toInt();

                    if (newEvery > 0 && newFor > 0)
                    {
                        bleConnectEvery = newEvery;
                        bleConnectFor = newFor;
                        Serial.printf("Updated intervals: every=%d, for=%d\n", bleConnectEvery, bleConnectFor);
                    }
                }
            }
            else if (key == "disable")
            {
                String valueLower = value;
                valueLower.toLowerCase();
                disable = (valueLower == "true" || value == "1");
                Serial.printf("BLE disable flag set to: %s\n", disable ? "true" : "false");
            }
        }
    }
}

void Hublink::sleep(uint64_t milliseconds)
{
    esp_sleep_enable_timer_wakeup(milliseconds * 1000); // Convert to microseconds
    esp_light_sleep_start();
}
