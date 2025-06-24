#!/bin/bash
# Build and run host tests

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo "Building ModESP host tests..."

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ../test/host
if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed${NC}"
    exit 1
fi

# Build
make -j4
if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed${NC}"
    exit 1
fi

echo -e "${GREEN}Build successful!${NC}"
echo "Running tests..."

# Run tests
ctest --verbose
if [ $? -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
else
    echo -e "${RED}Some tests failed${NC}"
    exit 1
fi