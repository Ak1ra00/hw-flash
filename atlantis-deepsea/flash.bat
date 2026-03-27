@echo off
REM ──────────────────────────────────────────────────────────────────────────
REM  Atlantis DeepSea — one-click flash script (Windows)
REM  Usage:  flash.bat [COM_PORT]
REM  Example: flash.bat COM3
REM ──────────────────────────────────────────────────────────────────────────

SET PORT=%1

echo.
echo     +---------------------------------------+
echo     ^|        A T L A N T I S                ^|
echo     ^|          D E E P S E A                ^|
echo     ^|     BIP85 Password Manager            ^|
echo     +---------------------------------------+
echo.

WHERE pio >nul 2>&1
IF %ERRORLEVEL% NEQ 0 (
    WHERE platformio >nul 2>&1
    IF %ERRORLEVEL% NEQ 0 (
        echo [*] Installing PlatformIO...
        pip install platformio
    )
)

IF "%PORT%"=="" (
    echo [!] No COM port specified.
    echo     Usage: flash.bat COM3
    echo     Check Device Manager for the correct COM port.
    pause
    exit /b 1
)

echo [*] Flashing to %PORT% ...
cd /d "%~dp0"
pio run -t upload --upload-port %PORT%

IF %ERRORLEVEL% EQU 0 (
    echo.
    echo [OK] Flash complete!
    echo      Open serial monitor: pio device monitor --port %PORT% --baud 115200
) ELSE (
    echo.
    echo [ERR] Flash failed. Check USB connection and COM port.
)
pause
