# Hublink-Node_ESP32

HublinkNode_ESP32 is an Arduino library designed to facilitate Bluetooth Low Energy (BLE) communication and SD card file transfer for ESP32-based nodes. It provides a simple interface for periodic BLE advertising and file transfer capabilities.

## Features
- BLE communication using the ESP32 BLE stack
- File transfer between an SD card and connected BLE central devices
- Configurable BLE advertising periods
- Support for common file types (.txt, .csv, .log)
- Configuration via BLE characteristic writes
- Easy integration with user-defined callbacks

## Installation
1. Download the library from the [GitHub repository](https://github.com/Neurotech-Hub/HublinkNode_ESP32)
2. Place the `HublinkNode_ESP32` folder into your Arduino libraries directory
3. Restart the Arduino IDE if it is already open

### Prerequisites
- Install the ESP32 board package in Arduino IDE following the [official guide](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
- An ESP32 development board
- An SD card module compatible with ESP32

## Usage
Include the library in your sketch:

```cpp
#include <HublinkNode_ESP32.h>
```

### Basic Setup
```cpp
// Initialize HublinkNode with SD card settings
HublinkNode_ESP32 hublinkNode(CS_PIN, SD_CLOCK_FREQUENCY);

void setup() {
    // Initialize BLE with device name
    hublinkNode.initBLE("ESP32_BLE_SD");
    
    // Set up callbacks for BLE events
    hublinkNode.setBLECallbacks(new ServerCallbacks(),
                               new FilenameCallbacks(),
                               new ConfigCallbacks());
}
```

### Configuration Format
The library accepts configuration via BLE in the following format:
```
key1=value1;key2=value2;key3=value3
```

Example configuration string:
```
BLE_CONNECT_EVERY=300;BLE_CONNECT_FOR=30;rtc=2024-03-21 14:30:00
```

For a complete working example, refer to the `examples/hublink_esp32_example` directory in the library.

### Common Issues

**Finding .h File**
If the IDE cannot find the library header, try clearing the Arduino cache:
```bash
rm ~/Library/Arduino15
```

**ArduinoBLE Library Conflicts**
The ArduinoBLE library uses similar naming schemes. To avoid conflicts:
- Use separate sketch locations for Arduino BLE and ESP32 BLE projects
- Reset `Preferences > Sketchbook location` for different projects
- Or use full paths in header files

## License
This library is open-source and available under the MIT license.

## Contributing
Contributions are welcome! Please feel free to submit a Pull Request.