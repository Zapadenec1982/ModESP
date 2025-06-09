idf_component_register(
    SRCS "src/pressure_4_20ma_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES 
        sensor_drivers  # Base interfaces
        ESPhal         # HAL interfaces
        json           # nlohmann/json
    PRIV_REQUIRES
        esp_timer
)