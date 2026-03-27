# Flashing Guide — Atlantis DeepSea

## Quick Start

### Step 0 — Put the device in bootloader mode

Do this **before** flashing, especially if the device is already running firmware (Jade, etc.):

1. Hold the **BOOT button** (left button on the front of the T-Display)
2. While holding BOOT, unplug and replug the USB cable
3. Release BOOT — the screen goes blank — device is ready to flash

> On a brand-new, never-flashed device you can skip this step.

---

**Windows** — download and double-click, no installs needed:

1. Put device in bootloader mode (see above)
2. Download **[AtlantisDeepSea-Flasher.exe](https://github.com/ak1ra00/hw-flash/releases/latest)**
3. Double-click the `.exe` — done in ~30 seconds

**macOS** — three commands in Terminal:
```bash
git clone https://github.com/ak1ra00/hw-flash
cd hw-flash/atlantis-deepsea && chmod +x flash.sh && ./flash.sh
```

---

## Windows — Portable .exe (easiest)

No installs required. Everything is bundled.

### Step 1 — Download the flasher

Go to the [**Releases page**](https://github.com/ak1ra00/hw-flash/releases/latest) and download **`AtlantisDeepSea-Flasher.exe`**.

### Step 2 — Install USB driver (first time only)

The T-Display uses a **CP2102** USB chip. Windows 10/11 usually installs it automatically when you plug in the device. If the device isn't detected, install the driver manually:

1. Go to **[silabs.com/developers/usb-to-uart-bridge-vcp-drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)**
2. Download **CP210x Windows Drivers**
3. Extract and run **`CP210xVCPInstaller_x64.exe`**
4. **Restart Windows**

### Step 3 — Flash

1. Plug in your T-Display via **USB-C**
2. Double-click **`AtlantisDeepSea-Flasher.exe`**
3. The flasher auto-detects your device and flashes — done in ~30 seconds

> **Windows SmartScreen warning?**
> Click **More info** → **Run anyway**.
> This appears because the file isn't code-signed. The source code is fully open.

---

## Windows — From source (advanced)

Use this if you want to build the firmware yourself from source.

### Prerequisites

1. **Python 3** — [python.org/downloads](https://python.org/downloads)
   - ⚠️ Check **"Add Python to PATH"** during install
2. **Git** — [git-scm.com](https://git-scm.com)
3. **PlatformIO** — open Command Prompt and run:
   ```
   pip install platformio
   ```
4. **CP210x driver** — see Step 2 above

### Flash

Open Command Prompt:

```
git clone https://github.com/ak1ra00/hw-flash
cd hw-flash\atlantis-deepsea
flash.bat COM3
```

Replace `COM3` with your actual port (check **Device Manager → Ports**).

---

## macOS

### Step 1 — Install prerequisites

Open **Terminal** (Spotlight → type "Terminal"):

```bash
# Install Homebrew if you don't have it
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install Python 3
brew install python

# Install PlatformIO
pip3 install platformio
```

### Step 2 — Install USB driver (older macOS only)

- **macOS 12 Monterey and newer** — driver is built-in, skip this step
- **macOS 11 and older:**
  1. Google **"CP210x Mac driver Silicon Labs"**
  2. Download and open the `.dmg`, run the installer
  3. Go to **System Settings → Privacy & Security** and allow it
  4. **Restart your Mac**

### Step 3 — Flash

Plug in your T-Display via USB-C, then:

```bash
git clone https://github.com/ak1ra00/hw-flash
cd hw-flash/atlantis-deepsea
chmod +x flash.sh
./flash.sh
```

The script auto-detects the port. If it fails, pass the port manually:

```bash
ls /dev/cu.*                        # find your port
./flash.sh /dev/cu.usbserial-0001   # use it
```

---

## What a successful flash looks like

```
[*] Found: Silicon Labs CP210x USB to UART Bridge  (COM3)

[*] Flashing Atlantis DeepSea to COM3 ...
    Please wait ~30 seconds.

esptool.py v4.x
Connecting........
Chip is ESP32-D0WDQ6 (revision v1.0)
Writing at 0x00010000... (25 %)
Writing at 0x00040000... (50 %)
Writing at 0x00070000... (75 %)
Writing at 0x000d0000... (100 %)
Hash of data verified.
Hard resetting via RTS pin...

  +--------------------------------------------+
  |  Flash complete!                             |
  |  Unplug and replug your T-Display.           |
  |  The Atlantis DeepSea setup screen starts.   |
  +--------------------------------------------+
```

---

## Troubleshooting

| Problem | Fix |
|---|---|
| Device not detected | Install CP210x driver, restart Windows, try again |
| `Failed to connect to ESP32` | Enter bootloader mode first — see Step 0 above |
| Nothing happens on double-click | Right-click → **Run as administrator** |
| SmartScreen blocks the .exe | Click **More info** → **Run anyway** |
| Wrong COM port | Open **Device Manager → Ports** and check the number |
| Charge-only cable | Use a different USB-C cable — must support data transfer |
| `Permission denied` on macOS | Run `sudo ./flash.sh` |
| Port busy / access denied | Close Arduino IDE, PuTTY, or any serial monitor |

---

## First boot walkthrough

After flashing, the T-Display restarts automatically:

```
1.  Atlantis DeepSea boot animation

2.  "Seed phrase length?"
    → BTN1 toggles between 12 and 24 words
    → BTN2 confirms

3.  Word entry  (one word at a time)
    → BTN1 cycles letters  a → b → c → ... → z → backspace
    → BTN2 adds the letter — matching words appear as you type
    → When only 1 match remains, BTN2 accepts it automatically
    → BTN2 long-press → switch to scrollable word list

4.  Verification — re-enter 3 random words to confirm your backup

5.  Set a 6-digit PIN
    → BTN1 changes the current digit  (0 → 1 → ... → 9 → 0)
    → BTN2 confirms the digit and advances
    → BTN1 long-press goes back one digit
    → Enter the PIN twice to confirm

6.  Done — vault is saved, device is ready
```

---

## Daily use

```
PIN entry  →  Main Menu
  ├── Get Password
  │     ├── Index   0–9999    (BTN1 +1,  long-press +10)
  │     └── Length  10–85     (BTN1 +1)
  │           → Password shown for 30 seconds, then auto-cleared
  ├── Settings
  ├── About
  └── Lock Device
```

The device **auto-locks** after 2 minutes of inactivity.

---

## Button reference

| Button | Press type | Action |
|---|---|---|
| BTN1 (top) | Short press | Increment / next letter / scroll |
| BTN1 (top) | Long press | Back / delete / cancel |
| BTN2 (bottom) | Short press | Confirm / select / advance |
| BTN2 (bottom) | Long press | Switch mode / force word list |
