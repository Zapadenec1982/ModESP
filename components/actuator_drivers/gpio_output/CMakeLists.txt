idf_component_register(
    SRCS "src/gpio_output_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES 
        actuator_drivers  # Base interfaces
        ESPhal           # HAL interfaces
        json             # nlohmann/json
    PRIV_REQUIRES
        esp_timer
)
