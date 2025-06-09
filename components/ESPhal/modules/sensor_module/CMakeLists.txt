idf_component_register(
    SRCS 
        "src/sensor_module.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES
        base_module
        ESPhal
        core
        nlohmann_json
        sensor_drivers      # Base sensor driver interfaces
    PRIV_REQUIRES
        esp_timer
        freertos
)

# Include specific sensor drivers based on configuration
# The drivers will be automatically linked if enabled in menuconfig
if(CONFIG_ENABLE_DS18B20_DRIVER)
    list(APPEND extra_component_dirs "${CMAKE_CURRENT_LIST_DIR}/../../sensor_drivers/ds18b20")
endif()

if(CONFIG_ENABLE_NTC_DRIVER)
    list(APPEND extra_component_dirs "${CMAKE_CURRENT_LIST_DIR}/../../sensor_drivers/ntc")
endif()

if(CONFIG_ENABLE_GPIO_INPUT_DRIVER)
    list(APPEND extra_component_dirs "${CMAKE_CURRENT_LIST_DIR}/../../sensor_drivers/gpio_input")
endif()