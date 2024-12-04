#include <Hublink.h>
#include <SPI.h>
#include <SD.h>

const int cs = A0;

// ======== HUBLINK_HEADER_START ========
Hublink hublink(cs); // Rename instance
unsigned long lastBleEntryTime;
String advName = "HUBNODE";

// Update callback references
class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer) override
  {
    hublink.onConnect();
  }

  void onDisconnect(BLEServer *pServer) override
  {
    hublink.onDisconnect();
  }
};

class FilenameCallback : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    hublink.currentFileName = String(pCharacteristic->getValue().c_str());
  }
};

class GatewayCallback : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic) override
  {
    String rtc = hublink.parseGateway(pCharacteristic, "rtc");
    if (rtc.length() > 0)
    {
      Serial.println("Gateway settings received:");
      Serial.println("rtc: " + rtc);
    }
    hublink.sendFilenames = true;
  }
};

// Callback instances
static ServerCallbacks serverCallbacks;
static FilenameCallback filenameCallback;
static GatewayCallback gatewayCallback;

void enterBleSubLoop()
{
  Serial.println("Entering BLE sub-loop.");
  hublink.startAdvertising();

  unsigned long subLoopStartTime = millis();
  bool didConnect = false;

  while ((millis() - subLoopStartTime < hublink.bleConnectFor * 1000 && !didConnect) || hublink.deviceConnected)
  {
    hublink.updateConnectionStatus();
    didConnect |= hublink.deviceConnected;
    delay(100);
  }

  hublink.stopAdvertising();
  Serial.printf("Leaving BLE sub-loop. Free heap: %d\n", ESP.getFreeHeap());
}
// ======== HUBLINK_HEADER_END ========

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

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

  // One-time BLE initialization
  hublink.init(advName, true);
  hublink.setBLECallbacks(&serverCallbacks,
                          &filenameCallback,
                          &gatewayCallback);

  lastBleEntryTime = millis(); // init
}

void loop()
{
  unsigned long currentTime = millis();

  if (!hublink.disable && currentTime - lastBleEntryTime >= hublink.bleConnectEvery * 1000)
  {
    enterBleSubLoop();
    lastBleEntryTime = millis();
  }
  Serial.printf("Entering sleep. Free heap: %d\n", ESP.getFreeHeap());
  hublink.sleep(1000);
}
