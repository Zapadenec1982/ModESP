idf_component_register(
    SRCS "src/gpio_input_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES 
        sensor_drivers  # Base interfaces
        ESPhal         # HAL interfaces
        json           # nlohmann/json
    PRIV_REQUIRES
        esp_timer
)