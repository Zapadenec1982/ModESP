idf_component_register(
    SRCS 
        "src/esphal.cpp"
        "src/gpio_output_impl.cpp"
        "src/gpio_input_impl.cpp"
        "src/onewire_bus_impl.cpp"
        "src/adc_channel_impl.cpp"
    INCLUDE_DIRS 
        "include"
        "boards"
    PRIV_REQUIRES 
        driver
        esp_adc
        esp_timer
    REQUIRES
        nlohmann_json
)

# Додати залежності для драйверів ESP-IDF
target_link_libraries(${COMPONENT_LIB} INTERFACE
    idf::driver
    idf::esp_adc
    idf::esp_timer
)
