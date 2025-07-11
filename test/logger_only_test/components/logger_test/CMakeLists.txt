idf_component_register(
    SRCS "test_logger_standalone.cpp"
    INCLUDE_DIRS "."
    REQUIRES 
        unity
        logger
        base_module
        core
        json
    PRIV_REQUIRES
        freertos
        esp_timer
) 