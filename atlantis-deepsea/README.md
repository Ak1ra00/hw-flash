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

## Flashing

> 📖 **Full step-by-step guide → [FLASHING.md](FLASHING.md)**
>
> Covers macOS and Windows, USB driver install, port detection, and troubleshooting.

**Windows (easiest):**

1. Download **[AtlantisDeepSea-Flasher.exe](https://github.com/ak1ra00/hw-flash/releases/latest)** — no installs needed
2. Plug in your T-Display via USB-C
3. Double-click the `.exe` — flashes automatically in ~30 seconds

**macOS / Linux:**

```bash
git clone https://github.com/ak1ra00/hw-flash
cd hw-flash/atlantis-deepsea
chmod +x flash.sh
./flash.sh
```

---

## Security model

- Seed converted to BIP32 master key via `bip39_to_seed` (PBKDF2-SHA512, 2048 iterations)
- Master key encrypted with AES-256-CBC, key derived from PIN via PBKDF2-SHA256 (10,000 iterations)
- PIN verification uses a separate PBKDF2 hash — wrong PIN never attempts decryption
- After 5 wrong PINs: 30-second lockout
- After 2 minutes idle: device locks and clears key from RAM
- Flash encryption can be enabled via `idf.py efuse-burn` for physical tamper resistance

> **No secure element.** Keys live in ESP32 flash, encrypted. A physically determined attacker with soldering equipment *may* be able to extract flash. Enable ESP32 flash encryption for stronger physical protection.

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

Passwords are derived identically to ColdCard’s BIP85 implementation:

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

Hold **BTN1** while powering on → factory reset option wipes all NVS storage. Your passwords are always recoverable from your seed phrase.

---

## License

MIT — use freely, no warranty. Keep your seed phrase safe.
