# Base sensor drivers component providing interfaces and registry
set(SRCS
    "src/sensor_driver_init.cpp"
)

set(INCLUDE_DIRS
    "include"
)

# Conditionally add driver sources based on Kconfig
if(CONFIG_SENSOR_DRIVER_NTC_ENABLED)
    list(APPEND SRCS "ntc/src/ntc_driver.cpp")
    list(APPEND INCLUDE_DIRS "ntc/include")
endif()

if(CONFIG_SENSOR_DRIVER_DS18B20_ASYNC_ENABLED)
    list(APPEND SRCS "ds18b20_async/src/ds18b20_async_driver.cpp")
    list(APPEND INCLUDE_DIRS "ds18b20_async/include")
endif()

# Add other drivers here as they are implemented
# if(CONFIG_SENSOR_DRIVER_PRESSURE_4_20MA_ENABLED)
#     list(APPEND SRCS "pressure_4_20ma/src/pressure_4_20ma_driver.cpp")
#     list(APPEND INCLUDE_DIRS "pressure_4_20ma/include")
# endif()

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    REQUIRES 
        json        # For nlohmann/json
        ESPhal      # For HAL interfaces
    PRIV_REQUIRES
        esp_common
        esp_timer
)
