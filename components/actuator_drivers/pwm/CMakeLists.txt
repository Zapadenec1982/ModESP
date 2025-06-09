idf_component_register(
    SRCS "src/pwm_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES 
        actuator_drivers  # Base interfaces
        json             # nlohmann/json
        driver           # For LEDC driver
    PRIV_REQUIRES
        esp_timer
)