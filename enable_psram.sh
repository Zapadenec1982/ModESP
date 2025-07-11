#!/bin/bash
# enable_psram.sh - Script to enable PSRAM support

echo "Enabling PSRAM support in sdkconfig..."

# Backup current sdkconfig
cp sdkconfig sdkconfig.backup

# Enable PSRAM
sed -i 's/# CONFIG_SPIRAM is not set/CONFIG_SPIRAM=y/' sdkconfig

# Add PSRAM configuration options
cat >> sdkconfig << 'EOF'

# PSRAM Configuration
CONFIG_SPIRAM_TYPE_AUTO=y
CONFIG_SPIRAM_SIZE=-1
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_MEMTEST=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768
EOF

echo "PSRAM enabled! Run 'idf.py menuconfig' to review settings."
echo "Or run 'idf.py fullclean && idf.py build' to rebuild with PSRAM support."