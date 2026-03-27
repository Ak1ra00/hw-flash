#!/usr/bin/env python3
"""
Atlantis DeepSea - Windows Firmware Flasher
Built into a single .exe by PyInstaller.
Firmware .bin files are embedded as bundled resources.
"""
import sys
import os


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
    print("  |         Firmware Flasher  v1.0             |")
    print("  +==============================================+")
    print()


def find_device():
    """Return (port, description) for the first CP210x device found."""
    try:
        from serial.tools.list_ports import comports
        for p in comports():
            desc = (p.description or '').upper()
            hwid = (p.hwid or '').upper()
            if 'CP210' in desc or '10C4:EA60' in hwid:
                return p.device, p.description
    except Exception:
        pass
    return None, None


def wait_and_exit(code=0):
    print()
    input("Press Enter to close...")
    sys.exit(code)


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
        print("    Checklist:")
        print("      1. Plug in your T-Display via USB-C")
        print("      2. Install the CP210x USB driver:")
        print("         https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers")
        print("      3. Restart Windows after installing the driver")
        print("      4. Run this flasher again")
        wait_and_exit(1)

    print(f"[*] Found: {desc}  ({port})")
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
        '--before', 'default_reset',
        '--after',  'hard_reset',
        'write_flash',
        '--flash_mode', 'dio',
        '--flash_freq', '80m',
        '--flash_size', '4MB',
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
        print()
        print("  +----------------------------------------------+")
        print("  |  Flash complete!                             |")
        print("  |                                              |")
        print("  |  Unplug and replug your T-Display.           |")
        print("  |  The Atlantis DeepSea setup screen starts.   |")
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
