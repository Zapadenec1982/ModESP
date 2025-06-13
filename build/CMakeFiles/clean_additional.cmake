# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "ModESP.bin"
  "ModESP.map"
  "actuators.json.S"
  "alarms.json.S"
  "bootloader\\bootloader.bin"
  "bootloader\\bootloader.elf"
  "bootloader\\bootloader.map"
  "climate.json.S"
  "config\\sdkconfig.cmake"
  "config\\sdkconfig.h"
  "esp-idf\\esptool_py\\flasher_args.json.in"
  "esp-idf\\mbedtls\\x509_crt_bundle"
  "flash_app_args"
  "flash_bootloader_args"
  "flash_project_args"
  "flasher_args.json"
  "ldgen_libraries"
  "ldgen_libraries.in"
  "logging.json.S"
  "network.json.S"
  "project_elf_src_esp32s3.c"
  "rtc.json.S"
  "sensor.json.S"
  "sensors.json.S"
  "system.json.S"
  "ui.json.S"
  "wifi.json.S"
  "x509_crt_bundle.S"
  )
endif()
