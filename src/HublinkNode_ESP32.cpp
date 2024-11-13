#include "HublinkNode_ESP32.h"

HublinkNode_ESP32::HublinkNode_ESP32(uint8_t chipSelect, uint32_t clockFrequency) : cs(chipSelect), clkFreq(clockFrequency),
                                                                                    piReadyForFilenames(false), deviceConnected(false),
                                                                                    fileTransferInProgress(false), currentFileName(""), allFilesSent(false),
                                                                                    watchdogTimer(0), gatewayChanged(false) {}

void HublinkNode_ESP32::initBLE(String advName)
{
    // Initialize BLE
    BLEDevice::init(advName);
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
    onDisconnect(); // clear all vars and notification flags
}

// Use SD.begin(cs, SPI, clkFreq) whenever SD functions are needed in this way:
bool HublinkNode_ESP32::initializeSD()
{
    if (!SD.begin(cs, SPI, clkFreq))
    {
        Serial.println("Failed to initialize SD in Hublink library");
        return false;
    }
    return true;
}

void HublinkNode_ESP32::setBLECallbacks(BLEServerCallbacks *serverCallbacks,
                                        BLECharacteristicCallbacks *filenameCallbacks,
                                        BLECharacteristicCallbacks *configCallbacks)
{
    // Store pointers for later cleanup
    serverCallbacks = serverCallbacks;
    filenameCallbacks = filenameCallbacks;
    configCallbacks = configCallbacks;

    // Set the callbacks
    pServer->setCallbacks(serverCallbacks);
    pFilenameCharacteristic->setCallbacks(filenameCallbacks);
    pConfigCharacteristic->setCallbacks(configCallbacks);
}

void HublinkNode_ESP32::updateConnectionStatus()
{
    // Watchdog timer
    if (deviceConnected && (millis() - watchdogTimer > WATCHDOG_TIMEOUT_MS))
    {
        Serial.println("Watchdog timeout detected, disconnecting...");
        pServer->disconnect(pServer->getConnId());
    }

    if (deviceConnected && fileTransferInProgress && currentFileName != "")
    {
        Serial.print("Requested file: ");
        Serial.println(currentFileName);
        handleFileTransfer(currentFileName);
        currentFileName = "";
        fileTransferInProgress = false;
    }

    // value=1 for notifications, =2 for indications
    // (pFilenameCharacteristic->getDescriptorByUUID(BLEUUID((uint16_t)0x2902))->getValue()[0] & 0x0F) > 0
    if (deviceConnected && !fileTransferInProgress && !allFilesSent && gatewayChanged)
    {
        updateMtuSize(); // after negotiation
        Serial.print("MTU Size (negotiated): ");
        Serial.println(mtuSize);
        Serial.println("Sending filenames...");
        sendAvailableFilenames();
    }
}

void HublinkNode_ESP32::sendAvailableFilenames()
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

void HublinkNode_ESP32::handleFileTransfer(String fileName)
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

bool HublinkNode_ESP32::isValidFile(String fileName)
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
    for (int i = 0; i < 3; i++)
    {
        if (lowerFileName.endsWith(validExtensions[i]))
        {
            return true;
        }
    }
    return false;
}

void HublinkNode_ESP32::onConnect()
{
    Serial.println("Hublink node connected.");
    deviceConnected = true;
    watchdogTimer = millis();
    BLEDevice::setMTU(NEGOTIATE_MTU_SIZE);
}

void HublinkNode_ESP32::onDisconnect()
{
    Serial.println("Hublink node reset.");
    if (pFilenameCharacteristic)
    {
        uint8_t disableNotifyValue[2] = {0x00, 0x00}; // 0x00 to disable notifications
        pFilenameCharacteristic->getDescriptorByUUID(BLEUUID((uint16_t)0x2902))->setValue(disableNotifyValue, 2);
    }
    deviceConnected = false;
    piReadyForFilenames = false;
    fileTransferInProgress = false;
    allFilesSent = false;
    gatewayChanged = false;

    // Clean up callbacks if they exist
    if (serverCallbacks)
    {
        delete serverCallbacks;
        serverCallbacks = nullptr;
    }
    if (filenameCallbacks)
    {
        delete filenameCallbacks;
        filenameCallbacks = nullptr;
    }
    if (configCallbacks)
    {
        delete configCallbacks;
        configCallbacks = nullptr;
    }
}

void HublinkNode_ESP32::updateMtuSize()
{
    mtuSize = BLEDevice::getMTU() - MTU_HEADER_SIZE;
}

String HublinkNode_ESP32::parseGateway(BLECharacteristic *pCharacteristic, const String &key)
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

void HublinkNode_ESP32::setNodeChar()
{
    Serial.println("Starting setNodeChar()...");
    if (!initializeSD())
    {
        Serial.println("Failed to initialize SD card when setting node characteristic");
        pNodeCharacteristic->setValue("");
        return;
    }
    Serial.println("SD card initialized successfully");

    File nodeFile = SD.open("/hublink.node", FILE_READ);
    if (!nodeFile)
    {
        Serial.println("No hublink.node file found");
        pNodeCharacteristic->setValue("");
        return;
    }
    Serial.println("Successfully opened hublink.node file");

    String nodeContent = "";
    String currentLine = "";

    // Initialize disable to false before parsing file
    disable = false;
    Serial.println("Starting to read file contents...");

    while (nodeFile.available())
    {
        char c = nodeFile.read();
        if (c == '\n' || c == '\r')
        {
            Serial.println("Processing line: " + currentLine);
            processLine(currentLine, nodeContent);
            currentLine = "";
        }
        else
        {
            currentLine += c;
        }
    }

    // Process the last line even if it doesn't end with a newline
    if (currentLine.length() > 0)
    {
        Serial.println("Processing final line: " + currentLine);
        processLine(currentLine, nodeContent);
    }

    pNodeCharacteristic->setValue(nodeContent.c_str());
    Serial.println("Node characteristic set to: " + nodeContent);
    nodeFile.close();
    Serial.println("Finished setNodeChar()");
}

// Helper function to process each line
void HublinkNode_ESP32::processLine(const String &line, String &nodeContent)
{
    if (line.length() > 0)
    {
        // Add to nodeContent with comma separator
        if (nodeContent.length() > 0)
        {
            nodeContent += ",";
        }
        nodeContent += line;

        // Parse the line
        int equalPos = line.indexOf('=');
        if (equalPos != -1)
        {
            String key = line.substring(0, equalPos);
            String value = line.substring(equalPos + 1);

            if (key == "interval")
            {
                int dashPos = value.indexOf('-');
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
