#include <Hublink.h>

const int cs = A0;
const int BUTTON_PIN = 0;  // GPIO 0
Hublink hublink(cs);       // Use default Hublink instance

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  // If it's a reboot (not a timer wakeup), wait for button press
  if (wakeup_reason != ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("System rebooted. Press GPIO 0 button to continue...");
    bool ledState = false;
    while (digitalRead(BUTTON_PIN) == HIGH) {
      digitalWrite(LED_BUILTIN, ledState);
      ledState = !ledState;
      delay(100);  // Blink every 100ms
    }
    digitalWrite(LED_BUILTIN, LOW);  // Turn off LED after button press
  }

  // initialize SPI for SD card
  // SPI.begin(SCK, MISO, MOSI, cs); // if not using default SCK, MISO, MOSI
  hublink.doDebug = false;
  if (hublink.begin()) {
    Serial.println("✓ Hublink.");
  } else {
    Serial.println("✗ Failed.");
    while (1) {
    }
  }

  digitalWrite(LED_BUILTIN, HIGH);
  hublink.sync(10);  // only blocks when ready
  digitalWrite(LED_BUILTIN, LOW);
  esp_sleep_enable_timer_wakeup(1000000);  // 1 second in microseconds
  esp_deep_sleep_start();
}

void loop() {
}