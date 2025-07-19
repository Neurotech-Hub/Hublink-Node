#include <Hublink.h>

const int cs = A0;
Hublink hublink(cs); // Use default Hublink instance

void setup()
{
  Serial.begin(9600);
  delay(1000);
  pinMode(LED_BUILTIN, OUTPUT);

  // initialize SPI for SD card
  // SPI.begin(SCK, MISO, MOSI, cs); // if not using default SCK, MISO, MOSI
  if (hublink.begin())
  {
    Serial.println("✓ Hublink.");
  }
  else
  {
    Serial.println("✗ Failed.");
    while (1)
    {
    }
  }

  // hublink.sync(10); // force sync to modify meta.json
}

void loop()
{
  digitalWrite(LED_BUILTIN, HIGH);
  hublink.setAlert("test");     // send alert to gateway, clears after sync
  hublink.setBatteryLevel(100); // send battery level to gateway
  hublink.sync();               // only blocks when ready
  digitalWrite(LED_BUILTIN, LOW);
  // hublink.sleep(1000); // optional light sleep
  delay(1000);
}