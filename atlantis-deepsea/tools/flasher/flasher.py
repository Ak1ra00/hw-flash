#!/usr/bin/env python3
"""
Atlantis DeepSea - Windows Firmware Flasher
Built into a single .exe by PyInstaller.
Firmware .bin files are embedded as bundled resources.
"""
import sys
import os
import hmac
import hashlib
import time


def resource(name):
    """Resolve path to a bundled file (works both frozen and in dev)."""
    if getattr(sys, 'frozen', False):
        return os.path.join(sys._MEIPASS, name)
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), name)


def banner():
    print()
    print("  +==============================================+")
    print("  |           A T L A N T I S                  |")
    print("  |             D E E P S E A                  |")
    print("  |        BIP85 Password Manager              |")
    print("  |         Firmware Flasher  v1.3             |")
    print("  +==============================================+")
    print()


def find_device():
    """Return (port, description) for the first likely ESP32 device found."""
    try:
        from serial.tools.list_ports import comports
        ports = list(comports())

        known_ids = [
            'CP210',
            '10C4:EA60',
            'CH340',
            'CH9102',
            '1A86:7523',
            '1A86:55D4',
            'FTDI',
            '0403:6001',
            'USB-SERIAL',
            'USB SERIAL',
        ]
        for p in ports:
            desc = (p.description or '').upper()
            hwid = (p.hwid or '').upper()
            for tag in known_ids:
                if tag in desc or tag in hwid:
                    return p.device, p.description

        for p in ports:
            hwid = (p.hwid or '')
            if hwid and hwid != 'n/a':
                return p.device, p.description

    except Exception:
        pass
    return None, None


def wait_and_exit(code=0):
    print()
    input("Press Enter to close...")
    sys.exit(code)


# ── Seed provisioning helpers ──────────────────────────────────────────────────

def collect_seed():
    """
    Ask the user for their BIP39 seed phrase.
    Returns (mnemonic_str, word_count) or None if skipped.
    """
    print()
    print("  +----------------------------------------------+")
    print("  |  PC Seed Provisioning (optional)            |")
    print("  |                                              |")
    print("  |  Type your seed words here and they will     |")
    print("  |  be written to the device automatically.     |")
    print("  |  The seed is stored permanently on device.   |")
    print("  |  Press Enter to skip and use device buttons. |")
    print("  +----------------------------------------------+")
    print()
    answer = input("  Provision seed from PC now? [y/N]: ").strip().lower()
    if answer != 'y':
        return None

    while True:
        wc_str = input("\n  Word count (12 or 24): ").strip()
        if wc_str in ('12', '24'):
            word_count = int(wc_str)
            break
        print("  Please enter 12 or 24.")

    print(f"\n  Enter your {word_count} seed words, one per line:")
    words = []
    for i in range(word_count):
        while True:
            word = input(f"  Word {i+1:>2}: ").strip().lower()
            if word:
                words.append(word)
                break

    print("\n  Words received. Deriving keys...")
    return ' '.join(words), word_count


def mnemonic_to_seed(mnemonic):
    """BIP39: mnemonic string -> 64-byte seed (PBKDF2-HMAC-SHA512)."""
    return hashlib.pbkdf2_hmac(
        'sha512',
        mnemonic.encode('utf-8'),
        b'mnemonic',
        2048,
    )


def bip32_master(seed):
    """BIP32: 64-byte seed -> (master_key 32 bytes, master_chain 32 bytes)."""
    I = hmac.new(b'Bitcoin seed', seed, hashlib.sha512).digest()
    return I[:32], I[32:]


