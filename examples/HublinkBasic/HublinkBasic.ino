#include <Hublink.h>

const int cs = A0;
Hublink hublink(cs);  // Use default Hublink instance

void setup() {
  Serial.begin(9600);
  delay(1000);

  // initialize SPI for SD card
  // SPI.begin(SCK, MISO, MOSI, cs); // if not using default SCK, MISO, MOSI
  if (hublink.begin()) {
    Serial.println("✓ Hublink.");
  } else {
    Serial.println("✗ Failed.");
    while (1) {
    }
  }

  // hublink.sync(10); // force sync to modify meta.json
}

void loop() {
  hublink.sync();  // only blocks when ready
  // hublink.sleep(1000); // optional light sleep
  delay(1000);
}