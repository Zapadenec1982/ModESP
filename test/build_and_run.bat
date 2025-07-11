@echo off
REM Script to build and run tests on ESP32-S3

echo Building ModESP tests for ESP32-S3...

REM Set target to ESP32-S3
call idf.py set-target esp32s3

REM Clean previous build
call idf.py fullclean

REM Build the project with tests enabled
echo Building with RUN_TESTS=1...
call idf.py build

REM Check if build was successful
if %ERRORLEVEL% EQU 0 (
    echo Build successful. Flashing to device...
    call idf.py -p COM3 flash monitor
) else (
    echo Build failed!
    exit /b 1
)
