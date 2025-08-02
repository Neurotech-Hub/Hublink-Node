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

### setBatteryLevel(uint8_t level)
Sets the battery level for the node characteristic. Range 0-255 (0 indicates not set).
- `level`: Battery level (0-255)

Example:
```cpp
hublink.setBatteryLevel(85);  // Set to 85%
```

### setAlert(const String &alert)
Sets an alert message that will be included in the node characteristic during the next sync cycle. The alert is automatically cleared after the sync completes.
- `alert`: Alert message string

Example:
```cpp
hublink.setAlert("Low battery warning!");
hublink.sync();  // Alert gets sent and then automatically cleared
```

### getBatteryLevel() const
Returns the current battery level setting.
- Returns: uint8_t battery level (0-255)

### getAlert() const
Returns the current alert message.
- Returns: String alert message

### Initialization Process
The `begin()` function initializes the Hublink node with the following sequence:

1. **Debug Setup** (if enabled)
   - Initializes Serial1 at 115200 baud for debug output
   - Logs wake-up reason (timer, reset, or other)

2. **CPU Configuration**
   - Sets CPU frequency to 80MHz (minimum required for radio operation)

3. **SD Card Initialization**
   - Attempts to initialize SD card with specified chip select and clock frequency
   - Returns false if SD card initialization fails

4. **BLE Configuration**
   - Sets advertising name from parameter
   - Initializes default values:
     - `advertise_every`: 300 seconds (5 minutes)
     - `advertise_for`: 30 seconds
     - `disable`: false
     - `upload_path`: "/FED"
     - `append_path`: "subject:id/experimenter:name"
     - `try_reconnect`: true
     - `reconnect_attempts`: 3
     - `reconnect_every`: 30 seconds

5. **Meta.json Processing**
   - Reads and parses meta.json from SD card
   - Updates configuration values if valid JSON is found
   - Maintains default values if file is missing or invalid

6. **State Initialization**
   - Sets lastHublinkMillis for timing control
   - Returns true if all initialization steps complete successfully

