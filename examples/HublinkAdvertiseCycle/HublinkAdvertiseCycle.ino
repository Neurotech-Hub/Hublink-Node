#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID "57617368-5501-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_FILENAME "57617368-5502-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_FILETRANSFER "57617368-5503-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_GATEWAY "57617368-5504-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_NODE "57617368-5505-0001-8000-00805f9b34fb"

int lastMinFreeHeap = 0;
BLEServer *pServer = nullptr;
BLEService *pService = nullptr;
BLECharacteristic *pFilenameCharacteristic;
BLECharacteristic *pFileTransferCharacteristic;
BLECharacteristic *pConfigCharacteristic;
BLECharacteristic *pNodeCharacteristic;

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
    BLEDevice::init("ESP32_TEST"); // no leaks

    // Create server
    pServer = BLEDevice::createServer(); // fixed leak! BLEDevice.cpp

    // Create service
    pService = pServer->createService(BLEUUID(SERVICE_UUID)); // fixed leak! BLEServer.cpp

    // Create characteristic, no leaks
    pFilenameCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_FILENAME,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_INDICATE);
    pFilenameCharacteristic->addDescriptor(new BLE2902()); // !! fixed leak! BLE2902.cpp, BLECharacteristic.cpp

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

    // Start service
    pService->start(); // no leaks

    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising(); // no leaks
    pAdvertising->addServiceUUID(SERVICE_UUID);                 // fixed leak! BLEAdvertising.cpp
    pAdvertising->start();
}

void stopBLE()
{
    if (pService != nullptr)
    {
        pService->stop();
        pService = nullptr;
    }
    pServer = nullptr;
    BLEDevice::deinit(false); // must be false!!
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n\nSTARTUP-----------------------------");
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
