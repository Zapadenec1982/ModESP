# Base sensor drivers component providing interfaces and registry
idf_component_register(
    SRCS
        "src/sensor_driver_init.cpp"
        "ntc/src/ntc_driver.cpp"
        "ds18b20/src/ds18b20_driver.cpp"
    INCLUDE_DIRS 
        "include"
        "ntc/include"
        "ds18b20/include"
    REQUIRES 
        ESPhal      # For HAL interfaces
        json        # For nlohmann/json
    PRIV_REQUIRES
        esp_common
)