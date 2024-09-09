# ESP32 Webcam Module #

## Requirements ##
* ESP32-CAM Module
* FTDI USB to Serial Adapter* (for uploading the program to the ESP32-CAM)
* Arduino IDE 
* ESP32 Arduino Core

---
## Features ##
* Real-time video streaming using the ESP32-CAM module
* Simple web interface for accessing the camera feed

---
## Setup Instructions ##
1. **Install Arduino IDE:**
2. **Add ESP32 to Arduino IDE:**
   Open Arduino IDE, go to File > Preferences, and in the "Additional Board Manager URLs" field, paste the following URL
   (https://dl.espressif.com/dl/package_esp32_index.json
   Then, navigate to ```Tools > Board > Board Manager```, search for "ESP32", and install the ESP32 board package
3. **Connect ESP32-CAM to FTDI Adapter**
4. **Select Board and Port**
5. **Upload the Code**
6. **Run the Webcam**

### Warning ###
Once the upload is complete, press the reset button

Set the Serial Monitor baud rate to **115200**
