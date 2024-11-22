# Hublink-Node

HublinkNode is an Arduino library designed to facilitate Bluetooth Low Energy (BLE) communication and SD card file transfer for ESP32-based nodes. It provides a simple interface for periodic BLE advertising and file transfer capabilities.

## Features
- Single-initialization BLE communication using the ESP32 BLE stack
- File transfer between an SD card and connected BLE central devices
- Configurable BLE advertising periods via hublink.node file
- Support for common file types (.txt, .csv, .log)
- Configuration via BLE characteristic writes
- Light sleep support between advertising cycles
- Easy integration with user-defined callbacks

## Installation
1. Download the library from the [GitHub repository](https://github.com/Neurotech-Hub/HublinkNode)
2. Place the `HublinkNode` folder into your Arduino libraries directory
3. Restart the Arduino IDE if it is already open

### Prerequisites
- Install the ESP32 board package in Arduino IDE following the [official guide](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
- An ESP32-S3 development board
- An SD card module compatible with ESP32

## Usage
Include the library in your sketch:

```cpp
#include <HublinkNode.h>
```

### Basic Setup
```cpp
// Initialize HublinkNode with SD card settings
const int cs = A0;
HublinkNode hublinkNode(cs);     // optional (cs, clkFreq) parameters

// Define your callback classes
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) override {
        hublinkNode.onConnect();
    }
    void onDisconnect(BLEServer *pServer) override {
        hublinkNode.onDisconnect();
    }
};

// Create static callback instances
static ServerCallbacks serverCallbacks;
// ... other callbacks ...

void setup() {
    // Initialize BLE with device name
    hublinkNode.init("HUBNODE", true);
    hublinkNode.setBLECallbacks(&serverCallbacks,
                               &filenameCallback,
                               &gatewayCallback);
}
```

### Configuration File (hublink.node)
The library accepts configuration via a file on the SD card in the following format:
```
advertise=CUSTOM_NAME
interval=300,30
disable=false
```

Where:
- `advertise`: Sets custom BLE advertising name
- `interval`: Format is `EVERY_SECONDS,FOR_SECONDS`
- `disable`: Enables/disables BLE functionality

### BLE Configuration
The library also accepts configuration via BLE in the following format:
```
key1=value1;key2=value2;key3=value3
```

Example configuration string:
```
rtc=2024-03-21 14:30:00
```

For a complete working example, refer to the `examples/hublink_example` directory in the library.

## License
This library is open-source and available under the MIT license.

## Contributing
Contributions are welcome! Please feel free to submit a Pull Request.