def provision_device(port, master_key, master_chain, word_count):
    """
    Send BIP32 master keys to the device over serial.
    Device must be in provisioning mode (broadcasting ATLANTIS_READY).
    Returns True on success.
    """
    import serial as _serial

    print()
    print("[*] Waiting for device to boot (~5 s)...")
    time.sleep(5)

    try:
        ser = _serial.Serial(port, 115200, timeout=2)
    except Exception as e:
        print(f"[!] Cannot open serial port {port}: {e}")
        return False

    print("[*] Looking for provisioning mode (up to 40 s)...")
    start = time.time()
    ready = False
    while time.time() - start < 40:
        try:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
        except Exception:
            break
        if line == 'ATLANTIS_READY':
            ready = True
            break

    if not ready:
        ser.close()
        print("[!] Device did not enter provisioning mode.")
        print("    (Device may already have a seed stored.)")
        return False

    # Send: PROVISION:<64hex_key>:<64hex_chain>:<wc>
    packet = f"PROVISION:{master_key.hex()}:{master_chain.hex()}:{word_count}\n"
    ser.write(packet.encode('ascii'))
    ser.flush()

    # Wait for acknowledgement
    start = time.time()
    while time.time() - start < 10:
        try:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
        except Exception:
            break
        if line == 'PROVISION_OK':
            ser.close()
            return True
        if line == 'PROVISION_ERR':
            print("[!] Device reported a provisioning error.")
            ser.close()
            return False

    ser.close()
    print("[!] No response from device — timed out.")
    return False


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    banner()

    # ── Locate embedded firmware ───────────────────────────────────────────
    bins = {
        '0x1000':  resource('bootloader.bin'),
        '0x8000':  resource('partitions.bin'),
        '0xe000':  resource('boot_app0.bin'),
        '0x10000': resource('firmware.bin'),
    }

    missing = [name for name, path in bins.items() if not os.path.exists(path)]
    if missing:
        print(f"[ERR] Missing firmware files: {missing}")
        print("      Re-download the flasher from the Releases page.")
        wait_and_exit(1)

    # ── Find the T-Display ─────────────────────────────────────────────────
    print("[*] Looking for LilyGO T-Display (CP210x device)...")
    port, desc = find_device()

    if not port:
        print()
        print("[!] T-Display not found.")
        print()
        try:
            from serial.tools.list_ports import comports
            visible = list(comports())
            if visible:
                print("    Detected serial ports:")
                for p in visible:
                    print(f"      {p.device:8s}  {p.description}  [{p.hwid}]")
                print()
                print("    If your device is listed above, it may use a")
                print("    different USB chip. Please report this at:")
                print("    https://github.com/ak1ra00/hw-flash/issues")
            else:
                print("    No serial ports detected at all.")
        except Exception:
            pass
        print()
        print("    Checklist:")
        print("      1. Plug in your T-Display via USB-C")
        print("      2. Install the CP210x USB driver:")
        print("         https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers")
        print("      3. Restart Windows after installing the driver")
        print("      4. Run this flasher again")
        wait_and_exit(1)

    print(f"[*] Found: {desc}  ({port})")

    # ── Optionally collect seed before flashing ────────────────────────────
    seed_data = collect_seed()

    print()
    print(f"[*] Flashing Atlantis DeepSea to {port} ...")
    print("    Please wait ~30 seconds.")
    print()

    # ── Flash via esptool ──────────────────────────────────────────────────
    import esptool

    cmd = [
        '--chip',   'esp32',
        '--port',   port,
        '--baud',   '921600',
        '--before', 'default-reset',
        '--after',  'hard-reset',
        'write-flash',
        '--flash-mode', 'dio',
        '--flash-freq', '80m',
        '--flash-size', '4MB',
    ]
    for addr, path in bins.items():
        cmd += [addr, path]

    success = False
    try:
        esptool.main(cmd)
        success = True
    except SystemExit as e:
        success = (e.code == 0)
    except Exception as e:
        print(f"\n[ERR] Unexpected error: {e}")

    # ── Result ─────────────────────────────────────────────────────────────
    if success:
        prov_ok = False
        if seed_data:
            mnemonic, wc = seed_data
            seed_bytes   = mnemonic_to_seed(mnemonic)
            mk, mc       = bip32_master(seed_bytes)
            prov_ok      = provision_device(port, mk, mc, wc)
            # Overwrite sensitive values before GC
            del mnemonic, seed_bytes, mk, mc

        print()
        if prov_ok:
            print("  +----------------------------------------------+")
            print("  |  Flash + Provision complete!                 |")
            print("  |                                              |")
            print("  |  Seed saved to device permanently.           |")
            print("  |  Unplug and replug your T-Display.           |")
            print("  +----------------------------------------------+")
        else:
            print("  +----------------------------------------------+")
            print("  |  Flash complete!                             |")
            print("  |                                              |")
            print("  |  Unplug and replug your T-Display.           |")
            if seed_data:
                print("  |  Enter seed manually on device buttons.      |")
            else:
                print("  |  Your seed is preserved — no data wiped.     |")
            print("  +----------------------------------------------+")
        wait_and_exit(0)

    else:
        print()
        print("  [ERR] Flash failed.")
        print()
        print("  Try these steps and run the flasher again:")
        print("    1. Hold the BOOT button on the T-Display when you run this")
        print("    2. Use a different USB-C cable (must be a DATA cable)")
        print("    3. Close Arduino IDE or any serial monitor")
        wait_and_exit(1)


if __name__ == '__main__':
    main()
