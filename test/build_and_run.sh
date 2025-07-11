#!/bin/bash
# Script to build and run tests on ESP32-S3

echo "Building ModESP tests for ESP32-S3..."

# Set target to ESP32-S3
idf.py set-target esp32s3

# Clean previous build
idf.py fullclean

# Build the project with tests enabled
echo "Building with RUN_TESTS=1..."
idf.py build

# Flash to device if build successful
if [ $? -eq 0 ]; then
    echo "Build successful. Flashing to device..."
    idf.py -p COM3 flash monitor
else
    echo "Build failed!"
    exit 1
fi
