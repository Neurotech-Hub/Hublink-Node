# Hublink

Hublink is an Arduino library designed to facilitate Bluetooth Low Energy (BLE) communication and SD card file transfer for ESP32-based nodes. It provides a simple interface for periodic BLE advertising and file transfer capabilities.

Learn more at [https://hublink.cloud](https://hublink.cloud).

## Core Functions
### sync(uint32_t temporaryConnectFor = 0)
Manages BLE advertising and connection cycles. Returns true if a connection was established.
- `temporaryConnectFor`: Optional duration in seconds to override the default advertising period
- Returns: boolean indicating if connection was successful

Example:
```cpp
// Use default advertising period from meta.json
bool success = hublink.sync();

// Override with 60-second advertising period
bool success = hublink.sync(60);
```

### sleep(uint64_t seconds)
Puts the ESP32 into light sleep mode for the specified duration.
- `seconds`: Duration to sleep in seconds
- Returns: void

Example:
```cpp
// Sleep for 5 seconds
hublink.sleep(5);
```

## Installation
1. Download the Hublink-Node library in Arduino IDE. Alternatively (but not recommended), clone the [GitHub repository](https://github.com/Neurotech-Hub/HublinkNode).
4. Ensure the dependencies are installed from Arduino IDE:
   - NimBLE-Arduino
   - ArduinoJson

### Flashing with Hublink Nodes
1. If the device has been previously flashed, it may be in a deep sleep state and will not connect to the serial port. To enter boot mode, hold the `Boot` button and toggle the `Reset` button (then release the `Boot` button).
2. If the Arduino IDE does not indicate that it is connected to "Adafruit Feather ESP32-S3 2MB PSRAM", click Tools -> Board -> esp32 -> Adafruit Feather ESP32-S3 2MB PSRAM (you will need to download the esp32 board package (by espressif) from the Arduino IDE).

### Prerequisites
- Install the ESP32 board package in Arduino IDE following the [official guide](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
- An ESP32-S3 board with an SD card module ([View Pricing](https://hublink.cloud/pricing))

## Usage
For a complete working example, refer to the `examples/` directory. Files on the SD card are treated as accumulators and Hublink uses file size as a "diff" proxy (i.e., if the number of bytes in a file changes _at all_, Hublink will attempt to transfer the file).

### Meta Data File (meta.json)
The library accepts configuration via a file on the SD card in JSON format.

```json
{
  "hublink": {
      "advertise": "HUBLINK",
      "advertise_every": 300,
      "advertise_for": 30,
      "try_reconnect": true,
      "reconnect_attempts": 3,
      "reconnect_every": 30,
      "upload_path": "/FED",
      "append_path": "subject:id/experimenter:name",
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
  "experimenter": {
      "name": "john_doe"
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
}
```

Where,
- `advertise`: Sets custom BLE advertising name
- `interval_every`: Seconds between advertising periods
- `interval_for`: Duration of each advertising period in seconds
- `try_reconnect`: Enables/disables automatic reconnection attempts (default: true)
- `reconnect_attempts`: Number of reconnection attempts if initial connection fails (default: 3)
- `reconnect_every`: Seconds between reconnection attempts (default: 30)
- `upload_path`: Base path for file uploads (e.g., "/FED")
- `append_path`: Dynamic path segments using nested JSON values (e.g., "subject:id/experimenter:name")
- `disable`: Enables/disables BLE functionality

#### Path Construction
The `append_path` field supports multiple nested JSON values separated by forward slashes. For example:

- Single value: `"append_path": "subject:id"`
  - Result: `/FED/mouse001`

- Multiple values: `"append_path": "subject:id/experimenter:name"`
  - Result: `/FED/mouse001/john_doe`

The path construction:
1. Starts with `upload_path`
2. Appends each value specified in `append_path` if it exists and is not empty
3. Skips any missing or empty values
4. Sanitizes the path to ensure it's valid for S3 storage:
   - Allows alphanumeric characters (a-z, A-Z, 0-9)
   - Allows hyphen (-), underscore (_), plus (+), period (.)
   - Removes duplicate slashes
   - Removes trailing slashes

For example, if `experimenter:name` is missing or empty, the path would be `/FED/mouse001` instead of `/FED/mouse001/`.

Hublink uses [bblanchon/ArduinoJson](https://github.com/bblanchon/ArduinoJson) to parse the JSON file. There are a number of free JSON editors/visualizers (e.g., [JSON to Graph Converter](https://jsonviewer.tools/editor)).

## Coming Soon

### Hublink Editor App
The Hublinke Node library supports secure transfer of meta.json configuration between client (mobile app) and ESP32:

**Reading meta.json (ESP32 → Client)**
1. Client requests meta.json content
2. ESP32 serves current configuration via NODE characteristic
3. Client receives complete JSON in single read

**Writing meta.json (Client → ESP32)**
1. Client divides meta.json into chunks and sends sequentially
2. ESP32 validates chunk sequence and writes to temporary file
3. On completion, ESP32:
   - Validates JSON structure
   - Backs up existing meta.json
   - Replaces with new content
   - Updates BLE configuration

## License
This library is open-source and available under the MIT license.

## Contributing
Contributions are welcome! Please feel free to submit a Pull Request.

## TODO
1. Memory/Resource Cleanup
[x] Verify cleanupCallbacks() is called in all exit paths
[ ] Add memory tracking at key points in sync() cycle
[x] Ensure file handles are properly closed
2. BLE State Management
[ ] Add state validation before BLE operations
[ ] Verify complete deinit between advertising cycles (see below)
[x] Review static callback instance lifecycle
3. Timing & Deep Sleep
[x] Validate timer arithmetic for overflows
[x] Add proper delays between BLE operations
[ ] Review initialization after deep sleep
- Consider testing at begin(), detecting wake from sleep, or sync():
```cpp
// Ensure clean BLE state before starting
if (NimBLEDevice::getInitialized()) {
    NimBLEDevice::deinit(true);
    delay(100);
}
```
4. SD Card Operations
[ ] Add SD card state validation
[ ] Verify SD reinitialization after sleep
[x] Review file handle management
5. Null Pointer Protection
[x] Add null checks in critical paths
[x] Validate object state before operations
[x] Review pointer lifecycle in callbacks