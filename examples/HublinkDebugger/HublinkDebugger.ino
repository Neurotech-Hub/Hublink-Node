#include <SD.h>
#include <SPI.h>

const int cs = A0;
File logFile;
const char *LOG_PREFIX = "/log_";
char filename[32];

void setup()
{
    Serial.begin(9600);    // For debug output
    Serial1.begin(115200); // For receiving bytes
    delay(1000);

    // Initialize SPI for SD card
    SPI.begin(SCK, MISO, MOSI, cs);
    if (!SD.begin(cs))
    {
        Serial.println("✗ SD Card initialization failed!");
        while (1)
            ;
    }
    Serial.println("✓ SD Card initialized.");

    // Create unique filename using boot count
    int fileCount = 0;
    do
    {
        sprintf(filename, "%s%d.bin", LOG_PREFIX, fileCount++);
    } while (SD.exists(filename));

    logFile = SD.open(filename, FILE_WRITE);
    if (!logFile)
    {
        Serial.println("✗ Failed to create log file!");
        while (1)
            ;
    }
    Serial.printf("✓ Logging to: %s\n", filename);
}

void loop()
{
    if (Serial1.available())
    {
        uint8_t byte = Serial1.read();

        // Write timestamp (4 bytes) and data byte
        uint32_t timestamp = millis();
        logFile.write((uint8_t *)&timestamp, 4);
        logFile.write(byte);

        // Ensure data is written to SD card
        logFile.flush();

        // Optional: Print debug info
        Serial.printf("%lu: 0x%02X\n", timestamp, byte);
    }
}