Example:
```cpp
Hublink hublink(SD_CS_PIN, SD_CLK_FREQ);
if (hublink.begin("HUBLINK")) {
    Serial.println("Hublink initialized successfully");
} else {
    Serial.println("Hublink initialization failed");
}
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
  "device": {
      "id": "046"
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
- `advertise_every`: Seconds between advertising periods
- `advertise_for`: Duration of each advertising period in seconds
- `try_reconnect`: Enables/disables automatic reconnection attempts (default: true)
- `reconnect_attempts`: Number of reconnection attempts if initial connection fails (default: 3)
- `reconnect_every`: Seconds between reconnection attempts (default: 30)
- `upload_path`: Base path for file uploads (e.g., "/FED")
- `append_path`: Dynamic path segments using nested JSON values (e.g., "subject:id/experimenter:name")
- `disable`: Enables/disables BLE functionality
- `device.id`: Device identifier that appears in the node characteristic

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

## BLE Characteristics & Protocol

The Hublink library implements a custom BLE service with four characteristics for file transfer and device management. All characteristics use the service UUID: `57617368-5501-0001-8000-00805f9b34fb`

### 1. Node Characteristic (READ)
**UUID**: `57617368-5505-0001-8000-00805f9b34fb`

Provides device status and configuration information in JSON format:
```json
{
  "upload_path": "/FED",
  "firmware_version": "1.0.6",
  "battery_level": 85,
  "device_id": "046",
  "alert": "Low battery warning!"
}
```

**Fields**:
- `upload_path` (string): Base path for file uploads, configured via meta.json
- `firmware_version` (string): Current library version (always present)
- `battery_level` (number): Battery level 0-255 (0 = not set, only present if > 0)
- `device_id` (string): Device identifier from meta.json (only present if configured)
- `alert` (string): Alert message (only present if set by user, auto-clears after sync)

**Usage**: Read this characteristic after connection to get device information and status.

### 2. Gateway Characteristic (WRITE)
**UUID**: `57617368-5504-0001-8000-00805f9b34fb`

Accepts JSON commands to control device behavior. Multiple commands can be sent in a single JSON object:

```json
{
  "timestamp": 1234567890,
  "sendFilenames": true,
  "watchdogTimeoutMs": 10000,
  "metaJsonId": 1,
  "metaJsonData": "{\"hublink\":{\"advertise\":\"HUBLINK\"}}"
}
```

**Commands**:
- `timestamp` (number): Unix timestamp for device synchronization
- `sendFilenames` (boolean): Triggers file listing process when true
- `watchdogTimeoutMs` (number): Sets connection timeout in milliseconds (default: 10000)
- `metaJsonId` + `metaJsonData` (pair): For meta.json updates (see Meta.json Transfer section)

**Usage**: Write JSON commands to control device behavior. Device responds via callbacks.

### 3. Filename Characteristic (READ/WRITE/INDICATE)
**UUID**: `57617368-5502-0001-8000-00805f9b34fb`

**WRITE**: Send filename to request file transfer
```
"data.txt"
```

**INDICATE**: Receives file listing or transfer status
- **File listing format**: `"filename1.txt|1234;filename2.csv|5678;EOF"`
  - Each file: `"filename|filesize"`
  - Separator: `;`
  - End marker: `"EOF"`
- **Transfer status**: `"NFF"` (No File Found) if requested file doesn't exist

**Usage**: 
1. Write filename to request transfer
2. Subscribe to indications for file listing or status updates

### 4. File Transfer Characteristic (READ/INDICATE)
**UUID**: `57617368-5503-0001-8000-00805f9b34fb`

**INDICATE**: Receives file content in chunks
- **Data chunks**: Raw file bytes (MTU-sized, typically 512 bytes)
- **End marker**: `"EOF"` when transfer complete
- **Error marker**: `"NFF"` if file not found

**Usage**: Subscribe to indications to receive file content. Monitor for "EOF" or "NFF" markers.

## Connection Protocol

### 1. Device Discovery
- **Service UUID**: `57617368-5501-0001-8000-00805f9b34fb`
- **Advertising Name**: Configurable via meta.json (default: "HUBLINK")
- **MTU Size**: Device negotiates to 515 bytes (512 + 3 byte header)

### 2. Connection Sequence
1. **Connect** to device
2. **Read Node Characteristic** to get device info
3. **Subscribe to indications** on Filename and File Transfer characteristics
4. **Write to Gateway Characteristic** to send commands

### 3. File Transfer Workflow

#### File Listing
1. Write `{"sendFilenames": true}` to Gateway Characteristic
2. Receive file list via Filename Characteristic indications
3. Parse `"filename|size;filename2|size2;EOF"` format

#### File Download
1. Write filename to Filename Characteristic
2. Receive file content via File Transfer Characteristic indications
3. Monitor for "EOF" or "NFF" markers

### 4. Meta.json Transfer Protocol

#### Reading meta.json
1. Read Node Characteristic (contains current configuration)

#### Writing meta.json
1. Send chunks via Gateway Characteristic:
   ```json
   {"metaJsonId": 1, "metaJsonData": "{\"hublink\":{\"advertise\":\"HUBLINK\"}}"}
   {"metaJsonId": 2, "metaJsonData": "{\"device\":{\"id\":\"046\"}}"}
   ```
2. Send completion marker:
   ```json
   {"metaJsonId": 0, "metaJsonData": "EOF"}
   ```

**Chunking Rules**:
- Sequential IDs starting from 1
- Device validates JSON structure on completion
- Timeout: 5 seconds between chunks
- Automatic cleanup on timeout or error

### 5. Error Handling
- **Connection timeout**: 10 seconds (configurable via `watchdogTimeoutMs`)
- **File not found**: Device sends "NFF" via Filename Characteristic
- **Transfer errors**: Device disconnects and resets state
- **Meta.json errors**: Device reverts to previous configuration

### 6. State Management
- **Alert messages**: Auto-clear after each sync cycle
- **Battery level**: Persists until next update
- **File handles**: Automatically closed on disconnect
- **BLE state**: Reset between advertising cycles

## Coming Soon

### Hublink Editor App
The Hublink Node library supports secure transfer of meta.json configuration between client (mobile app) and ESP32:

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