# Flashing Guide — Atlantis DeepSea

Step-by-step instructions for flashing the firmware onto your LilyGO T-Display on **macOS** and **Windows**.

---

## macOS

### Step 1 — Install prerequisites

Open **Terminal** (Spotlight → type "Terminal")

```bash
# Install Homebrew if you don't have it
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install Python 3
brew install python

# Install PlatformIO
pip3 install platformio
```

### Step 2 — Install USB driver

The T-Display uses a **CP2102** USB-to-Serial chip.

- **macOS 12 Monterey and newer** — driver is built-in, skip this step
- **macOS 11 and older** — download the CP210x driver from Silicon Labs:
  1. Google **"CP210x Mac driver Silicon Labs"**
  2. Download and open the `.dmg`
  3. Run the installer
  4. Go to **System Settings → Privacy & Security** and allow the driver if prompted
  5. **Restart your Mac**

### Step 3 — Clone the repo

```bash
git clone https://github.com/ak1ra00/hw-flash
cd hw-flash/atlantis-deepsea
```

### Step 4 — Plug in your T-Display

Connect your LilyGO T-Display via **USB-C**.

### Step 5 — Flash

```bash
chmod +x flash.sh
./flash.sh
```

The script auto-detects the port. You'll see:

```
[*] Auto-detected port: /dev/cu.usbserial-0001
[*] Building and flashing to /dev/cu.usbserial-0001 ...
```

If auto-detection fails, find the port manually and pass it:

```bash
# List available ports
ls /dev/cu.*

# Flash to a specific port
./flash.sh /dev/cu.usbserial-0001
```

---

## Windows

### Step 1 — Install Python 3

1. Go to **[python.org/downloads](https://python.org/downloads)**
2. Download the latest installer
3. Run it — ⚠️ **check "Add Python to PATH"** at the bottom of the first screen
4. Click **Install Now**

### Step 2 — Install Git

1. Go to **[git-scm.com](https://git-scm.com)**
2. Download and run the installer (all defaults are fine)

### Step 3 — Install PlatformIO

Open **Command Prompt** (Start → type `cmd` → Enter):

```
pip install platformio
```

### Step 4 — Install USB driver

1. Google **"CP210x Windows driver Silicon Labs"**
2. Download `CP210x_Windows_Drivers.zip`
3. Extract it
4. Run **`CP210xVCPInstaller_x64.exe`** (or `x86` for 32-bit Windows)
5. Plug in your T-Display via **USB-C**

### Step 5 — Find your COM port

1. Open **Device Manager** (Start → type `Device Manager`)
2. Expand **Ports (COM & LPT)**
3. Look for **Silicon Labs CP210x USB to UART Bridge (COM?)**
4. Note the COM number — e.g. **COM3**

> If you don't see it, the driver didn't install correctly — retry Step 4.

### Step 6 — Clone the repo

In Command Prompt:

```
git clone https://github.com/ak1ra00/hw-flash
cd hw-flash\atlantis-deepsea
```

### Step 7 — Flash

```
flash.bat COM3
```

Replace `COM3` with your actual port number from Step 5.

---

## What a successful flash looks like

```
Linking .pio/build/lilygo-t-display/firmware.elf
Building .pio/build/lilygo-t-display/firmware.bin
esptool.py v4.x  Serial port COM3
Connecting........
Chip is ESP32-D0WDQ6 (revision v1.0)
Uploading stub...
Writing at 0x00010000... (12 %)
Writing at 0x00014000... (25 %)
Writing at 0x00018000... (37 %)
...
Writing at 0x000d0000... (100 %)
Hash of data verified.
Hard resetting via RTS pin...

[✓] Flash complete!
```

> **First build takes 2–4 minutes** — PlatformIO downloads the ESP32 toolchain and TFT_eSPI library automatically. Subsequent flashes are ~30 seconds.

---

## Troubleshooting

| Problem | Fix |
|---|---|
| `Port not found` | Check Device Manager (Windows) or `ls /dev/cu.*` (macOS) |
| `Permission denied` on macOS | Run `sudo ./flash.sh` |
| `pip not found` on Windows | Reinstall Python and check **Add to PATH** |
| `Failed to connect to ESP32` | Hold the **BOOT** button on the board while the script says `Connecting...` |
| Board not detected at all | Try a different USB-C cable — some cables are charge-only with no data lines |
| Driver not showing in Device Manager | Try a different USB port on your PC, then reinstall the CP210x driver |
| `invalid header` or garbled output | Wrong baud rate — monitor at **115200** baud |

---

## First boot walkthrough

After a successful flash, the T-Display restarts automatically:

```
1.  Atlantis DeepSea boot animation

2.  "Seed phrase length?"
    → BTN1 toggles between 12 and 24 words
    → BTN2 confirms

3.  Word entry  (repeated N times)
    → BTN1 cycles letters  a → b → c → ... → z → backspace
    → BTN2 adds the current letter to your typed prefix
    → Matching words appear as you type
    → When only 1 match remains, BTN2 accepts it automatically
    → BTN2 long-press switches to scrollable word list

4.  Verification — re-enter 3 random words to confirm your backup

5.  Set your 6-digit PIN
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

| Button | Press type | Action (context-dependent) |
|---|---|---|
| BTN1 (top) | Short press | Increment / next option / next letter |
| BTN1 (top) | Long press | Back / delete / cancel |
| BTN2 (bottom) | Short press | Confirm / select / advance |
| BTN2 (bottom) | Long press | Context action (switch mode, force pick) |
