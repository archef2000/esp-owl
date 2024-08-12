# AWDL ESP

Currently only for esp32-s3 devices.

Start of [Open Wireless Link](https://github.com/seemoo-lab/owl) (Apple Wireless Direct Link) support for ESP32 devices. For [OpenDrop](https://github.com/seemoo-lab/opendrop)

## Supported devices
- esp32-s3 8MB flash

## Flashing with prebuild binaries
1. Download the [owl-full-esp32s3.bin](https://github.com/archef2000/esp-owl/releases/download/main/owl-full-esp32s3.bin) from the main [release page](https://github.com/archef2000/esp-owl/releases)
2. Visit https://espressif.github.io/esptool-js/
3. Connect your esp to your computer
4. Set the baud rate to 460800
5. Click Connect select the right Serial port
6. Set the Flash Address to 0x0 and upload the owl-full-esp32s3.bin
7. Click Program and wait for the process to finish
8. Use a Serial Terminal to check if everything works.

## Build locally
ESP-IDF Build System needed
```
idf.py build
```
