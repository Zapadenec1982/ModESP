idf_component_register(
    SRCS "src/ds18b20_async_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES sensor_drivers ESPhal esp_timer
)
