# Hublink-Node_ESP32

HublinkNode_ESP32 is an Arduino library designed to facilitate Bluetooth Low Energy (BLE) communication and SD card file transfer for ESP32-based nodes.

## Features
- BLE communication using the ESP32 BLE stack.
- File transfer between an SD card and connected BLE central devices.
- Easy integration with user-defined callbacks for customization.

## Installation
1. Download the library from the [GitHub repository](https://github.com/Neurotech-Hub/HublinkNode_ESP32).
2. Place the `HublinkNode_ESP32` folder into your Arduino libraries directory.
3. Restart the Arduino IDE if it is already open.

### Use Arduino IDE
[Installing - - — Arduino ESP32 latest documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)

## Usage
Include the library in your sketch:

```cpp
#include <HublinkNode_ESP32.h>
```

Initialize the BLE and SD components in the `setup()` function and use the library's functions to manage BLE communication and file transfers.

For a full example, refer to the `examples` directory in the library.

### Issues

**Finding .h**
You may need to delete the Arduino cache to recognize new boards/libraries: `rm ~/Library/Arduino15`

**ArduinoBLE Library**
The ArduinoBLE library uses similar naming schemes. The easiest way to avoid conflicts with ESP32 is to separate sketch locations for Arduino BLE and ESP32 BLE project and resetting `Preferences > Sketchbook location` for those projects. Alternatively, you will need to use full paths in the header file, as Arduino usually does not prioritize the ESP32 paths in its compilation order.

## License
This library is open-source and available under the MIT license.

There are a few critical issues:
1. 