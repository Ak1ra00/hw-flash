#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
#  Atlantis DeepSea — one-click flash script
#  Usage:  ./flash.sh [PORT]
#  Example: ./flash.sh /dev/ttyUSB0
# ─────────────────────────────────────────────────────────────────────────────
set -e

PORT="${1:-}"

print_banner() {
cat << 'EOF'

    ╔═══════════════════════════════════════╗
    ║        A T L A N T I S                ║
    ║          D E E P S E A                ║
    ║     BIP85 Password Manager            ║
    ╚═══════════════════════════════════════╝

EOF
}

detect_port() {
    for p in /dev/ttyUSB0 /dev/ttyUSB1 /dev/ttyACM0 /dev/ttyACM1; do
        [ -e "$p" ] && echo "$p" && return
    done
    # macOS
    for p in /dev/cu.usbserial-* /dev/cu.SLAB_USBtoUART*; do
        [ -e "$p" ] && echo "$p" && return
    done
    echo ""
}

install_pio() {
    echo "[*] Installing PlatformIO..."
    pip3 install --quiet platformio
}

print_banner

# Ensure PlatformIO is available
if ! command -v pio &>/dev/null && ! command -v platformio &>/dev/null; then
    install_pio
fi
PIO=$(command -v pio || command -v platformio)

# Auto-detect port if not provided
if [ -z "$PORT" ]; then
    PORT=$(detect_port)
    if [ -z "$PORT" ]; then
        echo "[!] Could not auto-detect serial port."
        echo "    Plug in your T-Display via USB-C and try:"
        echo "    ./flash.sh /dev/ttyUSB0   (Linux)"
        echo "    ./flash.sh /dev/cu.usbserial-XXXX  (macOS)"
        exit 1
    fi
    echo "[*] Auto-detected port: $PORT"
fi

cd "$(dirname "$0")"

echo "[*] Installing / updating dependencies..."
$PIO pkg install

echo "[*] Configuring TFT_eSPI display library..."
TFT_DIR=".pio/libdeps/lilygo-t-display/TFT_eSPI"
if [ -d "$TFT_DIR" ]; then
    cp -f include/User_Setup.h "$TFT_DIR/User_Setup.h"
    echo "    User_Setup.h applied."
else
    echo "    Warning: TFT_eSPI not found, skipping User_Setup.h copy."
fi

echo "[*] Building and flashing to $PORT ..."
$PIO run -t upload --upload-port "$PORT"

echo ""
echo "[✓] Flash complete! Open serial monitor (115200 baud) to see logs."
echo "    $PIO device monitor --port $PORT --baud 115200"
