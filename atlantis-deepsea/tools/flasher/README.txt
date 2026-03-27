ATLANTIS DEEPSEA - Windows Portable Flasher
============================================

QUICK START
-----------
  1. Plug in your LilyGO T-Display via USB-C
  2. Double-click flash.bat
  3. Wait ~30 seconds
  4. Done!


IF THE T-DISPLAY IS NOT DETECTED
---------------------------------
You need to install the CP210x USB driver:

  1. Go to: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
  2. Download "CP210x Windows Drivers"
  3. Extract and run CP210xVCPInstaller_x64.exe
  4. RESTART WINDOWS
  5. Plug in the T-Display and run flash.bat again

After installing, open Device Manager and look for:
  Ports (COM & LPT) > Silicon Labs CP210x USB to UART Bridge (COM?)
That COM number is your port.

You can also run the flasher with a specific port:
  flash.bat COM3


TROUBLESHOOTING
---------------
"Failed to connect to ESP32"
  - Hold the BOOT button on the T-Display while running flash.bat
  - Try a different USB-C cable (some cables are charge-only with no data lines)

"Access denied on COM port"
  - Close Arduino IDE, PuTTY, or any serial monitor
  - Try a different USB port on your PC

"esptool.exe not found"
  - Make sure flash.bat is in the same folder as esptool.exe and the .bin files
  - Do not move files out of this folder


CONTENTS OF THIS PACKAGE
-------------------------
  flash.bat       - Flasher script (double-click this)
  esptool.exe     - Espressif flash tool (bundled, no install needed)
  firmware.bin    - Main application firmware
  bootloader.bin  - ESP32 bootloader
  partitions.bin  - Flash partition table
  boot_app0.bin   - OTA boot selector
  README.txt      - This file


FIRST BOOT
----------
After flashing, the T-Display restarts automatically and shows
the Atlantis DeepSea setup screen. Follow the on-screen steps:
  1. Choose 12 or 24 seed words
  2. Enter your BIP39 seed phrase word by word
  3. Verify 3 words to confirm your backup
  4. Set a 6-digit PIN
  5. Your vault is ready

BUTTONS
-------
  BTN1 (top button)    - Increment / next letter / scroll
  BTN2 (bottom button) - Confirm / select
  Hold BTN1            - Go back / delete
  Hold BTN2            - Switch mode
