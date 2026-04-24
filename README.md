# Atlantis DeepSea — BIP85 Password Manager

A hardware password manager for the **LilyGO T-Display (ESP32)** that derives unique, deterministic passwords from a single BIP39 seed phrase using the BIP85 standard.

Your seed never leaves the device. Every password is re-derived on demand — nothing is stored except the encrypted seed.

## How it works

1. Enter your 12 or 24-word BIP39 seed phrase on the device (or provision it from your PC via the flasher)
2. Pick a password slot (index 0–9999)
3. The device derives a strong, unique password for that slot using BIP85 (`m/83696968'/707764'/20'/index'`)
4. The password is shown on screen and broadcast over BLE — ready to type

Same seed + same index = same password, every time. No cloud, no sync, no server.

## Flash the firmware

### Windows — two clicks
1. Download **`AtlantisDeepSea-Flasher.exe`** from the [latest release](https://github.com/Ak1ra00/hw-flash/releases/latest)
2. Enter bootloader mode: hold **BOOT**, replug USB, release **BOOT**
3. Double-click the .exe — it auto-detects the device and flashes

> **SmartScreen warning?** Click **More info → Run anyway.**
> **Device not found?** Install the [CP210x driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers), restart, retry.

### macOS / Linux — terminal
```bash
git clone https://github.com/ak1ra00/hw-flash
cd hw-flash/atlantis-deepsea
chmod +x flash.sh && ./flash.sh
```

## PC seed provisioning

After flashing, the flasher CMD window will ask if you want to type your seed words directly into the terminal. The device receives the derived keys over serial — your seed words never touch the device screen.

## Hardware

- [LilyGO T-Display (ESP32)](https://github.com/Xinyuan-LilyGO/TTGO-T-Display) — ~$10–15
- USB-C cable

## License

MIT — see [LICENSE.md](LICENSE.md)
