cd \cd @echo off
REM ModESP Build Script for Windows
REM This script builds the ModESP project using ESP-IDF

echo ========================================
echo        ModESP Build Script
echo        Phase 5 - Adaptive UI
echo ========================================
echo.

REM Check if we're in ESP-IDF environment
where idf.py >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: idf.py not found!
    echo Please run this script from ESP-IDF Command Prompt
    echo.
    echo Steps:
    echo 1. Open "ESP-IDF 5.x CMD" from Start Menu
    echo 2. Navigate to C:\ModESP_dev
    echo 3. Run build.bat again
    echo.
    pause
    exit /b 1
)

REM Navigate to project directory
cd C:\ModESP_dev
echo Current directory: %CD%
echo.

REM Generate code from manifests
echo Generating code from manifests...
python tools\process_manifests.py --project-root . --output-dir main\generated
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to generate code from manifests!
    echo Make sure Python is installed and available.
    pause
    exit /b 1
)
echo Code generation completed successfully.
echo.

REM Clean previous build (optional - comment out for faster rebuilds)
echo Cleaning previous build...
call idf.py fullclean
echo.

REM Set target to ESP32
echo Setting target to ESP32...
call idf.py set-target esp32
echo.

REM Build the project
echo Building ModESP...
echo This may take several minutes on first build...
echo.
call idf.py build

REM Check if build was successful
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo    BUILD SUCCESSFUL!
    echo ========================================
    echo.
    echo Next steps:
    echo 1. Connect your ESP32 via USB
    echo 2. Check COM port in Device Manager
    echo 3. Run: idf.py -p COM3 flash monitor
    echo    (replace COM3 with your port)
    echo.
) else (
    echo.
    echo ========================================
    echo    BUILD FAILED!
    echo ========================================
    echo.
    echo Check the error messages above.
    echo Common issues:
    echo - Missing dependencies
    echo - Syntax errors in code
    echo - Include path problems
    echo.
)

pause
