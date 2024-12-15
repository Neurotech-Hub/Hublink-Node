#include <Hublink.h>

const int cs = A0;
Hublink hublink(cs); // Use default Hublink instance

// Callback function to handle timestamp
void onTimestampReceived(uint32_t timestamp)
{
    Serial.print("Received timestamp: ");
    Serial.println(timestamp);
    // User can do whatever they want with the timestamp here
    // For example, set an RTC module:
    // rtc.adjustTime(timestamp);
}

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
        while (1)
        {
        }
    }

    // Add a single extension
    hublink.addValidExtension(".xml");

    // Or clear and set new extensions
    hublink.clearValidExtensions();
    hublink.addValidExtension(".bin");
    hublink.addValidExtension(".dat");

    // Wait for serial input while flashing LED
    // Serial.println("Press any key to continue...");
    // while (!Serial.available())
    // {
    //   digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); // Toggle LED
    //   delay(200);
    // }
    digitalWrite(LED_BUILTIN, LOW); // Turn LED off before continuing
}

void loop()
{
    hublink.sync();      // only blocks when ready
    hublink.sleep(1000); // optional light sleep
                         // delay(1000);
}