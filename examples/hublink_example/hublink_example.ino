#include <HublinkNode.h>
#include <SPI.h>
#include <SD.h>

const int cs = A0;

// ======== HUBLINK_HEADER_START ========
HublinkNode hublinkNode(cs);     // optional (cs, clkFreq) parameters
unsigned long lastBleEntryTime;  // Tracks the last time we entered the BLE sub-loop
String advName = "HUBNODE";

// First define the callback classes
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) override {
    hublinkNode.onConnect();
  }

  void onDisconnect(BLEServer *pServer) override {
    hublinkNode.onDisconnect();
  }
};

class FilenameCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    hublinkNode.currentFileName = String(pCharacteristic->getValue().c_str());
  }
};

class GatewayCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) override {
    String rtc = hublinkNode.parseGateway(pCharacteristic, "rtc");
    if (rtc.length() > 0) {
      Serial.println("Gateway settings received:");
      Serial.println("rtc: " + rtc);
    }
    hublinkNode.sendFilenames = true;
  }
};

// Callback instances
static ServerCallbacks serverCallbacks;
static FilenameCallback filenameCallback;
static GatewayCallback gatewayCallback;

void enterBleSubLoop() {
  Serial.println("Entering BLE sub-loop.");
  hublinkNode.startAdvertising();

  unsigned long subLoopStartTime = millis();
  bool didConnect = false;

  while ((millis() - subLoopStartTime < hublinkNode.bleConnectFor * 1000 && !didConnect) || hublinkNode.deviceConnected) {
    hublinkNode.updateConnectionStatus();
    didConnect |= hublinkNode.deviceConnected;
    delay(100);
  }

  hublinkNode.stopAdvertising();
  Serial.printf("Leaving BLE sub-loop. Free heap: %d\n", ESP.getFreeHeap());
}
// ======== HUBLINK_HEADER_END ========

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(9600);
  delay(2000);
  Serial.println("Hello, hublink node.");

  // Setup SD card with proper pin definitions
  SPI.begin(SCK, MISO, MOSI, cs);
  if (!SD.begin(cs, SPI, 1000000)) {
    Serial.println("SD Card initialization failed!");
    while (1) {  // Optional: halt if SD fails
      Serial.println("Retrying SD init...");
      if (SD.begin(cs, SPI, 1000000))
        break;
      delay(1000);
    }
  }
  Serial.println("SD Card initialized.");

  // One-time BLE initialization
  hublinkNode.init(advName, true);
  hublinkNode.setBLECallbacks(&serverCallbacks,
                              &filenameCallback,
                              &gatewayCallback);

  lastBleEntryTime = millis();  // init
}

void loop() {
  unsigned long currentTime = millis();

  if (!hublinkNode.disable && currentTime - lastBleEntryTime >= hublinkNode.bleConnectEvery * 1000) {
    enterBleSubLoop();
    lastBleEntryTime = millis();
  }
  Serial.printf("Entering sleep. Free heap: %d\n", ESP.getFreeHeap());
  hublinkNode.sleep(1000);
}
