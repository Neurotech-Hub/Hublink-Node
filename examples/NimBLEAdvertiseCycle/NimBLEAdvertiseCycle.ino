#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>

#define SERVICE_UUID "57617368-5501-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_FILENAME "57617368-5502-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_FILETRANSFER "57617368-5503-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_GATEWAY "57617368-5504-0001-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_NODE "57617368-5505-0001-8000-00805f9b34fb"

int lastMinFreeHeap = 0;
NimBLEServer *pServer = nullptr;
NimBLEService *pService = nullptr;
NimBLECharacteristic *pFilenameCharacteristic;
NimBLECharacteristic *pFileTransferCharacteristic;
NimBLECharacteristic *pConfigCharacteristic;
NimBLECharacteristic *pNodeCharacteristic;

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

    // Create service
    pService = pServer->createService(SERVICE_UUID);

    // Create characteristics
    pFilenameCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_FILENAME,
        NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::WRITE |
            NIMBLE_PROPERTY::INDICATE);

    pFileTransferCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_FILETRANSFER,
        NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::INDICATE);

    pConfigCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_GATEWAY,
        NIMBLE_PROPERTY::WRITE);

    pNodeCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_NODE,
        NIMBLE_PROPERTY::READ);

    // Start service
    pService->start();

    // Start advertising
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);

    // Set up scan response data
    NimBLEAdvertisementData scanResponse;
    scanResponse.setName("ESP32_TEST");
    pAdvertising->setScanResponseData(scanResponse);

    pAdvertising->start();
}

void stopBLE()
{
    if (pService != nullptr)
    {
        // NimBLE doesn't need explicit service stop
        pService = nullptr;
    }

    // Stop advertising
    NimBLEDevice::getAdvertising()->stop();

    // NimBLE handles cleanup differently
    NimBLEDevice::deinit(true);
    pServer = nullptr;
}

void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    delay(1000);
    Serial.println("\n\n\n----------------------------- NIMBLE ADV CYCLE -----------------------------");
    Serial.println("Press Enter to continue...");

    // Flash LED while waiting for input
    while (!Serial.available())
    {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
    }

    // Clear any received characters
    while (Serial.available())
    {
        Serial.read();
    }

    // Ensure LED is off before continuing
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
