#include <Hublink.h>

const int cs = A0;
Hublink hublink(cs); // Use default Hublink instance

void setup()
{
  Serial.begin(9600);
  delay(1000);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // initialize SPI for SD card
  SPI.begin(SCK, MISO, MOSI, cs);
  if (hublink.init())
  {
    Serial.println("✓ Hublink.");
  }
  else
  {
    Serial.println("✗ Failed.");
    Serial.flush();
    while (1)
    {
    }
  }
  Serial.flush();
}

void loop()
{
  hublink.sync(); // only blocks when ready
  Serial.flush(); // Add flush before sleep
  hublink.sleep(1000); // optional light sleep
}