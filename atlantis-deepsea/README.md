# Atlantis DeepSea

**BIP85 Password Manager for LilyGO T-Display**

Derive unlimited, deterministic passwords from a single BIP39 seed phrase — fully offline, on a $10 device.

```
    A T L A N T I S
      D E E P S E A
   BIP85 Password Manager
```

---

## What it does

- Stores one BIP39 seed phrase (12 or 24 words) encrypted on the device
- Derives unique, deterministic passwords via the [BIP85](https://github.com/bitcoin/bips/blob/master/bip-0085.mediawiki) standard
- Compatible with ColdCard BIP85 output — same seed = same passwords
- Protected by a 6-digit PIN with lockout after 5 wrong attempts
- Auto-locks after 2 minutes of inactivity
- Never connects to the internet — WiFi/BLE disabled at runtime

---

## Hardware

| Part | Details |
|---|---|
| Board | LilyGO T-Display (ESP32) |
| Display | 1.14" IPS TFT, 135×240 |
| Buttons | 2× programmable (GPIO 35, GPIO 0) |
| Storage | 4 MB flash (NVS encrypted) |
| Price | ~$10 USD |

---

## Security model

- Seed converted to BIP32 master key via `bip39_to_seed` (PBKDF2-SHA512, 2048 iterations)
- Master key encrypted with AES-256-CBC, key derived from PIN via PBKDF2-SHA256 (10,000 iterations)
- PIN verification uses a separate PBKDF2 hash — wrong PIN never attempts decryption
- After 5 wrong PINs: 30-second lockout
- After 2 minutes idle: device locks and clears key from RAM
- Flash encryption can be enabled via `idf.py efuse-burn` for physical tamper resistance

> **No secure element.** Keys live in ESP32 flash, encrypted. Physically determined attacker with soldering equipment and significant effort *may* be able to extract flash. Enable ESP32 flash encryption for stronger physical protection.

---

## Flashing — Quick Start

### Linux / macOS

```bash
git clone https://github.com/ak1ra00/hw-flash
cd hw-flash/atlantis-deepsea
chmod +x flash.sh
./flash.sh            # auto-detects port
# or
./flash.sh /dev/ttyUSB0
```

### Windows

```
flash.bat COM3
```

### Requirements

- [PlatformIO](https://platformio.org/install/cli) (`pip install platformio`) — installed automatically if missing
- USB-C cable connected to T-Display
- On Linux: add yourself to `dialout` group → `sudo usermod -aG dialout $USER`

---

## First boot walkthrough

```
1. Boot animation → Atlantis DeepSea splash screen

2. "Seed phrase length?" → BTN1 toggle, BTN2 confirm
   → Choose 12 words (standard) or 24 words (max security)

3. Word entry (N words):
   → BTN1 cycles letters a-z + backspace
   → BTN2 adds current letter to prefix
   → Matching words appear as you type
   → When only 1 match remains → BTN2 accepts it
   → BTN2 long-press → scroll through matching words

4. Verification: re-enter 3 random words to confirm backup

5. Set a 6-digit PIN:
   → BTN1 changes current digit (0-9)
   → BTN2 confirms digit → advance to next
   → BTN1 long-press → back one digit
   → Enter PIN twice to confirm

6. Vault saved. Device is ready.
```

---

## Daily use

```
PIN entry → Main Menu
  ├── Get Password
  │     ├── Index  (0-9999)   ← BTN1 +1,  long = +10
  │     └── Length (10-85)    ← BTN1 +1
  │           → Password displayed for 30 seconds, then auto-cleared
  ├── Settings
  ├── About
  └── Lock Device
```

---

## BIP85 compatibility

Passwords are derived identically to ColdCard's BIP85 implementation:

```
Path:    m/83696968' / 707764' / {length}' / {index}'
Entropy: HMAC-SHA512(key="bip-entropy-from-k", data=child_private_key)
Charset: 0-9, A-Z, a-z, !#$%&()*+-;<=>?@^_`{|}~  (85 chars)
```

The same seed phrase on a ColdCard with the same index and length produces the same password.

---

## Building from source

```bash
cd atlantis-deepsea
pio run             # compile only
pio run -t upload   # compile + flash
pio device monitor  # serial output (115200 baud)
```

---

## Resetting the device

Hold **BTN1** while powering on → the device will offer a factory reset option that wipes all NVS storage. Your passwords are always recoverable from your seed phrase.

---

## License

MIT — use freely, no warranty. Keep your seed phrase safe.
