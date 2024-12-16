#include <NimBLEDevice.h>

static NimBLEServer *pServer;
int lastMinFreeHeap = 0;

// Simple server callbacks
class ServerCallbacks : public NimBLEServerCallbacks
{
    void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override
    {
        if (pServer != nullptr)
        {
            Serial.printf("Client connected: %s\n", connInfo.getAddress().toString().c_str());
        }
    }

    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override
    {
        if (pServer != nullptr)
        {
            Serial.println("Client disconnected");
        }
    }
} serverCallbacks;

// Simple characteristic callbacks
class CharCallbacks : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
    {
        Serial.printf("Characteristic written: %s\n", pCharacteristic->getValue().c_str());
    }
} charCallbacks;

void printMemoryStats(const char *label)
{
    int currentMinFreeHeap = ESP.getMinFreeHeap();
    int heapDiff = currentMinFreeHeap - lastMinFreeHeap;

    Serial.printf("[%lu] %s - Min free heap: %d bytes (Change: %d bytes)\n",
                  millis(),
                  label,
                  currentMinFreeHeap,
                  lastMinFreeHeap == 0 ? 0 : heapDiff);

    lastMinFreeHeap = currentMinFreeHeap;
}

void startBLE()
{
    NimBLEDevice::init("ESP32_TEST");

    // Create server
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);

    // Create service
    NimBLEService *pService = pServer->createService("DEAD");

    // Create characteristic
    NimBLECharacteristic *pCharacteristic =
        pService->createCharacteristic("BEEF",
                                       NIMBLE_PROPERTY::READ |
                                           NIMBLE_PROPERTY::WRITE |
                                           NIMBLE_PROPERTY::INDICATE);

    pCharacteristic->setValue("Hello");
    pCharacteristic->setCallbacks(&charCallbacks);

    // Start service
    pService->start();

    // Start advertising
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(pService->getUUID());

    // Set up scan response data
    NimBLEAdvertisementData scanResponse;
    scanResponse.setName("ESP32_TEST");
    pAdvertising->setScanResponseData(scanResponse);

    pAdvertising->start();
}

void stopBLE()
{
    if (pServer != nullptr)
    {
        // First disconnect any connected clients
        if (pServer->getConnectedCount() > 0)
        {
            pServer->disconnect(0);
            delay(50); // Give time for disconnect to complete
        }

        // Remove callbacks before deinit
        pServer->setCallbacks(nullptr);

        // Stop advertising
        NimBLEDevice::getAdvertising()->stop();
        delay(50);

        // Now deinit
        NimBLEDevice::deinit(true);
        pServer = nullptr;
    }
}

void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    delay(1000);
    Serial.println("\n\n\n----------------------------- NIMBLE ADV CYCLE -----------------------------");
    Serial.println("Press Enter to continue...");

    while (!Serial.available())
    {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
    }

    while (Serial.available())
        Serial.read();
    digitalWrite(LED_BUILTIN, LOW);
}

void loop()
{
    printMemoryStats("Before BLE Start");
    startBLE();
    printMemoryStats("After BLE Start");

    delay(1000); // advertise

    stopBLE();
    printMemoryStats("After BLE Stop");

    delay(1000);
}
