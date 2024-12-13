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
    Serial.println("\n=== Starting BLE Setup ===");
    printMemoryStats("Before BLE Init");

    Hublink::BLEDevice::init("ESP32_TEST");
    printMemoryStats("After BLE Init");

    Serial.println("Creating server...");
    pServer = Hublink::BLEDevice::createServer();
    if (!pServer)
    {
        Serial.println("Failed to create server!");
        return;
    }
    printMemoryStats("After Server Creation");

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
    pFilenameCharacteristic->addDescriptor();

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
    pFileTransferCharacteristic->addDescriptor();

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

    Serial.println("=== BLE Setup Complete ===");
    printMemoryStats("Setup Complete");
}

void stopBLE()
{
    // Clear characteristics first
    pFilenameCharacteristic = nullptr;
    pFileTransferCharacteristic = nullptr;
    pConfigCharacteristic = nullptr;
    pNodeCharacteristic = nullptr;
    delay(20); // Let characteristic cleanup settle

    // Stop service before clearing
    if (pService)
    {
        pService->stop();
        delay(50); // Let service stop settle
        pService = nullptr;
    }

    // Clear server pointer
    pServer = nullptr;
    delay(20); // Let pointer cleanup settle

    // Let BLE stack handle cleanup with delays
    Hublink::BLEDevice::deinit(false);
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
