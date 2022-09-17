# Esp32_High_Quality_Recorder

## PCB Design
At [Esp32_High_Quality_Recorder_PCB](https://github.com/ZhongWwwHhh/Esp32_High_Quality_Recorder_PCB).  
Designed with Kicad.  

## Usage
Install ESP-IDF then compile this project.  
Or flash released firmware to esp32 directly.  
~~As for update, rename the firmware to `update.bin` and save it into TF(SD) card. When next boot, new firmware will be flash to esp32 by OTA function.~~ `coming soon`

## Power-on Order
When power on, led1 will be set high immediately.  
If `USE_BATTERY` has been ticked in menuconfig, it will check battery level to pursure the power supply is higher than 3.3 voltage. Esp32 will drop to deep sleep since detecting power insufficient. During deep sleep, all leds will be off.  
Recording will start after momunting TF card and opening record file. Led1 will be set low.  
The file system get written per half second. Every time the file has deen saved, led2 would change its state.  

**So if it is running correctly, led1 is off and led2 will blink.**  