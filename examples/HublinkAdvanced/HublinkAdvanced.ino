#include <Hublink.h>

// Pin definitions
const int cs = A0; // Chip select pin for SD card

/*
 * Hublink Callback System
 * ======================
 * The Hublink library uses three types of callbacks to handle BLE events:
 *
 * 1. Server Callbacks: Handle device connection/disconnection events
 * 2. Filename Callbacks: Process incoming filename requests
 * 3. Gateway Callbacks: Handle gateway commands (like requesting file lists)
 *
 * While the library provides default callbacks, you can customize behavior by
 * creating your own callback classes that inherit from BLEServerCallbacks
 * and BLECharacteristicCallbacks. Each callback class needs a pointer to
 * the Hublink instance to access shared functionality.
 *
 * The callbacks are registered using hublink.setBLECallbacks() during setup.
 */

// Custom server callbacks class
class MyServerCallbacks : public BLEServerCallbacks
{
private:
    Hublink *hublink; // Pointer to Hublink instance

public:
    MyServerCallbacks(Hublink *hl) : hublink(hl) {}

    // Called when a device connects to the BLE server
    void onConnect(BLEServer *pServer)
    {
        Serial.println("Custom: Device connected!");
        hublink->onConnect();
    }

    // Called when a device disconnects from the BLE server
    void onDisconnect(BLEServer *pServer)
    {
        Serial.println("Custom: Device disconnected!");
        hublink->onDisconnect();
    }
};

// Custom filename callbacks class
class MyFilenameCallbacks : public BLECharacteristicCallbacks
{
private:
    Hublink *hublink;

public:
    MyFilenameCallbacks(Hublink *hl) : hublink(hl) {}

    // Called when the filename characteristic is written to
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        String filename = String(pCharacteristic->getValue().c_str());
        Serial.println("Custom: Filename requested: " + filename);
        hublink->currentFileName = filename;
    }
};

// Custom gateway callbacks class
class MyGatewayCallbacks : public BLECharacteristicCallbacks
{
private:
    Hublink *hublink;

public:
    MyGatewayCallbacks(Hublink *hl) : hublink(hl) {}

    // Called when the gateway characteristic is written to
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        String rtc = hublink->parseGateway(pCharacteristic, "rtc");
        if (rtc.length() > 0)
        {
            Serial.println("Gateway settings received:");
            Serial.println("rtc: " + rtc);
        }
        hublink->sendFilenames = true; // do not remove
    }
};

// Create Hublink instance
Hublink hublink(cs);

// Create custom callback instances
MyServerCallbacks *serverCallbacks;
MyFilenameCallbacks *filenameCallbacks;
MyGatewayCallbacks *gatewayCallbacks;

void setup()
{
    // Initialize serial communication
    Serial.begin(9600);
    delay(1000);
    Serial.println("Hello, Advanced Hublink.");

    // Initialize SPI for SD card
    SPI.begin(SCK, MISO, MOSI, cs);
    Serial.println(hublink.initSD() ? "✓ SD Card." : "✗ SD Card");

    // Create callback instances
    serverCallbacks = new MyServerCallbacks(&hublink);
    filenameCallbacks = new MyFilenameCallbacks(&hublink);
    gatewayCallbacks = new MyGatewayCallbacks(&hublink);

    // Initialize Hublink with custom advertising name
    hublink.init("HublinkAdvanced");

    // Set custom callbacks
    hublink.setBLECallbacks(
        serverCallbacks,
        filenameCallbacks,
        gatewayCallbacks);
}

void loop()
{
    // Handle BLE operations when ready
    hublink.sync();

    // Optional: Put device into light sleep between operations
    hublink.sleep(1000);
}
