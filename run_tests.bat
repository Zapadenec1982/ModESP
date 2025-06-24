@echo off
REM Build and run host tests on Windows

echo Building ModESP host tests...

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake
cmake ..\test\host -G "MinGW Makefiles"
if %errorlevel% neq 0 (
    echo CMake configuration failed
    exit /b 1
)

REM Build
mingw32-make -j4
if %errorlevel% neq 0 (
    echo Build failed
    exit /b 1
)

echo Build successful!
echo Running tests...

REM Run tests
ctest --verbose
if %errorlevel% equ 0 (
    echo All tests passed!
) else (
    echo Some tests failed
    exit /b 1
)