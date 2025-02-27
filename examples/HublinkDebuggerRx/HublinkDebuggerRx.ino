#include <SD.h>
#include <SPI.h>

const int cs = A0;
File logFile;
const char *LOG_PREFIX = "/log_";
char filename[32];
uint8_t byteBuffer[32]; // Buffer to store bytes until line ending
size_t byteCount = 0;   // Count of bytes in current line
bool lineInProgress = false;

void setup()
{
    Serial.begin(9600);    // For debug output
    Serial1.begin(115200); // For receiving bytes
    pinMode(LED_BUILTIN, OUTPUT);
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
        sprintf(filename, "%s%d.csv", LOG_PREFIX, fileCount++);
    } while (SD.exists(filename));

    logFile = SD.open(filename, FILE_WRITE);
    if (!logFile)
    {
        Serial.println("✗ Failed to create log file!");
        while (1)
            ;
    }
    Serial.printf("✓ Logging to: %s\n", filename);

    // Write CSV header
    logFile.println("timestamp,byte,hex");
    logFile.flush();
}

void loop()
{
    // Read all available bytes
    while (Serial1.available())
    {
        uint8_t inByte = Serial1.read();

        // If we see a newline
        if (inByte == '\n')
        {
            // If we have exactly one byte (ignoring possible \r), log it
            if (byteCount == 1 || (byteCount == 2 && byteBuffer[1] == '\r'))
            {
                uint32_t timestamp = millis();
                // Write timestamp and first byte value to file
                logFile.printf("%lu,%u,0x%02X\n", timestamp, byteBuffer[0], byteBuffer[0]);
                // Optional: Print debug info
                Serial.printf("%lu: %u (0x%02X)\n", timestamp, byteBuffer[0], byteBuffer[0]);
                // Ensure data is written to SD card
                logFile.flush();
                // Toggle LED on valid data
                digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            }
            // Reset for next line
            byteCount = 0;
            lineInProgress = false;
            continue;
        }

        // If we see a carriage return, just skip it
        if (inByte == '\r')
        {
            continue;
        }

        // Store the byte if we have room
        if (byteCount < sizeof(byteBuffer))
        {
            byteBuffer[byteCount++] = inByte;
            lineInProgress = true;
        }
    }

    // If we've been collecting bytes for too long without a newline,
    // reset the buffer (this helps recover from noise)
    if (lineInProgress && byteCount >= sizeof(byteBuffer))
    {
        byteCount = 0;
        lineInProgress = false;
    }
}
