@echo off
echo Starting Logger Module Tests...

REM Initialize ESP-IDF environment
call %IDF_PATH%\export.bat

REM Set target
idf.py set-target esp32s3

REM Build
echo Building logger tests...
idf.py build

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

REM Flash and monitor
echo Flashing and monitoring...
idf.py -p COM11 flash monitor

pause 