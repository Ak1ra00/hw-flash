@echo off
setlocal enabledelayedexpansion
title Atlantis DeepSea - Firmware Flasher

echo.
echo  +==============================================+
echo  ^|          A T L A N T I S                    ^|
echo  ^|            D E E P S E A                    ^|
echo  ^|       BIP85 Password Manager                ^|
echo  ^|         Windows Firmware Flasher            ^|
echo  +==============================================+
echo.

:: ── Use port passed as argument if given ────────────────────────────────────
if not "%1"=="" (
    set PORT=%1
    echo [*] Using port: !PORT!
    goto :flash
)

:: ── Auto-detect CP210x via PowerShell WMI ───────────────────────────────────
echo [*] Searching for T-Display (CP210x USB device)...
set PORT=
for /f "usebackq delims=" %%P in (
    `powershell -NoProfile -Command "try { $d = Get-WmiObject Win32_PnPEntity | Where-Object {$_.Name -match 'CP210'}; if ($d) { if ($d -is [array]) { $d = $d[0] }; if ($d.Name -match 'COM(\d+)') { 'COM' + $Matches[1] } } } catch { }"`
) do (
    set PORT=%%P
)

if not "!PORT!"=="" (
    echo [*] Found T-Display on !PORT!
    goto :flash
)

:: ── Not found — show available ports and ask ─────────────────────────────────
echo.
echo [!] T-Display not detected automatically.
echo.
echo     Checklist:
echo       1. Is the T-Display plugged in via USB-C?
echo       2. Is the CP210x driver installed?
echo          If not: see README.txt for the download link.
echo.
echo     COM ports currently visible on this PC:
powershell -NoProfile -Command "Get-WmiObject Win32_PnPEntity | Where-Object {$_.Name -match 'COM\d'} | Sort-Object Name | ForEach-Object { '      ' + $_.Name }"
echo.
set /p PORT="Enter COM port (e.g. COM3): "
if "!PORT!"=="" (
    echo [ERR] No port entered. Exiting.
    pause
    exit /b 1
)

:flash
echo.
echo [*] Flashing Atlantis DeepSea firmware to !PORT! ...
echo     Please wait — takes about 30 seconds.
echo.

esptool.exe ^
    --chip esp32 ^
    --port !PORT! ^
    --baud 921600 ^
    --before default_reset ^
    --after hard_reset ^
    write_flash ^
    --flash_mode dio ^
    --flash_freq 80m ^
    --flash_size 4MB ^
    0x1000  bootloader.bin ^
    0x8000  partitions.bin ^
    0xe000  boot_app0.bin ^
    0x10000 firmware.bin

if %errorlevel% equ 0 (
    echo.
    echo  +--------------------------------------------+
    echo  ^|  [OK]  Flash complete!                     ^|
    echo  ^|                                            ^|
    echo  ^|  Unplug and replug your T-Display.         ^|
    echo  ^|  Atlantis DeepSea setup screen will start. ^|
    echo  +--------------------------------------------+
) else (
    echo.
    echo  [ERR] Flash failed.
    echo.
    echo  Common fixes:
    echo    1. Hold the BOOT button on the T-Display, then re-run this script
    echo    2. Try a different USB-C cable (must be a DATA cable, not charge-only)
    echo    3. Close Arduino IDE or any serial monitor app
    echo    4. Run flash.bat COM3  (replace COM3 with your actual port)
)

echo.
pause
