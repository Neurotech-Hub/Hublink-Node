#include <HublinkNode_ESP32.h>
#include <SPI.h>
#include <SD.h>

const int cs = A0;

// ======== HUBLINK_HEADER_START ========
HublinkNode_ESP32 hublinkNode(cs);  // optional (cs, clkFreq) parameters
unsigned long lastBleEntryTime = 0; // Tracks the last time we entered the BLE sub-loop

class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer) override
  {
    hublinkNode.onConnect();
  }

  void onDisconnect(BLEServer *pServer) override
  {
    hublinkNode.onDisconnect();
  }
};

class FilenameCallback : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    hublinkNode.currentFileName = String(pCharacteristic->getValue().c_str());
    if (hublinkNode.currentFileName != "")
    {
      hublinkNode.fileTransferInProgress = true;
    }
  }
};

class GatewayCallback : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic) override
  {
    String rtc = hublinkNode.parseGateway(pCharacteristic, "rtc");

    // Only set gatewayChanged if we got valid values
    if (rtc.length() > 0)
    {
      hublinkNode.gatewayChanged = true; // true for any change, triggers sending available filenames
      Serial.println("Gateway settings received:");
      Serial.println("rtc: " + rtc);
    }
    else
    {
      Serial.println("No valid gateway settings found");
    }
  }
};
// ======== HUBLINK_HEADER_END ========

// ======== HUBLINK_BLE_ACCESSORY_START ========
void enterBleSubLoop()
{
  Serial.println("Entering BLE sub-loop.");
  BLEDevice::getAdvertising()->start();
  unsigned long subLoopStartTime = millis();
  bool connectedInitially = false;

  while ((millis() - subLoopStartTime < hublinkNode.bleConnectFor * 1000 && !connectedInitially) || hublinkNode.deviceConnected)
  {
    hublinkNode.updateConnectionStatus(); // Update connection and watchdog timeout

    // If the device just connected, mark it as initially connected
    if (hublinkNode.deviceConnected)
    {
      connectedInitially = true;
    }

    delay(100); // Avoid busy waiting
  }

  BLEDevice::getAdvertising()->stop();
  Serial.println("Leaving BLE sub-loop.");
}
// ======== HUBLINK_BLE_ACCESSORY_END ========

void setup()
{
  Serial.begin(9600);
  delay(2000);
  Serial.println("Hello, hublink node.");

  // Setup SD card with proper pin definitions
  SPI.begin(SCK, MISO, MOSI, cs);
  if (!SD.begin(cs, SPI, 1000000))
  {
    Serial.println("SD Card initialization failed!");
    while (1)
    { // Optional: halt if SD fails
      Serial.println("Retrying SD init...");
      if (SD.begin(cs, SPI, 1000000))
        break;
      delay(1000);
    }
  }
  Serial.println("SD Card initialized.");

  // ======== HUBLINK_SETUP_START ========
  hublinkNode.initBLE("ESP32_BLE_SD"); // !! make optional from hublink.node file
  hublinkNode.setBLECallbacks(new ServerCallbacks(),
                              new FilenameCallback(),
                              new GatewayCallback());
  hublinkNode.setNodeChar();
  // ======== HUBLINK_SETUP_END ========

  Serial.println("BLE setup done, looping....");
}

void loop()
{
  // ======== HUBLINK_LOOP_START ========
  unsigned long currentTime = millis();

  // Check if it's time to enter the BLE sub-loop and not disabled
  if (!hublinkNode.disable && currentTime - lastBleEntryTime >= hublinkNode.bleConnectEvery * 1000)
  {
    enterBleSubLoop();
    lastBleEntryTime = millis();
  }
  // ======== HUBLINK_LOOP_END ========
}
