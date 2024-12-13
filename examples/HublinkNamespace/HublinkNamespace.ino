#include "HublinkBLE.h"

#define SERVICE_UUID "57617368-5501-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_FILENAME "57617368-5502-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_FILETRANSFER "57617368-5503-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_GATEWAY "57617368-5504-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_NODE "57617368-5505-0001-8000-00805f9b34fb"

int lastMinFreeHeap = 0;
Hublink::BLEServer *pServer = nullptr;
Hublink::BLEService *pService = nullptr;
Hublink::BLECharacteristic *pFilenameCharacteristic;
Hublink::BLECharacteristic *pFileTransferCharacteristic;
Hublink::BLECharacteristic *pConfigCharacteristic;
Hublink::BLECharacteristic *pNodeCharacteristic;

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
    Serial.println("Starting BLE...");
    Hublink::BLEDevice::init("ESP32_TEST");

    Serial.println("Creating server...");
    pServer = Hublink::BLEDevice::createServer();
    if (!pServer)
    {
        Serial.println("Failed to create server!");
        return;
    }

    Serial.println("Creating service...");
    pService = pServer->createService(SERVICE_UUID);
    if (!pService)
    {
        Serial.println("Failed to create service!");
        return;
    }

    Serial.println("Creating filename characteristic...");
    pFilenameCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_FILENAME,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_INDICATE);
    if (!pFilenameCharacteristic)
    {
        Serial.println("Failed to create filename characteristic!");
        return;
    }

    Serial.println("Adding descriptor to filename characteristic...");
    pFilenameCharacteristic->addDescriptor(new Hublink::BLE2902());

    Serial.println("Creating file transfer characteristic...");
    pFileTransferCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_FILETRANSFER,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_INDICATE);
    if (!pFileTransferCharacteristic)
    {
        Serial.println("Failed to create file transfer characteristic!");
        return;
    }

    Serial.println("Adding descriptor to file transfer characteristic...");
    pFileTransferCharacteristic->addDescriptor(new Hublink::BLE2902());

    Serial.println("Creating config characteristic...");
    pConfigCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_GATEWAY,
        BLECharacteristic::PROPERTY_WRITE);
    if (!pConfigCharacteristic)
    {
        Serial.println("Failed to create config characteristic!");
        return;
    }

    Serial.println("Creating node characteristic...");
    pNodeCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_NODE,
        BLECharacteristic::PROPERTY_READ);
    if (!pNodeCharacteristic)
    {
        Serial.println("Failed to create node characteristic!");
        return;
    }

    Serial.println("Starting service...");
    pService->start();

    Serial.println("Getting advertising handle...");
    BLEAdvertising *pAdvertising = Hublink::BLEDevice::getAdvertising();
    if (!pAdvertising)
    {
        Serial.println("Failed to get advertising handle!");
        return;
    }

    Serial.println("Adding service UUID to advertising...");
    pAdvertising->addServiceUUID(SERVICE_UUID);

    Serial.println("Starting advertising...");
    pAdvertising->start();
    Serial.println("BLE startup complete");
}

void stopBLE()
{
    // First get our counts for debugging
    Serial.printf("Before cleanup - Servers: %d, Services: %d, Characteristics: %d, Descriptors: %d\n",
                  Hublink::serverCount, Hublink::serviceCount, Hublink::characteristicCount, Hublink::descriptorCount);

    // Stop all active operations first
    Serial.println("Stopping advertising...");
    Hublink::BLEDevice::stopAdvertising();
    delay(10);

    // Clean up characteristics first (which will clean up descriptors)
    Serial.println("Cleaning up characteristics...");
    pFilenameCharacteristic = nullptr;
    pFileTransferCharacteristic = nullptr;
    pConfigCharacteristic = nullptr;
    pNodeCharacteristic = nullptr;

    // Then clean up service (which will trigger characteristic destructors)
    Serial.println("Cleaning up service...");
    if (pService != nullptr)
    {
        // delete pService;
        pService = nullptr;
    }

    // Then clean up server (which will trigger service destructors)
    Serial.println("Cleaning up server...");
    if (pServer != nullptr)
    {
        // delete pServer;
        pServer = nullptr;
    }

    // Now clean up core BLE
    Serial.println("Cleaning up core BLE...");
    delay(100); // keep connection alive
    ::BLEDevice::deinit(false);
    delay(100);

    // Finally reset wrapper state
    Serial.println("Resetting wrapper state...");
    Hublink::BLEDevice::deinit(false);
    delay(100);

    Serial.printf("After cleanup - Servers: %d, Services: %d, Characteristics: %d, Descriptors: %d\n",
                  Hublink::serverCount, Hublink::serviceCount, Hublink::characteristicCount, Hublink::descriptorCount);

    Serial.println("BLE cleanup complete");
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n\NAMESPACE-----------------");
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
