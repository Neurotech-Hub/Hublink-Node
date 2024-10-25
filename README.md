# HublinkNode_ESP32

HublinkNode_ESP32 is an Arduino library designed to facilitate Bluetooth Low Energy (BLE) communication and SD card file transfer for ESP32-based nodes.

## Features
- BLE communication using the ESP32 BLE stack.
- File transfer between an SD card and connected BLE central devices.
- Easy integration with user-defined callbacks for customization.

## Installation
1. Download the library from the [GitHub repository](https://github.com/neurotechhub/HublinkNode_ESP32).
2. Place the `HublinkNode_ESP32` folder into your Arduino libraries directory.
3. Restart the Arduino IDE if it is already open.

## Usage
Include the library in your sketch:

```cpp
#include <HublinkNode_ESP32.h>
```

Initialize the BLE and SD components in the `setup()` function and use the library's functions to manage BLE communication and file transfers.

For a full example, refer to the `examples` directory in the library.

## License
This library is open-source and available under the MIT license.

