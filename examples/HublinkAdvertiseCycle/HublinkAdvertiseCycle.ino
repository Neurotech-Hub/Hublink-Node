#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>
#include <BLEUtils.h>

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

int lastMinFreeHeap = 0;
BLEServer *pServer = nullptr;
BLEService *pService = nullptr;
BLECharacteristic *pCharacteristic = nullptr;

void printMemoryStats(const char *label)
{
    int currentMinFreeHeap = ESP.getMinFreeHeap();
    int heapDiff = currentMinFreeHeap - lastMinFreeHeap;

    Serial.printf("%s - Min free heap: %d bytes (Change: %d bytes)\n",
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
    pCharacteristic = pService->createCharacteristic(
        BLEUUID(CHARACTERISTIC_UUID),
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
    if (pServer != nullptr)
    {
        pServer = nullptr;
    }
    BLEDevice::deinit(false); // must be false!!
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
}

void loop()
{
    printMemoryStats("Before BLE Start");
    startBLE();
    delay(200);
    printMemoryStats("After BLE Start");

    stopBLE();
    printMemoryStats("After BLE Stop");

    delay(200);
}
