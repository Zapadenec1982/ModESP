idf_component_register(
    SRCS 
        "src/esphal.cpp"
        "modules/rtc_module/rtc_module.cpp"
        "modules/sensor_module/src/sensor_module.cpp"
        # Other modules temporarily disabled until fixed:
        # "modules/actuator_module/src/actuator_module.cpp"
    INCLUDE_DIRS 
        "include"
        "boards"
        "modules/rtc_module"
        "modules/sensor_module/include"
        # Other module includes temporarily disabled:
        # "modules/actuator_module/include"
    PRIV_REQUIRES 
        driver
        esp_adc
        esp_timer
        freertos
        sensor_drivers  # Added here for linking
    REQUIRES
        json
        base_module
        core
        sensor_drivers
        # actuator_drivers
)
