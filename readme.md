# razer_battery_taskbar
Util for displaying information about the charge level of Razer devices


## Usage

1. Set winusb drivers for your device with zadig (for me need to change interfaces 1-4 for Dethadder v2 Pro)
2. Change hardcoded variables for your device 
```
#define TRAN_ID 0x3f        // Razer dethadder v2 Pro
#define PRODUCT_ID 0x007D   // Razer dethadder v2 Pro
```
3. compile and run start.vbs