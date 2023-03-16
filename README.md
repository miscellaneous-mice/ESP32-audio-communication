# ESP32 audio communication

## About the project

*
This project is made to start and stop recording and transfer the audio data from ESP32 using MEMS microphones in stereo to the connected device.
*

## Parts Used are 

```
ESP32 board used : https://www.adafruit.com/product/3405

MEMS microphone : https://www.adafruit.com/product/3421

LEDs (RGB): https://robu.in/product/diffused-rgb-common-anode-led-10mm-tricolor-5pcs/?pid=456281

Any SD card reader esp32
```


**This is project is done on Arduino ide which is available here**  :  https://www.arduino.cc/en/software

**Arduino ide requirements**

*We need to add some libraries to the arduino ide via the package manager either via ZIP file or by inbuilt package downloader option*

*Board used is Espressif ESP32 Dev Module : https://docs.platformio.org/en/latest/boards/espressif32/esp32dev.html?utm_source=platformio&utm_medium=piohome*


## Steps to get the project working

*
1) Copy all the files from www to the SD card and connect it to ESP32

2) Import the code to Arduino IDE 

3) Compile the code and dump the code to ESP32

4) Use the phone to connect the WiFi ssid = "ESP32_audio" password = "12345678" 

5) Enter the address “192.168.4.1” to log in to the server in the browser on your mobile phone 

6) Now you can start, stop and transfer recording seamlessly
*


## Note:
-	**This is a stereo channel recording** 
- **Schematics are given in my other project done in KICAD : https://github.com/miscellaneous-mice/ESP32_PNOI_PCB/tree/main/ESP32**
- **If the library used is : https://github.com/me-no-dev/ESPAsyncWebServer**
