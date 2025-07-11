# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "actuators.json.S"
  "alarms.json.S"
  "climate.json.S"
  "esp-idf\\esptool_py\\flasher_args.json.in"
  "esp-idf\\mbedtls\\x509_crt_bundle"
  "flash_app_args"
  "flash_bootloader_args"
  "flasher_args.json"
  "integration_test.map"
  "ldgen_libraries"
  "ldgen_libraries.in"
  "littlefs_py_venv"
  "logging.json.S"
  "network.json.S"
  "project_elf_src_esp32s3.c"
  "rtc.json.S"
  "sensors.json.S"
  "system.json.S"
  "ui.json.S"
  "wifi.json.S"
  "x509_crt_bundle.S"
  )
endif()
