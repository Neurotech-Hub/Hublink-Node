# Hublink

Hublink is an Arduino library designed to facilitate Bluetooth Low Energy (BLE) communication and SD card file transfer for ESP32-based nodes. It provides a simple interface for periodic BLE advertising and file transfer capabilities.

Learn more at [https://hublink.cloud](https://hublink.cloud).

## Installation
1. Download the library from the [GitHub repository](https://github.com/Neurotech-Hub/HublinkNode)
2. Place the `HublinkNode` folder into your Arduino libraries directory
3. Restart the Arduino IDE if it is already open

### Prerequisites
- Install the ESP32 board package in Arduino IDE following the [official guide](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
- An ESP32-S3 board with an SD card module ([View Pricing](https://hublink.cloud/pricing))

## Usage
For a complete working example, refer to the `examples/` directory.

### Meta Data File (meta.json)
The library accepts configuration via a file on the SD card in JSON format.

```json
{
  "hublink": {
      "advertise": "HUBLINK",
      "advertise_every": 300,
      "advertise_for": 30,
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
}
```

Where,
- `advertise`: Sets custom BLE advertising name
- `interval_every`: Seconds between advertising periods
- `interval_for`: Duration of each advertising period in seconds
- `disable`: Enables/disables BLE functionality

Hublink uses [bblanchon/ArduinoJson](https://github.com/bblanchon/ArduinoJson) to parse the JSON file. There are a number of free JSON editors/visualizers (e.g., [JSON to Graph Converter](https://jsonviewer.tools/editor)).

#### Meta.json Transfer Workflow
The library supports secure transfer of meta.json configuration between client (mobile app) and ESP32:

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

The transfer protocol includes error handling for:
- Sequence validation
- Timeout detection
- Connection loss
- JSON structure validation
- Incomplete transfers

## License
This library is open-source and available under the MIT license.

## Contributing
Contributions are welcome! Please feel free to submit a Pull Request.
