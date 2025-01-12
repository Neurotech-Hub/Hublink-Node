#include <Hublink.h>

const int cs = A0;
Hublink hublink(cs); // Use default Hublink instance

void printMemory(const char *label)
{
  Serial.printf("[MEM-%s] Free: %lu, Min: %lu, MaxBlock: %lu, PSRAM: %lu\n",
                label,
                ESP.getFreeHeap(),
                ESP.getMinFreeHeap(),
                ESP.getMaxAllocHeap(),
                ESP.getFreePsram());
  Serial.flush();
}

void setup()
{
  Serial.begin(9600);
  delay(1000);

  // initialize SPI for SD card
  SPI.begin(SCK, MISO, MOSI, cs); // if not using default SCK, MISO, MOSI
  printMemory("SPI");

  if (hublink.begin())
  {
    printMemory("BEGIN");
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
  hublink.sync(10); // only blocks when ready
  printMemory("SYNC");
  delay(500);

  // hublink.sleep(1000); // optional light sleep
  esp_sleep_enable_timer_wakeup(2 * 1000000); // 2 seconds in microseconds
  esp_deep_sleep_start();
}