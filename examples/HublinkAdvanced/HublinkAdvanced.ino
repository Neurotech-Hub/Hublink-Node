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
    if (hublink.begin())
    {
        Serial.println("✓ Hublink.");

        // Example of reading meta.json values safely
        if (hublink.hasMetaKey("subject", "id"))
        {
            String subjectId = hublink.getMeta<String>("subject", "id");
            Serial.printf("Subject ID: %s\n", subjectId.c_str());
        }

        // Example of reading boolean value
        if (hublink.hasMetaKey("subject", "doFoo"))
        {
            bool doFoo = hublink.getMeta<bool>("subject", "doFoo");
            Serial.printf("doFoo enabled: %s\n", doFoo ? "true" : "false");
        }

        // Example of reading integer value
        if (hublink.hasMetaKey("subject", "age"))
        {
            int age = hublink.getMeta<int>("subject", "age");
            Serial.printf("Subject age: %d\n", age);
        }
    }
    else
    {
        Serial.println("✗ Failed.");
        while (1)
        {
        }
    }
    hublink.setTimestampCallback(onTimestampReceived);

    // Add a single extension
    hublink.addValidExtensions({".xml", ".json"});

    // Or clear and set new extensions
    hublink.clearValidExtensions();
    hublink.addValidExtension(".bin");

    digitalWrite(LED_BUILTIN, LOW); // Turn LED off before continuing
}

void loop()
{
    int syncCount = 0;
    hublink.try_reconnect = true;
    hublink.reconnect_attempts = 3;
    hublink.reconnect_every = 30; // seconds
    hublink.upload_path = "/myhub";
    hublink.append_path = String(syncCount);

    hublink.advertise = "hublink"; // override default advertise name
    if (hublink.sync())
    {
        syncCount++;
    }
    hublink.sleep(1000); // optional ESP32 light sleep
                         // delay(1000);
}
