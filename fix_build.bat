@echo off
REM Fix ModESP Build Issues

echo ========================================
echo     Fixing ModESP Build Issues
echo ========================================
echo.

cd /d C:\ModESP_dev

echo Setting target to ESP32 (not S3)...
call idf.py set-target esp32

echo.
echo Cleaning build directory...
rmdir /s /q build 2>nul

echo.
echo Creating stub components...
mkdir components\lcd_ui 2>nul
mkdir components\mqtt_ui 2>nul

REM Create minimal CMakeLists for lcd_ui
echo idf_component_register(> components\lcd_ui\CMakeLists.txt
echo     # Empty component for now>> components\lcd_ui\CMakeLists.txt
echo )>> components\lcd_ui\CMakeLists.txt

REM Create minimal CMakeLists for mqtt_ui  
echo idf_component_register(> components\mqtt_ui\CMakeLists.txt
echo     # Empty component for now>> components\mqtt_ui\CMakeLists.txt
echo )>> components\mqtt_ui\CMakeLists.txt

echo.
echo Fixed! Now try building again:
echo   idf.py build
echo.
pause
