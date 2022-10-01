# Esp32_High_Quality_Recorder

## PCB Design
At [Esp32_High_Quality_Recorder_PCB](https://github.com/ZhongWwwHhh/Esp32_High_Quality_Recorder_PCB).  
Designed with Kicad.  

## Usage
Install ESP-IDF then compile this project.  
Or flash released firmware to esp32 directly.  
As for update, rename the firmware to `update.bin` and save it into TF(SD) card. When next boot, new firmware will be flashed to esp32 by OTA function.  

## Power-on Order

### General startup
When power on, led1 will be set high immediately.  
If `USE_BATTERY` has been ticked in menuconfig, it will check battery level to pursure the power supply is higher than 3.3 voltage(but it isn't so accurate). Esp32 will drop to deep sleep once detecting power insufficient. During deep sleep, all leds will be off.  
Recording will start after momunting TF card and opening record file. Led1 will keep high.  
The file system get written about per half second. Every time the file has deen saved, led2 would change its state.  

**So if it is running correctly, led1 is on and led2 will blink.**  

### OTA update from TF card
After momunt TF card, it will try finding the file named `update.bin`. If there is a file named as same, OTA would start.  
The new firmware will be flashed into one of the two ota partitions automatically. Then delete the file and reboot to new firmware.  
**Be careful that there is no file also named `update.bin` in TF card to avoid fault.**