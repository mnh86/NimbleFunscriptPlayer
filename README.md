# NimbleFunscriptPlayer

A simple funscript player (firmware) for the [Nimble Connectivity Module](https://shop.exploratorydevices.com/product/connectivity-module-dev-kit/), an ESP32 controller for the [NimbleStroker](https://shop.exploratorydevices.com/).

Upload your funscript files and double click the Encoder Dial to cycle through and play funscript files from the Module itself.

## Usage

1. Install [Windows Virtual COM Port (VCP) drivers](https://github.com/mnh86/NimbleConModule/blob/feat/docs/docs/setup-guide-windows-arduino-ide1.md#install-windows-virtual-com-port-vcp-drivers) for the USB/serial connection to the module.
2. Set up [VSCode with PlatformIO](https://randomnerdtutorials.com/vs-code-platformio-ide-esp32-esp8266-arduino/).
3. Clone this repo and open the project in VSCode.
4. Attach the NimbleConModule to your computer via USB/Serial connection.
5. Place `.funscript` files into the `./data/` folder (see [README](./data/README.md) instructions).
6. Use the PlatformIO tool "Upload Filesystem Image" to upload the files.
7. Build and upload this program into the NimbleConModule.
8. Attach the NimbleConModule to the actuator (Label A).
   - Note: Pendant connection not supported.
9. Double click the Encoder Dial to start the first file. Double click again to change files.
10. Single click will pause/resume playing.
11. Long press will stop playing.

## Attributions

- [Funscript spec](https://devs.handyfeeling.com/docs/scripts/basics/)
- [Official NimbleConSDK](https://github.com/ExploratoryDevices/NimbleConModule)

See also [platformio.ini](./platformio.ini) for other 3rd party OSS libraries used in this project.
