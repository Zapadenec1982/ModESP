@echo off
REM ModESP Flash & Monitor Script

echo ========================================
echo     ModESP Flash and Monitor
echo ========================================
echo.

REM Default COM port - change this to your port
set COM_PORT=COM3

REM Ask user for COM port
echo Current port: %COM_PORT%
echo.
set /p USER_PORT="Enter COM port (or press Enter for %COM_PORT%): "
if not "%USER_PORT%"=="" set COM_PORT=%USER_PORT%

echo.
echo Using port: %COM_PORT%
echo.

REM Navigate to project
cd /d C:\ModESP_dev

REM Flash and monitor
echo Flashing to ESP32 and starting monitor...
echo (Press Ctrl+] to exit monitor)
echo.

call idf.py -p %COM_PORT% flash monitor

pause
