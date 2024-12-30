#define ENABLE_SPI_DEBUG 1
#define ENABLE_SDIO_DEBUG 1
#include <SD.h>
#include <SPI.h>

const int chipSelect = A0; // Using A0 as chip select

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        delay(10);

    // Configure SPI mode before SD initialization
    SPI.begin();
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);

    Serial.println("\nSD Card Tester");
    Serial.println("Commands:");
    Serial.println("  init     - Test SD card initialization");
    Serial.println("  write    - Write test file");
    Serial.println("  list     - List files");
    Serial.println("  remove   - Remove test file");
    Serial.println("  begin1   - SD.begin(cs)");
    Serial.println("  begin2   - SD.begin(cs, SPI)");
    Serial.println("  begin3   - SD.begin(cs, SPI, 4000000)");
    Serial.println("  end      - Call SD.end()");
}

void loop()
{
    if (Serial.available())
    {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command == "init")
        {
            testInit();
        }
        else if (command == "write")
        {
            writeTestFile();
        }
        else if (command == "list")
        {
            listFiles();
        }
        else if (command == "remove")
        {
            removeTestFile();
        }
        else if (command == "begin1")
        {
            testBegin1();
        }
        else if (command == "begin2")
        {
            testBegin2();
        }
        else if (command == "begin3")
        {
            testBegin3();
        }
        else if (command == "end")
        {
            endSD();
        }

        Serial.println("\nReady for next command:");
    }
}

void testInit()
{
    Serial.println("Testing SD card initialization...");

    uint8_t cardType = SD.cardType();
    Serial.print("Card Type: ");
    switch (cardType)
    {
    case CARD_NONE:
        Serial.println("No card");
        break;
    case CARD_MMC:
        Serial.println("MMC");
        break;
    case CARD_SD:
        Serial.println("SD");
        break;
    case CARD_SDHC:
        Serial.println("SDHC");
        break;
    default:
        Serial.println("Unknown");
        break;
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("Card size: %lluMB\n", cardSize);
}

void writeTestFile()
{
    Serial.println("Writing meta.json...");

    File file = SD.open("/meta.json", FILE_WRITE);
    if (!file)
    {
        Serial.println("Failed to open file for writing!");
        return;
    }

    const char *jsonContent = R"({
  "hublink": {
      "advertise": "HUBLINK",
      "advertise_every": 30,
      "advertise_for": 30,
      "try_reconnect": true,
      "reconnect_attempts": 3,
      "reconnect_every": 30000,
      "disable": false
  },
  "subject": {
      "id": "mouse001",
      "strain": "C57BL/6",
      "strain_options": [
          "C57BL/6",
          "BALB/c",
          "129S1/SvImJ",
          "F344",
          "Long Evans",
          "Sprague Dawley"
      ],
      "sex": "male",
      "sex_options": [
          "male",
          "female"
      ]
  },
  "fed": {
      "program": "Classic",
      "program_options": [
          "Classic",
          "Intense",
          "Minimal",
          "Custom"
      ]
  }
})";

    if (file.print(jsonContent))
    {
        Serial.println("meta.json written successfully!");
    }
    else
    {
        Serial.println("Write failed!");
    }

    file.close();
}

void listFiles()
{
    Serial.println("Listing files:");

    File root = SD.open("/");
    if (!root)
    {
        Serial.println("Failed to open root directory!");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        Serial.print("  ");
        Serial.print(file.name());
        Serial.print("\t");
        Serial.println(file.size());
        file = root.openNextFile();
    }

    root.close();
}

void removeTestFile()
{
    if (SD.remove("/test.txt"))
    {
        Serial.println("test.txt removed!");
    }
    else
    {
        Serial.println("Failed to remove test.txt");
    }
}

void testBegin1()
{
    Serial.println("Testing SD.begin(cs)...");
    if (SD.begin(chipSelect))
    {
        Serial.println("Success!");
    }
    else
    {
        Serial.println("Failed!");
    }
}

void testBegin2()
{
    Serial.println("Testing SD.begin(cs, SPI)...");
    if (SD.begin(chipSelect, SPI))
    {
        Serial.println("Success!");
    }
    else
    {
        Serial.println("Failed!");
    }
}

void testBegin3()
{
    Serial.println("Testing SD.begin(cs, SPI, 4000000)...");
    if (SD.begin(chipSelect, SPI, 100000))
    {
        Serial.println("Success!");
    }
    else
    {
        Serial.println("Failed!");
    }
}

void endSD()
{
    Serial.println("Ending SD card operations...");
    SD.end();
    Serial.println("SD card operations ended");
}