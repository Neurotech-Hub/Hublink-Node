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
    Serial.println("Restarting BLE advertising...");
    BLEDevice::getAdvertising()->start();
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

class ConfigCallback : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic) override
  {
    String bleConnectEvery = hublinkNode.parseConfig(pCharacteristic, "BLE_CONNECT_EVERY");
    String bleConnectFor = hublinkNode.parseConfig(pCharacteristic, "BLE_CONNECT_FOR");
    String rtc = hublinkNode.parseConfig(pCharacteristic, "rtc");

    // Only set configChanged if we got valid values
    if (bleConnectEvery.length() > 0 || bleConnectFor.length() > 0 || rtc.length() > 0)
    {
      hublinkNode.configChanged = true;
      Serial.println("Config changed received:");
      if (bleConnectEvery.length() > 0)
      {
        hublinkNode.bleConnectEvery = bleConnectEvery.toInt();
        Serial.println("BLE_CONNECT_EVERY: " + String(hublinkNode.bleConnectEvery));
      }
      if (bleConnectFor.length() > 0)
      {
        hublinkNode.bleConnectFor = bleConnectFor.toInt();
        Serial.println("BLE_CONNECT_FOR: " + String(hublinkNode.bleConnectFor));
      }
      if (rtc.length() > 0)
        Serial.println("rtc: " + rtc);
    }
    else
    {
      Serial.println("No valid config values found");
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
  // Initialize serial for debugging
  Serial.begin(9600);
  delay(2000); // Allow time for serial initialization
  Serial.println("Hello, hublink node.");

  // Setup SD card manually in user sketch
  SPI.begin(SCK, MISO, MOSI, cs);
  while (!SD.begin(cs, SPI, 1000000))
  {
    Serial.println("SD Card initialization failed!");
    delay(500);
  }
  Serial.println("SD Card initialized.");

  // ======== HUBLINK_SETUP_START ========
  hublinkNode.initBLE("ESP32_BLE_SD");
  hublinkNode.setBLECallbacks(new ServerCallbacks(),
                              new FilenameCallback(),
                              new ConfigCallback());
  BLEDevice::getAdvertising()->start();
  // ======== HUBLINK_SETUP_END ========

  Serial.println("BLE setup done, looping....");
}

void loop()
{
  // ======== HUBLINK_LOOP_START ========
  unsigned long currentTime = millis();

  // Check if it's time to enter the BLE sub-loop
  if (currentTime - lastBleEntryTime >= hublinkNode.bleConnectEvery * 1000)
  {
    enterBleSubLoop();
    lastBleEntryTime = millis();
  }
  // ======== HUBLINK_LOOP_END ========
}
