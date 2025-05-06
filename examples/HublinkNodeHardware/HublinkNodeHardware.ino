#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include "Adafruit_MAX1704X.h"
#include <Adafruit_BME280.h>
#include <RTClib.h>

// Pin definitions from Pins_Arduino.h
const uint8_t LED_RED_PIN = LED_RED;       // Pin 13
const uint8_t LED_BLUE_PIN = LED_BLUE;     // Pin 33
const uint8_t SD_CS_PIN = SS;              // Pin 46
const uint8_t SD_EN_PIN = SD_EN;           // Pin 45
const uint8_t I2C_EN_PIN = I2C_EN;         // Pin 7
const uint8_t RTC_PWR_PIN = RTC_POWER;     // Pin 41
const uint8_t MAG_SENSOR_PIN = MAG_SENSOR; // Pin 1
const uint8_t USB_SENSE_PIN = USB_SENSE;   // Pin 34

// Create objects for each sensor
Adafruit_MAX17048 lipo;
Adafruit_BME280 bme;
RTC_DS3231 rtc;

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        delay(10);

    Serial.println("\nHublink Node Hardware Test");
    Serial.println("-------------------------");

    // Initialize LED pins
    Serial.println("\nInitializing LEDs...");
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_BLUE_PIN, OUTPUT);
    digitalWrite(LED_RED_PIN, LOW);
    digitalWrite(LED_BLUE_PIN, LOW);
    Serial.println("✓ LEDs initialized");

    // Test LEDs
    Serial.println("\nTesting LEDs (2 seconds each)...");
    testLEDs();

    // Enable I2C and RTC power
    Serial.println("\nInitializing I2C bus...");
    pinMode(I2C_EN_PIN, OUTPUT);
    pinMode(RTC_PWR_PIN, OUTPUT);
    digitalWrite(I2C_EN_PIN, HIGH);
    digitalWrite(RTC_PWR_PIN, HIGH);
    delay(100); // Wait for power to stabilize
    Wire.begin(SDA, SCL);
    Serial.println("✓ I2C bus initialized");

    // Test LiPo fuel gauge
    Serial.println("\nTesting MAX17048 LiPo fuel gauge...");
    testLiPoGauge();

    // Test BME280
    Serial.println("\nTesting BME280 sensor...");
    testBME280();

    // Test RTC
    Serial.println("\nTesting DS3231 RTC...");
    testRTC();

    // Initialize SD card
    Serial.println("\nInitializing SD card...");
    pinMode(SD_EN_PIN, OUTPUT);
    digitalWrite(SD_EN_PIN, HIGH);
    delay(100);

    if (!SD.begin(SD_CS_PIN))
    {
        Serial.println("✗ SD card initialization failed!");
        blinkError();
        return;
    }
    Serial.println("✓ SD card initialized");

    // Test SD card read/write
    Serial.println("\nTesting SD card read/write...");
    testSDCard();

    // Initialize magnetic sensor and USB sense pins
    Serial.println("\nInitializing additional sensors...");
    pinMode(MAG_SENSOR_PIN, INPUT);
    pinMode(USB_SENSE_PIN, INPUT);
    Serial.println("✓ Additional sensors initialized");

    Serial.println("\nAll hardware tests complete!");
}

void testLEDs()
{
    Serial.println("Testing RED LED");
    digitalWrite(LED_RED_PIN, HIGH);
    delay(2000);
    digitalWrite(LED_RED_PIN, LOW);

    Serial.println("Testing BLUE LED");
    digitalWrite(LED_BLUE_PIN, HIGH);
    delay(2000);
    digitalWrite(LED_BLUE_PIN, LOW);

    Serial.println("✓ LED test complete");
}

void testLiPoGauge()
{
    if (!lipo.begin())
    {
        Serial.println("✗ Couldn't find LiPo fuel gauge");
        blinkError();
        return;
    }
    Serial.println("✓ Found MAX17048 fuel gauge");

    // Read and display battery stats
    Serial.printf("Battery Voltage: %.2fV\n", lipo.cellVoltage());
    Serial.printf("Battery Percent: %.1f%%\n", lipo.cellPercent());
    Serial.println("✓ Fuel gauge test complete");
}

void testBME280()
{
    if (!bme.begin(0x76))
    { // Try alternate address if 0x77 fails
        if (!bme.begin(0x77))
        {
            Serial.println("✗ Couldn't find BME280 sensor!");
            blinkError();
            return;
        }
    }
    Serial.println("✓ Found BME280 sensor");

    // Read and display environmental data
    Serial.printf("Temperature: %.2f°C\n", bme.readTemperature());
    Serial.printf("Humidity: %.1f%%\n", bme.readHumidity());
    Serial.printf("Pressure: %.1fhPa\n", bme.readPressure() / 100.0F);
    Serial.println("✓ BME280 test complete");
}

void testRTC()
{
    if (!rtc.begin())
    {
        Serial.println("✗ Couldn't find RTC!");
        blinkError();
        return;
    }
    Serial.println("✓ Found RTC");

    if (rtc.lostPower())
    {
        Serial.println("! RTC lost power, setting time to compile time");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    DateTime now = rtc.now();
    Serial.printf("RTC Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                  now.year(), now.month(), now.day(),
                  now.hour(), now.minute(), now.second());

    float temp = rtc.getTemperature();
    Serial.printf("RTC Temperature: %.2f°C\n", temp);
    Serial.println("✓ RTC test complete");
}

void testSDCard()
{
    // Create test file
    File testFile = SD.open("/test.txt", FILE_WRITE);
    if (!testFile)
    {
        Serial.println("✗ Failed to create test file!");
        blinkError();
        return;
    }

    // Write to file
    if (testFile.println("Hublink Node Test"))
    {
        Serial.println("✓ Successfully wrote to test file");
    }
    else
    {
        Serial.println("✗ Failed to write to test file!");
        testFile.close();
        blinkError();
        return;
    }
    testFile.close();

    // Read from file
    testFile = SD.open("/test.txt");
    if (!testFile)
    {
        Serial.println("✗ Failed to open test file for reading!");
        blinkError();
        return;
    }

    String content = testFile.readString();
    testFile.close();

    if (content.indexOf("Hublink Node Test") >= 0)
    {
        Serial.println("✓ Successfully read from test file");
    }
    else
    {
        Serial.println("✗ File content verification failed!");
        blinkError();
        return;
    }

    // Clean up test file
    if (SD.remove("/test.txt"))
    {
        Serial.println("✓ Test file cleaned up");
    }
    else
    {
        Serial.println("! Failed to clean up test file");
    }

    Serial.println("✓ SD card test complete");
}

void loop()
{
    static unsigned long lastUpdate = 0;
    const unsigned long UPDATE_INTERVAL = 5000; // 5 seconds

    // Always update USB sense LED (no delay)
    digitalWrite(LED_BLUE_PIN, digitalRead(USB_SENSE_PIN));

    // Update sensor readings every 5 seconds
    if (millis() - lastUpdate >= UPDATE_INTERVAL)
    {
        lastUpdate = millis();

        // Print all sensor values
        Serial.println("\nCurrent Readings:");
        Serial.printf("Battery: %.1f%% (%.2fV)\n",
                      lipo.cellPercent(), lipo.cellVoltage());

        Serial.printf("BME280: %.1f°C, %.1f%%, %.1fhPa\n",
                      bme.readTemperature(),
                      bme.readHumidity(),
                      bme.readPressure() / 100.0F);

        DateTime now = rtc.now();
        Serial.printf("Time: %02d:%02d:%02d\n",
                      now.hour(), now.minute(), now.second());

        // Add magnetic sensor status
        Serial.printf("Magnetic Sensor: %s\n",
                      digitalRead(MAG_SENSOR_PIN) ? "NOT DETECTED" : "DETECTED");

        Serial.printf("USB Power: %s\n",
                      digitalRead(USB_SENSE_PIN) ? "DISCONNECTED" : "CONNECTED");

        // Only toggle red LED now (blue LED follows USB sense)
        digitalWrite(LED_RED_PIN, !digitalRead(LED_RED_PIN));
    }
}

void blinkError()
{
    // Blink both LEDs rapidly to indicate error
    for (int i = 0; i < 5; i++)
    {
        digitalWrite(LED_RED_PIN, HIGH);
        digitalWrite(LED_BLUE_PIN, HIGH);
        delay(100);
        digitalWrite(LED_RED_PIN, LOW);
        digitalWrite(LED_BLUE_PIN, LOW);
        delay(100);
    }
}
